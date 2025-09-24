#include "common.h"
#include "monitor/monitor.h"
#include "monitor/expr.h"
#include "monitor/watchpoint.h"
#include "nemu.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>

void cpu_exec(uint32_t);

static char* rl_gets(void) {
    static char *line_read = NULL;
    if (line_read) {
        free(line_read);
        line_read = NULL;
    }
    line_read = readline("(nemu) ");
    if (line_read && *line_read) {
        add_history(line_read);
    }
    return line_read;
}

static int cmd_c(char *args) {
    cpu_exec(-1);
    return 0;
}

static int cmd_q(char *args) {
    return -1;
}

static int cmd_help(char *args);
static int cmd_si(char *args);
static int cmd_info(char *args);
static int cmd_scan(char *args);
static int cmd_eval(char *args);
static int cmd_w(char *args);
static int cmd_d(char *args);
static int cmd_bt(char *args);

static struct {
    char *name;
    char *description;
    int (*handler)(char *);
} cmd_table [] = {
    { "help", "Display informations about all supported commands", cmd_help },
    { "c", "Continue the execution of the program", cmd_c },
    { "q", "Exit NEMU", cmd_q },
    { "si", "Step execute N instructions (default 1)", cmd_si },
    { "info", "Show registers or watchpoints", cmd_info },
    { "x", "Scan memory", cmd_scan },
    { "p", "Evaluate expression", cmd_eval },
    { "w", "Set watchpoint", cmd_w },
    { "d", "Delete watchpoint", cmd_d },
    { "bt", "Print the StackFrame Chain", cmd_bt}
};

#define NR_CMD (sizeof(cmd_table) / sizeof(cmd_table[0]))

static int cmd_si(char *args) {
    int step = 1;
    if (args != NULL) {
        sscanf(args, "%d", &step);
    }
    cpu_exec(step);
    return 0;
}

static int cmd_info(char *args) {
    int i;
    if (args == NULL) return 0;

    if (strcmp(args, "r") == 0) {
        for (i = 0; i < 8; i++) {
            printf("%s\t0x%08x\n", regsl[i], reg_l(i));
        }
        printf("eip\t0x%08x\n", cpu.eip);
    }
    else if (strcmp(args, "w") == 0) {
        WP *wp = head;
        if (!wp) {
            printf("No watchpoints.\n");
        } else {
            printf("Num\tExpr\t\tValue\n");
            while (wp) {
                printf("%d\t%s\t%u (0x%x)\n", wp->NO, wp->expr, wp->value, wp->value);
                wp = wp->next;
            }
        }
    }
    return 0;
}

static int cmd_scan(char *args) {
    int N;
    uint32_t addr;
    int i;
    if (args == NULL) return 0;
    sscanf(args, "%d %x", &N, &addr);
    for (i = 0; i < N; i++) {
        uint32_t data = swaddr_read(addr + i * 4, 4);
        printf("0x%08x: 0x%08x\n", addr + i * 4, data);
    }
    return 0;
}

static int cmd_help(char *args) {
    char *arg = strtok(NULL, " ");
    int i;
    if (arg == NULL) {
        for (i = 0; i < NR_CMD; i++) {
            printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        }
    } else {
        for (i = 0; i < NR_CMD; i++) {
            if (strcmp(arg, cmd_table[i].name) == 0) {
                printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
                break;
            }
        }
        if (i == NR_CMD) {
            printf("Unknown command '%s'\n", arg);
        }
    }
    return 0;
}

static int cmd_eval(char *args) {
	if (args == NULL) return 0;
	bool success = true;
	uint32_t result = expr(args, &success);
	if (success) {
		printf("%u\n", result);
	} else {
		printf("Invalid expression!\n");
	}
	return 0;
}

static int cmd_w(char *args) {
    if (args == NULL) return 0;
    new_wp(args);
    return 0;
}

static int cmd_d(char *args) {
    int no;
    if (args == NULL) return 0;
    no = atoi(args);
    free_wp(no);
    return 0;
}

void ui_mainloop(void) {
    while (1) {
        char *str = rl_gets();
        char *str_end = str + strlen(str);

        char *cmd = strtok(str, " ");
        if (cmd == NULL) { continue; }

        char *args = cmd + strlen(cmd) + 1;
        if (args >= str_end) {
            args = NULL;
        }

#ifdef HAS_DEVICE
        extern void sdl_clear_event_queue(void);
        sdl_clear_event_queue();
#endif

        {
            int i;
            for (i = 0; i < NR_CMD; i++) {
                if (strcmp(cmd, cmd_table[i].name) == 0) {
                    if (cmd_table[i].handler(args) < 0) { return; }
                    break;
                }
            }
            if (i == NR_CMD) {
                printf("Unknown command '%s'\n", cmd);
            }
        }
    }
}

static int cmd_bt(char *args){
    const char *search_func_name(uint32_t eip);
    struct{
        swaddr_t prev_ebp;
        swaddr_t ret_addr;
        uint32_t args[4];
    }PartOfStackFrame;
    uint32_t ebp = cpu.ebp;
    uint32_t eip = cpu.eip;
    int i = 0;
    while(ebp != 0){
        PartOfStackFrame.args[0] = swaddr_read(ebp + 8,4);
        PartOfStackFrame.args[1] = swaddr_read(ebp + 12,4);
        PartOfStackFrame.args[2] = swaddr_read(ebp + 16,4);
        PartOfStackFrame.args[3] = swaddr_read(ebp + 20,4);

     printf("#%d 0x%08x in %s (0x%08x 0x%08x 0x%08x 0x%08x)\n",
         i, eip, search_func_name(eip),
         PartOfStackFrame.args[0], PartOfStackFrame.args[1],
         PartOfStackFrame.args[2], PartOfStackFrame.args[3]);
        i++;
        eip = swaddr_read(ebp + 4,4); 
        ebp = swaddr_read(ebp,4); 
    }
    return 0;
}

