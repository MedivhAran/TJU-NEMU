#include "nemu.h"
/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <sys/types.h>
#include <regex.h>

enum {
	NOTYPE = 256, EQ, NEQ, AND, OR,
	/* Add more token types */
	HEX, DEC, REG,

	/* TODO: Add more token types */

};

static struct rule {
	char *regex;
	int token_type;
} rules[] = {
	/* TODO: Add more rules.
	 * Pay attention to the precedence level of different rules.
	 */
  {" +", NOTYPE},                // 空格
  {"\\+", '+'},                     // 加号
  {"-", '-'},                       // 减号
  {"\\*", '*'},                     // 乘号
  {"/", '/'},                       // 除号
  {"\\(", '('},                     // 左括号
  {"\\)", ')'},                     // 右括号
  {"0[xX][0-9a-fA-F]+", HEX},    // 十六进制数
  {"[0-9]+", DEC},               // 十进制数
  {"\\$[a-zA-Z]+", REG},         // 寄存器
  {"==", EQ},                    // 等于
  {"!=", NEQ},                   // 不等
  {"&&", AND},                   // 与
  {"\\|\\|", OR},                // 或	
};

#define NR_REGEX (sizeof(rules) / sizeof(rules[0]) )

static regex_t re[NR_REGEX];

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
	int i;
	char error_msg[128];
	int ret;

	for(i = 0; i < NR_REGEX; i ++) {
		ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
		if(ret != 0) {
			regerror(ret, &re[i], error_msg, 128);
			Assert(ret == 0, "regex compilation failed: %s\n%s", error_msg, rules[i].regex);
		}
	}
}

typedef struct token {
	int type;
	char str[32];
} Token;

Token tokens[32];
int nr_token;


static bool make_token(char *e) {
	int position = 0;
	int i;
	regmatch_t pmatch;

	nr_token = 0;

	while (e[position] != '\0') {
		for (i = 0; i < NR_REGEX; i ++) {
			if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
				char *substr_start = e + position;
				int substr_len = pmatch.rm_eo;

				Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
					i, rules[i].regex, position, substr_len, substr_len, substr_start);

				position += substr_len;

				if (rules[i].token_type == NOTYPE) {
					break; // 空格跳过
				}

				tokens[nr_token].type = rules[i].token_type;
				if (rules[i].token_type == DEC || rules[i].token_type == HEX || rules[i].token_type == REG) {
					Assert(substr_len < sizeof(tokens[nr_token].str),
						   "token too long: %.*s", substr_len, substr_start);
					strncpy(tokens[nr_token].str, substr_start, substr_len);
					tokens[nr_token].str[substr_len] = '\0';
				}
				nr_token++;
				break;
			}
		}

		if (i == NR_REGEX) {
			printf("no match at position %d\n%s\n%*.s^\n",
				   position, e, position, "");
			return false;
		}
	}

	return true;
}


static uint32_t my_str2reg(const char *s, bool *success) {
    // s 是寄存器名
    int i;
    for (i = 0; i < 8; i++) {
        if (strcmp(s, regsl[i]) == 0) { // regsl[] 存放寄存器名字，定义在 reg.c
            *success = true;
            return reg_l(i);            // reg_l(i) 获取 32位寄存器值
        }
    }

	for (i = 0; i < 8; i++) {
        if (strcmp(s, regsw[i]) == 0) { // regsl[] 存放寄存器名字，定义在 reg.c
            *success = true;
            return reg_w(i);            // reg_l(i) 获取 32位寄存器值
        }
    }

	for (i = 0; i < 8; i++) {
        if (strcmp(s, regsb[i]) == 0) { // regsl[] 存放寄存器名字，定义在 reg.c
            *success = true;
            return reg_b(i);            // reg_l(i) 获取 32位寄存器值
        }
    }


    if (strcmp(s, "eip") == 0) {
        *success = true;
        return cpu.eip;
    }

    // 找不到寄存器
    *success = false;
    printf("Unknown register: %s\n", s);
    return 0;
}


static bool check_parentheses(int p, int q) {
    // 最外层不是括号，直接返回 false
    if (tokens[p].type != '(' || tokens[q].type != ')') {
        return false;
    }

    int balance = 0;
	int i = p;
    for (i = p; i <= q; i++) {
        if (tokens[i].type == '(') {
            balance++;
        } else if (tokens[i].type == ')') {
            balance--;
            if (balance < 0) {
                // 出现右括号多余情况
                return false;
            }
        }

        // 如果在还没到 q 就已经闭合了，说明不是一对完整的大括号包着整个表达式
        if (balance == 0 && i < q) {
            return false;
        }
    }

    // balance 必须等于 0 才是括号匹配的
    return balance == 0;
}

static int precedence(int type) {
    switch (type) {
        case OR:  return 1;
        case AND: return 2;
        case EQ: case NEQ: return 3;
        case '+': return 4;
		case '-': return 4;
        case '*': return 5;
		case '/': return 5;
    }
    return 0;
}

static int dominant_op(int p, int q) {
    int op = -1;
    int min_pri = 100;
    int balance = 0;
	int i;
    for (i = p; i <= q; i++) {
        if (tokens[i].type == '(') { balance++; continue; }
        if (tokens[i].type == ')') { balance--; continue; }
        if (balance > 0) continue; // 括号中内容跳过

        int pri = precedence(tokens[i].type);
        if (pri > 0 && pri <= min_pri) {
            min_pri = pri;
            op = i;
        }
    }
    return op;
}

static int eval(int p, int q) {
	if (p > q) {
		return 0;
	}

	else if (p == q) {
		int val = 0;
		if (tokens[p].type == DEC) {
			sscanf(tokens[p].str, "%d", &val);
			return val;
		} else if (tokens[p].type == HEX) {
			sscanf(tokens[p].str, "%x", &val);
			return val;
		} else if (tokens[p].type == REG) {
			bool ok = true;
            return my_str2reg(tokens[p].str + 1, &ok); // 跳过 $
		} else {
			Assert(0, "Unexpected token type %d", tokens[p].type);
			return 0;
		}
	}

	else if (check_parentheses(p, q) == true) {
		return eval(p + 1, q - 1);
	}
	else {		//没有括号的情况
		int op = dominant_op(p, q);
		if (op == -1) {
			return 0;
		}

		int val1 = eval(p, op - 1);

		int val2 = eval(op + 1, q);

		switch (tokens[op].type) {
			case '+': return val1 + val2;
			case '-': return val1 - val2;
			case '*': return val1 * val2;
			case '/': 
				if (val2 == 0) {
					printf("Error: Cannot Divide zero!~\n");
					return 0;
				}
				return val1 / val2;
			case EQ: return val1 == val2;
			case NEQ: return val1 != val2;
			case AND: return val1 && val2;
			case OR: return val1 || val2;
			default:
				return 0;
		}
	}
}







uint32_t expr(char *e, bool *success) {
	if(!make_token(e)) {
		*success = false;
		return 0;
	}
	*success = true;
	/* TODO: Insert codes to evaluate the expression. */
	return eval(0, nr_token - 1);
}


