/***************************************************************************************
* Copyright (c) 2014-2024 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include <isa.h>
#include <regex.h>
/* 
 * We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */

enum {
	TK_NOTYPE = 256,    // 空格, 识别后直接丢弃
	// TK_EQ,              // "=="

	TK_NUM,             // 十进制整数, 如 123
	TK_HEX,             // 十六进制整数, 如 0x80000000
	TK_REG,             // 寄存器, 如 $a0, $pc
  	/* TODO: Add more token types */
};


static struct rule {
	const char *regex;
	int token_type;
} rules[] = {
	/* 规则按"具体优先 / 最长优先"排列:
	 * 0x 开头的十六进制数必须排在十进制数前面,否则 "0x80000000" 会被 "[0-9]+" 先匹配成 "0" 
	 */
	{" +",                  TK_NOTYPE},     // 空格串 (一个或多个)
	{"0[xX][0-9a-fA-F]+",   TK_HEX},        // 十六进制整数
	{"[0-9]+u?",            TK_NUM},        // 十进制整数 (u 后缀供任务 6 使用)
	{"\\$[a-zA-Z0-9]+",     TK_REG},        // 寄存器 ($a0, $pc, ...)
	// {"==",                  TK_EQ },        // equal

	{"\\+",                 '+'},           // 加号
	{"-",                   '-'},           // 减号 / 负号
	{"\\*",                 '*'},           // 乘号
	{"/",                   '/'},           // 除号
	{"\\(",                 '('},           // 左括号
	{"\\)",                 ')'},           // 右括号
	/* TODO: Add more rules. */
};

#define NR_REGEX ARRLEN(rules)
static regex_t re[NR_REGEX] = {};

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
	int i;
	char error_msg[128];
	int ret;

	for (i = 0; i < NR_REGEX; i ++) {
		ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
		if (ret != 0) {
			regerror(ret, &re[i], error_msg, 128);
			panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
		}
	}
}

// ===================================== 实现了词法分析 =======================================
// 定义 Token 结构体
typedef struct token {
	int type;
	char str[32];
} Token;

// __attribute__((used)) 是 GCC/Clang 的扩展属性，告诉编译器"即使这个变量在当前文件里看起来没被直接引用，也不要把它当死代码优化掉"
#define MAX_TOKENS 512
static Token tokens[MAX_TOKENS] __attribute__((used)) = {};
static int nr_token __attribute__((used)) = 0;

/* 
 * add_token(int, const char*, int): 把识别出的一个 token 记录到 tokens 数组中.
 * type : token的类型，本质只是一个int
 * str  : token的值 - 实际是一个指向token开头的字符串指针，只有数字/寄存器才有意义，否则传NULL
 * len  : token的长度，服务于上面的指针
 */
static void add_token(int type, const char *str, int len) {
	assert(nr_token < MAX_TOKENS && "Tokens overflow!");
	tokens[nr_token].type = type;
	
	// 只有需要知道具体内容的 token (数字/寄存器) 才复制子串.且缓冲区不够时直接 assert 失败
	if (type == TK_NUM || type == TK_HEX || type == TK_REG) {
		assert(len < (int)sizeof(tokens[nr_token].str) && "Token length overflow!");
		strncpy(tokens[nr_token].str, str, len);	// 用 strncpy 将字符串复制到 tokens[i] 结构体的 str 中
		tokens[nr_token].str[len] = '\0';			// strncpy 是值复制, 不会在末尾自动加\0.故手动添加
	}

	nr_token++;
}

static bool make_token(char *e) {
	int position = 0;
	int i;
	regmatch_t pmatch;

	nr_token = 0;

	while (e[position] != '\0') {
		for (i = 0; i < NR_REGEX; i++) {
			/* 从当前位置开始匹配第 i 条规则
			 * int regexec(const regex_t *preg, const char *string, size_t nmatch,regmatch_t pmatch[], int eflags):
			 * &re[i]     : 预编译的正则对象
			 * e+position : 从当前位置开始的子字符串
			 * 1          : 只关心第一个匹配结果
			 * &pmatch    : 将结果的属性放置到pmatch结构体中
			 * 0          : 无特殊错误标志
			 */
			if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
				// pmatch.rm_so == 0 表示必须从头匹配(e+position)，不能跳过任何字符。例如:"abc123", [0-9]能匹配123, 但rm_so != 0
				char *substr_start = e + position;
				int substr_len = pmatch.rm_eo - pmatch.rm_so;

				Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
					i, rules[i].regex, position, substr_len, substr_len, substr_start);

				position += substr_len;

				/* 记录 token; 空格串 TK_NOTYPE 直接丢弃 */
				switch (rules[i].token_type) {
					case TK_NOTYPE:
						break;
					case TK_NUM:
					case TK_HEX:
					case TK_REG:
						add_token(rules[i].token_type, substr_start, substr_len);
						break;
					default:
						add_token(rules[i].token_type, NULL, 0);
						break;
		/* TODO: Now a new token is recognized with rules[i]. Add codes
		 * to record the token in the array `tokens'. For certain types
		 * of tokens, some extra actions should be performed.
		 */
				}
				break;      // 命中一条规则就重新从头试, 不再往下匹配
			}
		}

		if (i == NR_REGEX) {
			printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
			return false;
		}
	}

	// nr_token=0 则说明全都是空格, 表达式无意义, 故此处同样返回false. 否则返回true
	return (nr_token == 0 ? false:true);
}
// =====================================================================================================


// ========================== 基于词法分析得到的tokens,实现了表达式求值 ===================================
/* 检查 [p, q] 区间的括号, 返回:
 *    1 : 被一对匹配的括号整体包围        如 "(2 - 1)"
 *    0 : 括号匹配, 但不被整体包围        如 "4 + 3 * (2 - 1)"
 *   -1 : 括号不匹配, 非法表达式          如 "(4 + 3)) * ((2 - 1)"
 */
static int check_parentheses(int p, int q) {
	/* 第一遍: 检查括号是否匹配 */
	int depth = 0;
	for (int i = p; i <= q; i++) {
		if (tokens[i].type == '(') {
			depth++;
		} else if (tokens[i].type == ')') {
			depth--;
			if (depth < 0) return -1;       // 右括号多余
		}
	}
	if (depth != 0) return -1;              // 左括号多余

	/* 第二遍: 括号已匹配, 再判断首尾是否是一对包围整个区间的括号 */
	if (tokens[p].type != '(' || tokens[q].type != ')') return 0;	// 不被整体包围的返回 0 

	depth = 0;
	for (int i = p; i <= q; i++) {
		if (tokens[i].type == '(') {
			depth++;
		} else if (tokens[i].type == ')') {
			depth--;
			if (depth == 0 && i != q) return 0;   // 首括号提前闭合, 非整体包围, 返回 0 
		}
	}

	return 1;
}

/* 判断位置 pos 上的运算符是否为单目 (负号).
 * 当它前面是区间开头、左括号、或另一个运算符时, 它是单目的
 * ——因为运算符后面不可能紧跟一个二元运算符.
 */
static bool is_unary(int pos, int p) {
	if (pos == p) return true;

	int prev = tokens[pos - 1].type;
	return  prev == '(' || prev == '+' || 
			prev == '-' || prev == '*' || prev == '/';
}

/* 在 [p, q] 中寻找主运算符:
 *   1. 非运算符的 token 不是主运算符
 *   2. 出现在一对括号中的 token 不是主运算符(这也是要实现check_parentheses的原因,为了让这条规则有效)
 *   3. 主运算符的优先级在 (括号外的) 所有运算符中最低
 *   4. 多个同为最低优先级时, 取最右边的 (左结合) -> 取最左边: 8-4-2=8-(2)=6 会出问题！
 * 优先级: + - 为 1, * / 为 2. 找不到返回 -1. */
static int find_main_op(int p, int q) {
	int depth = 0;
	int op = -1;
	int op_prio = 99;        // 大于任何实际优先级 (1, 2)

	for (int i = p; i <= q; i++) {
		int t = tokens[i].type;

		if (t == '(') {
			depth++;
		} else if (t == ')') {
			depth--;
		} else if (depth == 0 && !is_unary(i, p)
				&& (t == '+' || t == '-' || t == '*' || t == '/')) {
			// 只有"括号外的" "非单目" "运算符token"才有可能是主运算符
			int prio = (t == '+' || t == '-') ? 1 : 2;
			if (prio <= op_prio) {      // <= 保证取最右边的那个
				op_prio = prio;
				op = i;
			}
		}
	}

	return op;
}

static word_t eval(int p, int q, bool* success){
	/* ① 空区间: 非法 (如 "1+" 的右子式或 "+3" 的左子式 - make_token不会报错,但是语义出错: 在找到主运算符的下一次递归后报错) */
	if (p > q) {
		*success = false;
		return 0;
	}

	/* ② 单个 token: 递归基, 必须是数字或寄存器 */
	if (p == q) {
		switch (tokens[p].type) {
			case TK_NUM: return strtoul(tokens[p].str, NULL, 10);
			case TK_HEX: return strtoul(tokens[p].str, NULL, 16);
			case TK_REG: return isa_reg_str2val(tokens[p].str, success);
			default:     *success = false; return 0;
		}
	}

	/* ③ 括号: 先判非法, 再脱掉整体包围的括号, 0则继续往向下走 */
	int ret = check_parentheses(p, q);
	if (ret == -1) { *success = false; return 0; }
	if (ret ==  1) { return eval(p + 1, q - 1, success); }

	/* ④ 找主运算符. 找不到时, 区间可能是"单目运算符 + 子表达式", 如 "-1" */
	int op = find_main_op(p, q);
	if (op == -1) {
		if (p < q && tokens[p].type == '-') {
			word_t val = eval(p + 1, q, success);
			if (!*success) return 0;
			return 0 - val;                 // 负号: -x 即 0 - x (无符号回绕)
		}
		*success = false;
		return 0;
	}

	/* ⑤ 按主运算符分裂, 递归求两个子式, 再合并 */
	word_t val1 = eval(p, op - 1, success);
	if (!*success) return 0;
	word_t val2 = eval(op + 1, q, success);
	if (!*success) return 0;

	switch (tokens[op].type) {
		case '+': return val1 + val2;
		
		case '-': return val1 - val2;
		case '*': return val1 * val2;
		case '/':
			if (val2 == 0) { *success = false; return 0; }   // 除 0, 非法
			return val1 / val2;
		default: assert(0);
	}

	return 0;
}
// =====================================================================================================


// word_t 定义为 uint32_t/uint64_t -> ./include/common.h:38:typedef MUXDEF(CONFIG_ISA64, uint64_t, uint32_t) word_t;
word_t expr(char *e, bool *success) {
	if (!make_token(e)) {
		*success = false;
		return 0;
	}

	/* TODO: Insert codes to evaluate the expression. */
	*success = true;		// 默认成功; 只在 eval 出错时重新置 false
	return eval(0, nr_token - 1, success);
}
