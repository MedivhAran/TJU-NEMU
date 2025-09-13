#ifndef __WATCHPOINT_H__
#define __WATCHPOINT_H__

#include "common.h"
#include <stdint.h>

typedef struct watchpoint {
    int NO;
    struct watchpoint *next;
    char expr[256];
    uint32_t value;
} WP;

// 全局头指针
extern WP *head;

// 初始化监视点池
void init_wp_pool(void);

// 创建新监视点（会自动计算初始值）
WP* new_wp(char *expr_str);

// 删除监视点（根据编号）
void free_wp(int NO);

// 检查所有监视点，值发生变化就停止执行
void check_watchpoints(void);

#endif
