#include "monitor/watchpoint.h"
#include "monitor/expr.h"
#include "monitor/monitor.h"
#include "cpu/reg.h"
#include <string.h>
#include <assert.h>

#define NR_WP 32

WP *head = NULL;
static WP *free_ = NULL;
static WP wp_pool[NR_WP];

void init_wp_pool(void) {
	int i = 0;
    for (i = 0; i < NR_WP; i++) {
        wp_pool[i].NO = i;
        wp_pool[i].next = &wp_pool[i + 1];
    }
    wp_pool[NR_WP - 1].next = NULL;
    head = NULL;
    free_ = wp_pool;
}

WP* new_wp(char *expr_str) {
    if (free_ == NULL) {
        Assert(0, "No free watchpoint!");
    }

    WP* wp = free_;
    free_ = free_->next;

    wp->next = head;
    head = wp;

    strncpy(wp->expr, expr_str, sizeof(wp->expr) - 1);
    wp->expr[sizeof(wp->expr) - 1] = '\0';

    bool success = true;
    wp->value = expr(expr_str, &success);
    if (!success) {
        free_wp(wp->NO);
        Assert(0, "Invalid expression for watchpoint!");
    }

    printf("Watchpoint %d set, expr = %s, value = %u (0x%x)\n",
           wp->NO, wp->expr, wp->value, wp->value);

    return wp;
}

void free_wp(int NO) {
    WP *wp = head, *pre = NULL;

    while (wp != NULL && wp->NO != NO) {
        pre = wp;
        wp = wp->next;
    }
    Assert(wp != NULL, "No such watchpoint!");

    if (pre == NULL) {
        head = wp->next;
    } else {
        pre->next = wp->next;
    }

    wp->next = free_;
    free_ = wp;
}

void check_watchpoints(void) {
    WP *wp = head;
    while (wp) {
        bool success = true;
        uint32_t new_val = expr(wp->expr, &success);
        if (!success) assert(0);

        if (new_val != wp->value) {
            printf("Hint: watchpoint %d triggered at eip = 0x%08x\n", wp->NO, cpu.eip);
            printf("Old value = %u (0x%x), New value = %u (0x%x)\n",
                   wp->value, wp->value, new_val, new_val);
            wp->value = new_val;
            nemu_state = STOP;
        }
        wp = wp->next;
    }
}
