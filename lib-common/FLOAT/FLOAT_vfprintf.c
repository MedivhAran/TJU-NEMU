#include <stdio.h>
#include <stdint.h>
#include "FLOAT.h"
#include <string.h>

#ifdef LINUX_RT
#include <sys/mman.h>
#include <unistd.h>
#endif

extern char _vfprintf_internal;
extern char _fpmaxtostr;
extern char _ppfs_setargs;
extern int __stdio_fwrite(char *buf, int len, FILE *stream);

__attribute__((used)) static int format_FLOAT(FILE *stream, FLOAT f) {
	/* Format a 16.16 fixed-point FLOAT to decimal with 6-digit fraction (truncate). */
	char buf[64];

	int neg = (f < 0);
	uint32_t uf = neg ? (uint32_t)(-f) : (uint32_t)f;
	uint32_t ip = uf >> 16;                // integer part
	uint32_t frac = uf & 0xFFFF;           // fractional 16-bit

	/* Scale fractional part to 6 decimal digits: floor(frac * 1e6 / 2^16) */
	uint32_t dec = (uint32_t)(((uint64_t)frac * 1000000ULL) >> 16);

	if (neg) {
		int len = sprintf(buf, "-%u.%06u", ip, dec);
		return __stdio_fwrite(buf, len, stream);
	} else {
		int len = sprintf(buf, "%u.%06u", ip, dec);
		return __stdio_fwrite(buf, len, stream);
	}
}

static void modify_vfprintf() {
	/* Redirect the call to _fpmaxtostr inside _vfprintf_internal to format_FLOAT().
	 * We scan for a CALL rel32 whose resolved target is &_fpmaxtostr and patch
	 * its rel32 to point to format_FLOAT. Under Linux runtime, make code pages
	 * writable/executable temporarily with mprotect; under NEMU we directly write.
	 */

#if 0
	else if (ppfs->conv_num <= CONV_A) {  /* floating point */
		ssize_t nf;
		nf = _fpmaxtostr(stream,
				(__fpmax_t)
				(PRINT_INFO_FLAG_VAL(&(ppfs->info),is_long_double)
				 ? *(long double *) *argptr
				 : (long double) (* (double *) *argptr)),
				&ppfs->info, FP_OUT );
		if (nf < 0) {
			return -1;
		}
		*count += nf;

		return 0;
	} else if (ppfs->conv_num <= CONV_S) {  /* wide char or string */
#endif

	/* Runtime patching */
	uint8_t *base = (uint8_t *)&_vfprintf_internal;
	const uint8_t *target = (const uint8_t *)&_fpmaxtostr;
	int patched = 0;

	/* Search within a reasonable window */
	const size_t scan_len = 8192; /* big enough for this function body */

#ifdef LINUX_RT
	long pagesz = sysconf(_SC_PAGESIZE);
	if (pagesz <= 0) pagesz = 4096;
#endif
	size_t i = 0;
	for (; i + 5 <= scan_len; i++) {
		if (base[i] != 0xE8) continue; // call rel32
		int32_t rel = *(int32_t *)(base + i + 1);
		uint8_t *resolved = (base + i + 5) + rel;
		if ((void *)resolved == (void *)target) {
			/* Patch this call to format_FLOAT */
			uint8_t *call_site = base + i;
			uint8_t *next_ip = call_site + 5;
			int32_t new_rel = (int32_t)((uint8_t *)&format_FLOAT - next_ip);

#ifdef LINUX_RT
			uintptr_t page = (uintptr_t)call_site & ~(uintptr_t)(pagesz - 1);
			mprotect((void *)page, pagesz, PROT_READ | PROT_WRITE | PROT_EXEC);
#endif
			*(int32_t *)(call_site + 1) = new_rel;
#ifdef LINUX_RT
			mprotect((void *)page, pagesz, PROT_READ | PROT_EXEC);
#endif
			patched = 1;
			break;
		}
	}

	(void)patched; /* suppress unused warning if not checked */

#if 0
	else if (ppfs->conv_num <= CONV_A) {  /* floating point */
		ssize_t nf;
		nf = format_FLOAT(stream, *(FLOAT *) *argptr);
		if (nf < 0) {
			return -1;
		}
		*count += nf;

		return 0;
	} else if (ppfs->conv_num <= CONV_S) {  /* wide char or string */
#endif

}

static void modify_ppfs_setargs() {
	/* Patch _ppfs_setargs to avoid FPU when handling PA_DOUBLE:
	 * locate the sequence "dd 02 89 58 4c dd 19 eb" (fldl/mov/fstpl/jmp)
	 * and overwrite its first 5 bytes with a near jmp to the 64-bit
	 * integer handler sequence "8B 3A 8B 6A 04 8D 5A 08 89 58 4C 89 39 89 69 04".
	 */

#if 0
	enum {                          /* C type: */
		PA_INT,                       /* int */
		PA_CHAR,                      /* int, cast to char */
		PA_WCHAR,                     /* wide char */
		PA_STRING,                    /* const char *, a '\0'-terminated string */
		PA_WSTRING,                   /* const wchar_t *, wide character string */
		PA_POINTER,                   /* void * */
		PA_FLOAT,                     /* float */
		PA_DOUBLE,                    /* double */
		__PA_NOARG,                   /* non-glibc -- signals non-arg width or prec */
		PA_LAST
	};

	/* Flag bits that can be set in a type returned by `parse_printf_format'.  */
	/* WARNING -- These differ in value from what glibc uses. */
#define PA_FLAG_MASK		(0xff00)
#define __PA_FLAG_CHAR		(0x0100) /* non-gnu -- to deal with hh */
#define PA_FLAG_SHORT		(0x0200)
#define PA_FLAG_LONG		(0x0400)
#define PA_FLAG_LONG_LONG	(0x0800)
#define PA_FLAG_LONG_DOUBLE	PA_FLAG_LONG_LONG
#define PA_FLAG_PTR		(0x1000) /* TODO -- make dynamic??? */

	while (i < ppfs->num_data_args) {
		switch(ppfs->argtype[i++]) {
			case (PA_INT|PA_FLAG_LONG_LONG):
				GET_VA_ARG(p,ull,unsigned long long,ppfs->arg);
				break;
			case (PA_INT|PA_FLAG_LONG):
				GET_VA_ARG(p,ul,unsigned long,ppfs->arg);
				break;
			case PA_CHAR:	/* TODO - be careful */
				/* ... users could use above and really want below!! */
			case (PA_INT|__PA_FLAG_CHAR):/* TODO -- translate this!!! */
			case (PA_INT|PA_FLAG_SHORT):
			case PA_INT:
				GET_VA_ARG(p,u,unsigned int,ppfs->arg);
				break;
			case PA_WCHAR:	/* TODO -- assume int? */
				/* we're assuming wchar_t is at least an int */
				GET_VA_ARG(p,wc,wchar_t,ppfs->arg);
				break;
				/* PA_FLOAT */
			case PA_DOUBLE:
				GET_VA_ARG(p,d,double,ppfs->arg);
				break;
			case (PA_DOUBLE|PA_FLAG_LONG_DOUBLE):
				GET_VA_ARG(p,ld,long double,ppfs->arg);
				break;
			default:
				/* TODO -- really need to ensure this can't happen */
				assert(ppfs->argtype[i-1] & PA_FLAG_PTR);
			case PA_POINTER:
			case PA_STRING:
			case PA_WSTRING:
				GET_VA_ARG(p,p,void *,ppfs->arg);
				break;
			case __PA_NOARG:
				continue;
		}
		++p;
	}
#endif

	/* You should modify the run-time binary to let the `PA_DOUBLE'
	 * branch execute the code in the `(PA_INT|PA_FLAG_LONG_LONG)'
	 * branch. Comparing to the original `PA_DOUBLE' branch, the
	 * target branch will also prepare a 64-bit argument, without
	 * introducing floating point instructions. When this function
	 * returns, the action of the code above should do the following:
	 */

#if 0
	while (i < ppfs->num_data_args) {
		switch(ppfs->argtype[i++]) {
			case (PA_INT|PA_FLAG_LONG_LONG):
			here:
				GET_VA_ARG(p,ull,unsigned long long,ppfs->arg);
				break;
			// ......
				/* PA_FLOAT */
			case PA_DOUBLE:
				goto here;
				GET_VA_ARG(p,d,double,ppfs->arg);
				break;
			// ......
		}
		++p;
	}
#endif

	uint8_t *base = (uint8_t *)&_ppfs_setargs;
	const size_t scan_len = 8192;

	const uint8_t pat_double[]     = {0xDD,0x02,0x89,0x58,0x4C,0xDD,0x19,0xEB};
	const uint8_t pat_double_pref[] = {0xDD,0x02}; /* 通用 FLD m64fp 前缀 */
	const uint8_t pat_ll[]         = {0x8B,0x3A,0x8B,0x6A,0x04,0x8D,0x5A,0x08,0x89,0x58,0x4C,0x89,0x39,0x89,0x69,0x04};
	const uint8_t pat_ll_pref[]    = {0x8B,0x3A,0x8B,0x6A,0x04};

	int32_t idx_double = -1, idx_ll = -1;
	size_t i;
	for (i = 0; i + sizeof(pat_double) <= scan_len; i++) {
		if (memcmp(base + i, pat_double, sizeof(pat_double)) == 0) {
			idx_double = (int32_t)i;
			break;
		}
	}
	/* 若找不到完整 double 模式，退化为寻找通用 FPU 指令前缀 dd 02 */
	if (idx_double < 0) {
		for (i = 0; i + sizeof(pat_double_pref) <= scan_len; i++) {
			if (memcmp(base + i, pat_double_pref, sizeof(pat_double_pref)) == 0) {
				idx_double = (int32_t)i;
				break;
			}
		}
	}
	for (i = 0; i + sizeof(pat_ll) <= scan_len; i++) {
		if (memcmp(base + i, pat_ll, sizeof(pat_ll)) == 0) {
			idx_ll = (int32_t)i;
			break;
		}
	}
	/* 若找不到完整 LL 模式，尝试匹配其前缀 */
	if (idx_ll < 0) {
		for (i = 0; i + sizeof(pat_ll_pref) <= scan_len; i++) {
			if (memcmp(base + i, pat_ll_pref, sizeof(pat_ll_pref)) == 0) {
				idx_ll = (int32_t)i;
				break;
			}
		}
	}

	if (idx_double >= 0 && idx_ll >= 0) {
		uint8_t *src = base + idx_double;      /* points at dd 02 ... */
		uint8_t *dst = base + idx_ll;          /* beginning of LL handler */
		uint8_t *next = src + 5;               /* after overwritten bytes */
		int32_t rel = (int32_t)(dst - next);

#ifdef LINUX_RT
		long pagesz = sysconf(_SC_PAGESIZE);
		if (pagesz <= 0) pagesz = 4096;
		uintptr_t page = (uintptr_t)src & ~(uintptr_t)(pagesz - 1);
		mprotect((void *)page, pagesz, PROT_READ | PROT_WRITE | PROT_EXEC);
#endif
		src[0] = 0xE9;                         /* jmp rel32 */
		*(int32_t *)(src + 1) = rel;
#ifdef LINUX_RT
		mprotect((void *)page, pagesz, PROT_READ | PROT_EXEC);
#endif
	}

}

void init_FLOAT_vfprintf() {
	modify_vfprintf();
	modify_ppfs_setargs();
}
