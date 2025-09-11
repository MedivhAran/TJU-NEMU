#include "monitor/monitor.h"
#include "monitor/expr.h"
#include "monitor/watchpoint.h"
#include "nemu.h"

#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>

void cpu_exec(uint32_t);


/* We use the `readline' library to provide more flexibility to read from stdin. */
char* rl_gets() {
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




//六个指令
static int cmd_c(char *args) {
	cpu_exec(-1);
	return 0;
}
static int cmd_q(char *args) {
	return -1;
}

static int cmd_help(char *args);

//单步调试
static int cmd_si(char *args);
//打印寄存器信息
static int cmd_info(char *args);
//扫描内存
static int cmd_scan(char *args);



static struct {
	char *name;
	char *description;
	int (*handler) (char *);	//函数指针
	} cmd_table [] = {
	{ "help", "Display informations about all supported commands", cmd_help },
	{ "c", "Continue the execution of the program", cmd_c },
	{ "q", "Exit NEMU", cmd_q },
	/* TODO: Add more commands */
	{ "si", "step into (n steps)", cmd_si},
	{"info", "show info of register", cmd_info},
	{"x", "scan memory", cmd_scan}
};

#define NR_CMD (sizeof(cmd_table) / sizeof(cmd_table[0]))

static int cmd_si(char *args){
	int step = 1;
    if (args != NULL) {
        sscanf(args, "%d", &step);
    }
    cpu_exec(step);
    return 0;
}

static int cmd_info(char *args) {
    if (args == NULL) return 0;
	//若用户输入的参数为 r
    if (strcmp(args, "r") == 0) {
        {
			int i;
			//依次打印所有寄存器的信息
    		for (i = 0; i < 8; i++) {
			//寄存器名字 寄存器16进制的值  寄存器10进制的值
        	printf("%s\t0x%08x\t%d\n", regsl[i], reg_l(i), reg_l(i));
    	}
    	printf("eip\t0x%08x\t%d\n", cpu.eip, cpu.eip);
		}
    } 
    return 0;
}

static int cmd_scan(char *args) {
    int N;
    uint32_t addr;
    if (args == NULL) 
	{return 0;}
    sscanf(args, "%d %x", &N, &addr);
	int i = 0;
    for (i = 0; i < N; i++) {
        uint32_t data = swaddr_read(addr + i * 4, 4);
        printf("0x%08x: 0x%08x\n", addr + i * 4, data);
    }
    return 0;
}

static int cmd_help(char *args) {
	/* extract the first argument */
	char *arg = strtok(NULL, " ");
	int i;

	if(arg == NULL) {
		/* no argument given */
		for(i = 0; i < NR_CMD; i ++) {
			printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
		}
	}
	else {
		for(i = 0; i < NR_CMD; i ++) {
			if(strcmp(arg, cmd_table[i].name) == 0) {
				printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
				return 0;
			}
		}
		printf("Unknown command '%s'\n", arg);
	}
	return 0;
}


void ui_mainloop() {
	while(1) {
		//从终端读取一行用户输入
		char *str = rl_gets();
		char *str_end = str + strlen(str);

		/* extract the first token as the command */
		char *cmd = strtok(str, " ");
		if(cmd == NULL) { continue; }

		/* treat the remaining string as the arguments,
		 * which may need further parsing
		 */
		//cmd此时是指令的本体，现在需要分割后面的参数，+1是因为空格
		char *args = cmd + strlen(cmd) + 1;
		if(args >= str_end) {
			args = NULL;
		}

#ifdef HAS_DEVICE
		extern void sdl_clear_event_queue(void);
		sdl_clear_event_queue();
#endif

		int i;
		//遍历指令
		for(i = 0; i < NR_CMD; i ++) {
			if(strcmp(cmd, cmd_table[i].name) == 0) {
				if(cmd_table[i].handler(args) < 0) { return; }
				break;
			}
		}

		if(i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
	}
}




