#include "cpu/exec/helper.h"

make_helper(call_rel32) {
    // 读取4字节的相对偏移量
    int32_t offset = instr_fetch(eip + 1, 4);

    // 将返回地址压栈
    // call 指令长度是 1 (opcode) + 4 (offset) = 5 字节
    // 返回地址是下一条指令的地址，即 eip + 5
    cpu.esp -= 4; // 栈向下增长
    swaddr_write(cpu.esp, 4, cpu.eip + 5);

    // 更新 EIP 实现跳转
    // 新的 EIP = 当前EIP + 指令长度 + 偏移量
    // 即 EIP = (eip + 5) + offset
    cpu.eip += offset;

    // 打印调试信息
    print_asm("call %x", cpu.eip + 5);

    // 返回指令长度
    return 5;
}
