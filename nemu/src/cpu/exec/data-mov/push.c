// 文件: nemu/src/cpu/exec/data-mov/push.c

#include "cpu/exec/helper.h"

// 'r' 表示寄存器, 'v' 表示 32位 (在 PA2 中)
make_helper(push_r_v) {

    int reg_code = instr_fetch(eip, 1) & 0x7;

    // 执行 push 操作
    cpu.esp -= 4;
    swaddr_write(cpu.esp, 4, cpu.gpr[reg_code]._32);

    // 打印调试信息
    // 返回指令长度
    return 1;
}
