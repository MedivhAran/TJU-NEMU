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

extern WP *head;

void init_wp_pool(void);

WP* new_wp(char *expr_str);

void free_wp(int NO);

void check_watchpoints(void);

#endif
