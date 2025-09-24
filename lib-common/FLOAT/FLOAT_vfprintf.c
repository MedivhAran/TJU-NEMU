#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>
#include "FLOAT.h"

extern char _vfprintf_internal;
extern char _fpmaxtostr;
extern char _ppfs_setargs;
extern int __stdio_fwrite(char *buf, int len, FILE *stream);

__attribute__((used)) static int format_FLOAT(FILE *stream, FLOAT f) {
	// 将 16.16 定点数按 6 位小数截断输出，不使用任何浮点指令
	char buf[64];
	uint32_t uf;
	int neg = (f < 0);
	if (neg) uf = (uint32_t)(-f); else uf = (uint32_t)f;
	uint32_t ip = uf >> 16;                     // 整数部分
	uint32_t frac = ((uint64_t)(uf & 0xffff) * 1000000ULL) >> 16; // 截断
	int len;
	if (neg) len = sprintf(buf, "-%u.%06u", ip, frac);
	else      len = sprintf(buf, "%u.%06u", ip, frac);
	return __stdio_fwrite(buf, len, stream);
}

static void modify_vfprintf() {
	/* 运行时劫持 _vfprintf_internal 中对 %f 的处理：
	 * 1) 将调用目标从 _fpmaxtostr 改为本文件的 format_FLOAT
	 * 2) 用 push DWORD PTR [edx] 作为第二实参(定点值)，保持第一实参 stream 不变
	 * 3) 把前面的 fstpt (%esp) 改成 push [edx]，并把 "sub esp,0x0c" 调整为 0x08
	 * 4) 清除周围的浮点指令 fldl/fldt 为 nop
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

	/* You should modify the run-time binary to let the code above
	 * call `format_FLOAT' defined in this source file, instead of
	 * `_fpmaxtostr'. When this function returns, the action of the
	 * code above should do the following:
	 */

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

	// -------- 实际劫持实现 --------
	uint8_t *p = (uint8_t *)&_vfprintf_internal;
	// 提前放开相邻内存页的写权限
	uintptr_t base = (uintptr_t)p;
	uintptr_t page = base & ~(uintptr_t)0xfff;
	mprotect((void *)page, 4096 * 4, PROT_READ | PROT_WRITE | PROT_EXEC);

	// 1) 定位 call _fpmaxtostr 的位置
	int call_idx = -1;
	int i = 0;
	for (i = 0; i < 2048; i++) {
		if (p[i] == 0xE8) { // call rel32
			int32_t rel = *(int32_t *)(p + i + 1);
			uint8_t *tgt = p + i + 5 + rel;
			if (tgt == (uint8_t *)&_fpmaxtostr) { call_idx = i; break; }
		}
	}
	if (call_idx < 0) return; // 未找到，放弃

	// 2) 在 call 之前的窗口内，处理参数与浮点指令
	int win_begin = call_idx - 100; if (win_begin < 0) win_begin = 0;
	int win_end = call_idx;       // 不含 call 自身
	int fstpt_idx = -1;
	int subesp_idx = -1;
	i = win_begin;
	for (i = win_begin; i < win_end - 2; i++) {
		// 2.a 查找 fstpt (%esp) -> db 3c 24
		if (p[i] == 0xDB && p[i + 1] == 0x3C && p[i + 2] == 0x24) { fstpt_idx = i; }
		// 2.b 查找 sub esp, imm8(0x0c) -> 83 ec 0c
		if (p[i] == 0x83 && p[i + 1] == 0xEC) { subesp_idx = i; }
	}
	if (fstpt_idx >= 0) {
		// 替换为: push DWORD PTR [edx] (ff 32) + nop
		p[fstpt_idx + 0] = 0xFF; // push r/m32
		p[fstpt_idx + 1] = 0x32; // [edx]
		p[fstpt_idx + 2] = 0x90; // nop 填充
	}
	if (subesp_idx >= 0 && p[subesp_idx] == 0x83 && p[subesp_idx + 1] == 0xEC) {
		// 将 0x0c 调小 4 字节，抵消上面的 push 新增的栈深
		// 注意：原第三字节是立即数
		uint8_t imm = p[subesp_idx + 2];
		if (imm >= 4) p[subesp_idx + 2] = imm - 4; // 一般是 0x0c -> 0x08
	}
	// 2.c 清除附近的 fldt (%edx)/fldl (%edx)
	i = win_begin;
	for (i = win_begin; i < win_end - 1; i++) {
		if ((p[i] == 0xDB && p[i + 1] == 0x2A) || // fldt (%edx)
			(p[i] == 0xDD && p[i + 1] == 0x02)) {  // fldl (%edx)
			p[i] = 0x90; p[i + 1] = 0x90; // 两字节 nop
		}
	}

	// 3) 将 call 目标改为 format_FLOAT
	int32_t new_rel = (int32_t)((uint8_t *)&format_FLOAT - (p + call_idx + 5));
	*(int32_t *)(p + call_idx + 1) = new_rel;
}

static void modify_ppfs_setargs() {
	/* 运行时修改 _ppfs_setargs：让 PA_DOUBLE 分支复用
	 * (PA_INT|PA_FLAG_LONG_LONG) 的取参逻辑，避免浮点指令。
	 * 实现方法：在 "fldl (%edx)" 处写入短跳，跳转到后面
	 * 已有的 64 位搬运代码块开始处。
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

	// -------- 实际修改实现 --------
	uint8_t *p = (uint8_t *)&_ppfs_setargs;
	uintptr_t page = ((uintptr_t)p) & ~(uintptr_t)0xfff;
	mprotect((void *)page, 4096 * 2, PROT_READ | PROT_WRITE | PROT_EXEC);

	// 在函数前 0x400 字节中，先找到 "lea 0x8(%edx),%ebx; fldl (%edx)" 的位置，
	// 再在后续寻找 64 位搬运块 "mov (%edx),%edi; mov 0x4(%edx),%ebp" 的起点，
	// 然后把 fldl 覆盖为 jmp rel8 到该块。
	int fldl_idx = -1;
	int here_idx = -1;
	int i = 0;
	for (i = 0; i < 0x400; i++) {
		// 匹配: 8d 5a 08 dd 02 89 58 4c dd 19 （允许后半不完全匹配，仅抓到 dd 02）
		if (i + 1 < 0x400 && p[i] == 0xDD && p[i + 1] == 0x02) {
			// 前面 3 字节是否是 lea 0x8(%edx),%ebx
			if (i >= 3 && p[i - 3] == 0x8D && p[i - 2] == 0x5A && p[i - 1] == 0x08) {
				fldl_idx = i;
				break;
			}
		}
	}
	// 在后续 128 字节中找 64 位搬运块起点: 8b 3a 8b 6a 04
	if (fldl_idx >= 0) {
		int j = fldl_idx;
		for (j = fldl_idx; j < fldl_idx + 128 && j + 4 < 0x600; j++) {
			if (p[j] == 0x8B && p[j + 1] == 0x3A && p[j + 2] == 0x8B && p[j + 3] == 0x6A && p[j + 4] == 0x04) {
				here_idx = j; break;
			}
		}
	}
	if (fldl_idx >= 0 && here_idx > fldl_idx && here_idx - fldl_idx - 2 >= -128 && here_idx - fldl_idx - 2 <= 127) {
		// 写入短跳 EB xx
		int8_t rel8 = (int8_t)(here_idx - (fldl_idx + 2));
		p[fldl_idx + 0] = 0xEB; // jmp short
		p[fldl_idx + 1] = (uint8_t)rel8;
	}
}

void init_FLOAT_vfprintf() {
	modify_vfprintf();
	modify_ppfs_setargs();
}
