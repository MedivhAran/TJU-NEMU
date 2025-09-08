#ifndef __REG_H__
#define __REG_H__

#include "common.h"

//所有寄存器，一共 24 个
enum { R_EAX, R_ECX, R_EDX, R_EBX, R_ESP, R_EBP, R_ESI, R_EDI };
enum { R_AX, R_CX, R_DX, R_BX, R_SP, R_BP, R_SI, R_DI };
enum { R_AL, R_CL, R_DL, R_BL, R_AH, R_CH, R_DH, R_BH };

/* TODO: Re-organize the `CPU_state' structure to match the register
 * encoding scheme in i386 instruction format. For example, if we
 * access cpu.gpr[3]._16, we will get the `bx' register; if we access
 * cpu.gpr[1]._8[1], we will get the 'ch' register. Hint: Use `union'.
 * For more details about the register encoding scheme, see i386 manual.
 */

typedef struct {
	union{
		struct{
     		union {
				uint32_t _32;
				uint16_t _16;
				uint8_t _8[2];
     		} gpr[8];
		};
	 /*gpr[8]：长度为 8 的数组，对应 8 个通用寄存器（EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI）。每个 gpr[i] 又是一个小结构体，里面有三种不同大小的字段：
		_32 → 对应 32 位寄存器（如 EAX）
		_16 → 对应低 16 位寄存器（如 AX）
		_8[0] → 对应低 8 位（如 AL）
		_8[1] → 对应高 8 位（如 AH）
		但是这里的问题在于结构体内的这几个变量是分开存储的，没有共享内存，所以需要引入union联，让多个字段共享同一块内存空间。*/

		/* Do NOT change the order of the GPRs' definitions. */
	 //八个 32 位寄存器
     	struct{uint32_t eax, ecx, edx, ebx, esp, ebp, esi, edi;};
	 };

     swaddr_t eip;
     
     union {
		struct {
			uint32_t CF		:1;
			uint32_t pad0	:1;
			uint32_t PF		:1;
			uint32_t pad1	:1;
			uint32_t AF		:1;
			uint32_t pad2	:1;
			uint32_t ZF		:1;
			uint32_t SF		:1;
			uint32_t TF		:1;
			uint32_t IF		:1;
			uint32_t DF		:1;
			uint32_t OF		:1;
			uint32_t IOPL	:2;
			uint32_t NT		:1;
			uint32_t pad3	:1;
			uint16_t pad4;
		};
		uint32_t val;
	} eflags;

} CPU_state;

extern CPU_state cpu;

static inline int check_reg_index(int index) {
	assert(index >= 0 && index < 8);
	return index;
}

#define reg_l(index) (cpu.gpr[check_reg_index(index)]._32)
#define reg_w(index) (cpu.gpr[check_reg_index(index)]._16)
#define reg_b(index) (cpu.gpr[check_reg_index(index) & 0x3]._8[index >> 2])

extern const char* regsl[];
extern const char* regsw[];
extern const char* regsb[];

#endif
