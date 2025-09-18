#include "cpu/exec/helper.h"
#include "cpu/reg.h" // 确保包含了这个头文件

make_helper(push_r_v) {
    // 解码操作数：从 opcode 中获取寄存器编号
    int reg_code = instr_fetch(eip, 1) & 0x7;

    // 执行 push 操作
    cpu.esp -= 4;
    swaddr_write(cpu.esp, 4, cpu.gpr[reg_code]._32);

    // 打印调试信息
    print_asm("push %%%s", regsl[reg_code]);

    // 返回指令长度
    return 1;
}
