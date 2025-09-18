#include "cpu/exec/helper.h"
#include "cpu/reg.h" 
make_helper(je_i_b) {
    // 解码出8位偏移量
    int8_t offset = instr_fetch(eip + 1, 1);
    
    // 打印汇编指令
    print_asm("je 0x%x", eip + 2 + offset);

    if (cpu.eflags.ZF == 1) {
        // 如果ZF=1，执行跳转
        cpu.eip += offset;
    }

    // 不论是否跳转，指令长度都是2字节
    return 2; 
}
