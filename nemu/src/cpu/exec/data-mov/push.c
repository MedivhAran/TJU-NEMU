#include "cpu/exec/helper.h"


make_helper(push_ebp) {
    // 1. 将 esp 减 4
    cpu.esp -= 4;

    // 2. 将 ebp 的值写入新的 esp 地址
    // 参数：(地址, 写入字节数, 要写入的值)
    swaddr_write(cpu.esp, 4, cpu.ebp);

    // 3. 调试信息，这是好习惯
    print_asm("push %%ebp");

    // `push %ebp` 这条指令只占 1 个字节 (opcode 0x55)
    return 1;
}