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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <string.h>

static int buf_pos = 0;					// 用于指示生成的表达式的当前位置
static char buf[65536] = {};			// this should be enough
static char code_buf[65536 + 128] = {}; // a little larger than `buf` to contain char code_format
// %s 是标准占位符,在sprintf中可以被替换;而%%u是转义后的%,失去了占位含义,是标准的%u字符串
static char *code_format =
"#include <stdio.h>\n"
"int main() { "
"    unsigned result = %s; "
"    printf(\"%%u\", result); "
"    return 0; "
"}";

/* 生成一个小于 n 的随机数 */
static uint32_t choose(uint32_t n) {
	// rand() % n 能产生 0; 但是choose(0)会导致除0错误,所以在此用下面的方式避免除零
    return (n == 0) ? 0 : rand() % n;
}

static void gen(char c) {
    buf[buf_pos++] = c;
}

/* 以 1/3 概率插入一个空格, 测试词法分析对"空格数量不固定"的处理 */
static void maybe_gen_space() {
    if (choose(3) == 0) gen(' ');
}

/* 
 * 生成一个 1 ~ 9999 的十进制数字 (不含 0, 从源头避免除 0).
 * 带 u 后缀以保证整个表达式按无符号运算 —— 否则 gcc 会按 int 算,
 * 溢出行为与 NEMU 的无符号回绕不一致, 期望值本身就是错的. 
 */
static void gen_num() {
    maybe_gen_space();
	// sprintf 复制到 &buf[0] + buf_pos 并在生成之后将指针往后移动
    buf_pos += sprintf(buf + buf_pos, "%uu", choose(9999) + 1);
}

/* 递归生成表达式. depth 限制递归深度, 防止 buf 溢出. */
static void gen_expr(int depth) {
    if (depth > 5) { gen_num(); return; }

    switch (choose(3)) {
        case 0:
            gen_num();
            break;

        case 1:
            gen('(');
            gen_expr(depth + 1);
            gen(')');
            break;

        default: {
            gen_expr(depth + 1);
			// 从一个[0:3]字符数组里面随机取一个char作为运算符
            char op = "+-*/"[choose(4)];
            gen(op);
            if (op == '/') {
                /* 除号的右操作数强制是非 0 数字, 彻底杜绝除 0 - 不在此限制右侧是数字,则有可能出现2/(4-4)的情况 */
                gen_num();
            } else {
                gen_expr(depth + 1);
            }
            break;
        }
    }
}

static void gen_rand_expr() {
	buf_pos = 0;
	gen_expr(0);
	buf[buf_pos] = '\0';
}

int main(int argc, char *argv[]) {
	// c语言中设定随机数种子的标准做法:
	srand(time(NULL));
	int loop = 1;
	if (argc > 1) {
		// 读取argv[1]中的内容并转换成int写入loop变量中
		sscanf(argv[1], "%d", &loop);
	}
	int i;
	for (i = 0; i < loop; i ++) {
		// 表达式生成器,结果写到全局字符数组 buf 中
		gen_rand_expr();

		/*
		 * int sprintf(char *restrict str, const char *restrict format, ...);
		 * buf作为%s替换到code_format中,然后整体赋值到code_buf变量中.
		 * 返回值是复制成功的char数,后续会用到
		 */
		sprintf(code_buf, code_format, buf);

		FILE *fp = fopen("/tmp/.code.c", "w");
		assert(fp != NULL);
		fputs(code_buf, fp);
		fclose(fp);

		int ret = system("gcc /tmp/.code.c -o /tmp/.expr");
		if (ret != 0) continue;

		// popen - pipe open.能通过管道拿到命令行的输出并返回给fp。所以在这里不用system("/tmp/.expr") - 只能执行不能拿到输出
		fp = popen("/tmp/.expr", "r");
		assert(fp != NULL);

		int result;
		ret = fscanf(fp, "%d", &result);
		pclose(fp);
		if(ret != 1) continue;	// ret==1表示成功读到一个字符串,否则丢弃该用例并继续执行循环

		// 打印: 结果 完整表达式
		printf("%u %s\n", result, buf);
	}
	return 0;
}
