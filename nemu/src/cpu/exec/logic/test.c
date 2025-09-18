#include "cpu/exec/helper.h"
#include "cpu/reg.h" // 包含它以防万一

// 辅助函数，用于计算PF标志位
// 输入一个8位数，如果其中1的个数为偶数，返回true
static bool parity_flag(uint8_t val) {
    int i, count = 0;
    for (i = 0; i < 8; i++) {
        if ((val >> i) & 1) {
            count++;
        }
    }
    return !(count % 2);
}

// 辅助函数，用于根据运算结果设置ZSP标志位
static void set_ZSP_flags(uint32_t result) {
    cpu.eflags.ZF = !result;
    cpu.eflags.SF = (result >> 31);
    cpu.eflags.PF = parity_flag(result & 0xff);
}

// 实现 TEST r/m32, r32 (opcode 0x85)
// 我们把它命名为 test_r2rm_l (register to register/memory, long)
make_helper(test_r2rm_l) {
    // 1. 解码操作数
    // decode_rm_l会读取ModR/M字节，并把解码结果存入op_src和op_dest
    int len = decode_rm_l(eip + 1);

    // 2. 执行按位与运算
    uint32_t result = op_dest->val & op_src->val;

    // 3. 设置标志位
    set_ZSP_flags(result);
    cpu.eflags.CF = 0;
    cpu.eflags.OF = 0;
    
    // 4. 打印调试信息
    print_asm("test %s,%s", op_src->str, op_dest->str);

    // 5. 返回指令长度 (opcode 1字节 + ModR/M等解码出的长度)
    return len + 1;
}