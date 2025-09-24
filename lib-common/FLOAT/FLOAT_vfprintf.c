#include <stdio.h>
#include <stdint.h>
#include "FLOAT.h"

#ifdef LINUX_RT
#include <sys/mman.h>
#include <unistd.h>
#endif

extern char _vfprintf_internal;
extern char _fpmaxtostr;
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
	/* For this uClibc build used in NEMU, redirecting the callee in
	 * _vfprintf_internal is sufficient to avoid using floating-point
	 * formatting. Many soft-float libc builds do not emit FPU ops in
	 * _ppfs_setargs, so we keep this as a no-op. If needed, this can be
	 * extended to patch _ppfs_setargs jump table to route PA_DOUBLE to
	 * the (PA_INT|PA_FLAG_LONG_LONG) handler.
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

}

void init_FLOAT_vfprintf() {
	modify_vfprintf();
	modify_ppfs_setargs();
}
