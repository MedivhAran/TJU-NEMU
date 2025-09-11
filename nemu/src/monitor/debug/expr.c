#include "nemu.h"
#include <sys/types.h>
#include <regex.h>
#include <stdlib.h>  // for strtoul
#include <string.h>  // for strcmp, strncpy

// Token 类型枚举
enum {
  NOTYPE = 256, EQ, NEQ, AND, OR,
  HEX, DEC, REG, DEREF, NEG, LT, LE, GT, GE, NOT
};

static struct rule {
  char *regex;
  int token_type;
} rules[] = {
  {" +",    NOTYPE},             // 空格
  {"\\+",   '+'},                 // 加号
  {"-",     '-'},                 // 减号
  {"\\*",   '*'},                 // 乘号
  {"/",     '/'},                 // 除号
  {"\\(",   '('},                 // 左括号
  {"\\)",   ')'},                 // 右括号
  {"0[xX][0-9a-fA-F]+", HEX},     // 十六进制
  {"[0-9]+", DEC},                // 十进制
  {"\\$[a-zA-Z]+", REG},          // 寄存器
 {"==",    EQ},                  // 等于
  {"!=",    NEQ},                 // 不等
  {"&&",    AND},                 // 与
  {"\\|\\|", OR},                  // 或
};
#define NR_REGEX (sizeof(rules) / sizeof(rules[0]))
static regex_t re[NR_REGEX];

void init_regex() {
  int i;
  char error_msg[128];
  int ret;
  for (i = 0; i < NR_REGEX; i++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);
      Assert(0, "regex compilation failed: %s\n%s", error_msg, rules[i].regex);
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
    for (i = 0; i < NR_REGEX; i++) 
	{
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) 
	  {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        Log("match rules[%d] = \"%s\" at pos %d, len %d: %.*s",
            i, rules[i].regex, position, substr_len, substr_len, substr_start);

        position += substr_len;

        if (rules[i].token_type == NOTYPE) break; // 跳过空格

        tokens[nr_token].type = rules[i].token_type;

		if (rules[i].token_type == '*') {
                    if (nr_token == 0 || 
                        (tokens[nr_token - 1].type != DEC &&
                         tokens[nr_token - 1].type != HEX &&
                         tokens[nr_token - 1].type != REG &&
                         tokens[nr_token - 1].type != ')' )) {
                        tokens[nr_token].type = DEREF;
                    }
		}
		if (rules[i].token_type == '-') {
			if (nr_token == 0 ||
				(tokens[nr_token - 1].type != DEC &&
				tokens[nr_token - 1].type != HEX &&
				tokens[nr_token - 1].type != REG &&
				tokens[nr_token - 1].type != ')')) {
				tokens[nr_token].type = NEG;
			}
		}



        if (rules[i].token_type == DEC || rules[i].token_type == HEX || rules[i].token_type == REG) 
		{
          Assert(substr_len < sizeof(tokens[nr_token].str),"token too long: %.*s", substr_len, substr_start);
          strncpy(tokens[nr_token].str, substr_start, substr_len);
          tokens[nr_token].str[substr_len] = '\0';
        }
        nr_token++;
        break;
      }
    }


    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }
  return true;
}


static uint32_t my_str2reg(const char *s, bool *success) {
  int i;
  for (i = 0; i < 8; i++) {
    if (strcmp(s, regsl[i]) == 0) { *success = true; return reg_l(i); }
    if (strcmp(s, regsw[i]) == 0) { *success = true; return reg_w(i); }
    if (strcmp(s, regsb[i]) == 0) { *success = true; return reg_b(i); }
  }
  if (strcmp(s, "eip") == 0) { *success = true; return cpu.eip; }

  *success = false;
  return 0;
}


static bool check_parentheses_balance() {
  int balance = 0;
  int i;
  for (i = 0; i < nr_token; i++) {
    if (tokens[i].type == '(') balance++;
    else if (tokens[i].type == ')') balance--;
    if (balance < 0) return false; // 提前多了右括号
  }
  return balance == 0;
}

static bool check_parentheses_outer(int p, int q) {
  if (tokens[p].type != '(' || tokens[q].type != ')') return false;
  int balance = 0;
  int i;
  for (i = p; i <= q; i++) {
    if (tokens[i].type == '(') balance++;
    else if (tokens[i].type == ')') balance--;
    if (balance == 0 && i < q) return false; // 提前闭合
    if (balance < 0) return false;           // 错配
  }
  return balance == 0;
}


static int precedence(int type) {
  switch (type) {
    case OR:  return 1;
    case AND: return 2;
    case EQ: case NEQ: return 3;
    case '+': case '-': return 4;
    case '*': case '/': return 5;
	case DEREF: case NEG: return 7;
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
    if (balance > 0) continue;

    int pri = precedence(tokens[i].type);
    if (pri > 0) {
    if (pri < min_pri || (pri == min_pri && tokens[i].type != DEREF && tokens[i].type != NEG)) {
        min_pri = pri;
        op = i;
    }
}
  }
  return op;
}


static uint32_t eval(int p, int q) {
  if (p > q) {
    Assert(0, "Bad expression: empty range");
  }
  else if (p == q) {
    if (tokens[p].type == DEC) return strtoul(tokens[p].str, NULL, 10);
    if (tokens[p].type == HEX) return strtoul(tokens[p].str, NULL, 16);
    if (tokens[p].type == REG) {
      bool ok = true;
      uint32_t val = my_str2reg(tokens[p].str + 1, &ok);
      Assert(ok, "Unknown register: %s", tokens[p].str);
      return val;
    }
    Assert(0, "Unexpected token type %d", tokens[p].type);
  }
  else if (check_parentheses_outer(p, q)) {
    return eval(p + 1, q - 1);
  }
  else {
    int op = dominant_op(p, q);
    Assert(op != -1, "Bad expression: no operator");



	if (tokens[op].type == DEREF) {
        uint32_t addr = eval(op + 1, q);
        return swaddr_read(addr, 4); // 4字节读内存
    }
	if (tokens[op].type == NEG) {
		return -eval(op + 1, q);
	}

    uint32_t val1 = eval(p, op - 1);
    uint32_t val2 = eval(op + 1, q);

    switch (tokens[op].type) {
      case '+': return val1 + val2;
      case '-': return val1 - val2;
      case '*': return val1 * val2;
      case '/': Assert(val2 != 0, "Division by zero"); return val1 / val2;
      case EQ:  return val1 == val2;
      case NEQ: return val1 != val2;
      case AND: return val1 && val2;
      case OR:  return val1 || val2;
      default:  Assert(0, "Unknown operator %d", tokens[op].type);
    }
  }
  return 0;
}


uint32_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return 0;
  }
  if (!check_parentheses_balance()) {
    printf("Bad expression: unmatched parentheses\n");
    *success = false;
    return 0;
  }
  *success = true;
  return eval(0, nr_token - 1);
}
