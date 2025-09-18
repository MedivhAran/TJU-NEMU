#include "cpu/exec/helper.h"
// 'r' 表示寄存器, 'v' 表示 32位 (在 PA2 中)
make_helper(push_r_v) {
    // 解码操作数：从 opcode 中获取寄存器编号
    // 0x50 是 push %eax, 0x51 是 push %ecx ...
    // 所以寄存器编号就是 opcode 的最后 3 个 bit
    int reg_code = instr_fetch(eip, 1) & 0x7;

    // 执行 push 操作
    cpu.esp -= 4;
    // cpu.gpr[reg_code]._32 就是对应的32位寄存器
    swaddr_write(cpu.esp, 4, cpu.gpr[reg_code]._32);

    // 打印调试信息 (非常重要! 之前你漏了这一行)
    // reg_name() 是框架提供的函数，根据编号获取寄存器名字
    print_asm("push ");
    // 返回指令长度
    return 1;
}
