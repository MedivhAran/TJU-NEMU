

#include "cpu/exec/helper.h"

// 实现 opcode 0xe8, 即 call rel32
// 我们可以遵循命名约定，用 'l' 代表 long (32-bit)
make_helper(call_rel32) {
    // 1. 读取4字节的相对偏移量
    int32_t offset = instr_fetch(eip + 1, 4);

    // 2. 将返回地址压栈
    // call 指令长度是 1 (opcode) + 4 (offset) = 5 字节
    // 返回地址是下一条指令的地址，即 eip + 5
    cpu.esp -= 4; // 栈向下增长
    swaddr_write(cpu.esp, 4, cpu.eip + 5);

    // 3. 更新 EIP 实现跳转
    // 新的 EIP = 当前EIP + 指令长度 + 偏移量
    // 即 EIP = (eip + 5) + offset
    // 框架会在执行完 helper 后自动将 eip 加上指令长度 (我们返回的5)
    // 所以我们在这里只需要加上偏移量即可
    cpu.eip += offset;

    // 4. 打印调试信息
    print_asm("call %x", cpu.eip + 5);

    // 5. 返回指令长度
    return 5;
}
