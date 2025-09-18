#include "cpu/exec/helper.h"

void update_eflags(uint32_t result, uint32_t operand1, uint32_t operand2) {
    // 更新ZF标志位：如果结果是0，ZF为1
    cpu.eflags.ZF = (result == 0);
    // 更新SF标志位：如果结果是负数，SF为1
    cpu.eflags.SF = ((int32_t)result < 0);
    // 更新CF标志位：如果operand1 < operand2，CF为1（表示有借位）
    cpu.eflags.CF = (operand1 < operand2);
    // 更新OF标志位：如果减法溢出，OF为1
    cpu.eflags.OF = (((operand1 ^ operand2) & (operand1 ^ result)) >> 31) & 1;
    // 更新PF标志位：如果结果中1的个数是偶数，PF为1
    cpu.eflags.PF = (__builtin_parity(result) == 0);
}

make_helper(cmp_r2rm_b) {
    // 比较8位寄存器和内存
    uint8_t operand1 = op_src->val;  // 寄存器的值
    uint8_t operand2 = op_dest->val; // 内存的值
    update_eflags(operand1 - operand2, operand1, operand2);  // 执行减法并更新EFLAGS
    return 1;  // 返回指令长度
}

make_helper(cmp_r2rm_v) {
    // 比较32位寄存器和内存
    uint32_t operand1 = op_src->val;  // 寄存器的值
    uint32_t operand2 = op_dest->val; // 内存的值
    update_eflags(operand1 - operand2, operand1, operand2);  // 执行减法并更新EFLAGS
    return 1;  // 返回指令长度
}