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
#include <cpu/cpu.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "sdb.h"
#include <memory/vaddr.h>

static int is_batch_mode = false;

void init_regex();
void init_wp_pool();

/* We use the `readline' library to provide more flexibility to read from stdin. */
static char* rl_gets() {
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
  Log("Exit nemu!");
  nemu_state.state = NEMU_QUIT;
  return -1;
}

/* p EXPR - 打印表达式的值 */
static int cmd_p(char *args) {
    // 直接用整个 args(无需经 strtok),因为expr内部已经进行了空格处理
    if (args == NULL) {
        printf("Usage: p EXPR\n");
        return 0;
    }

    bool success = false;
    word_t val = expr(args, &success);

    if (success) printf("%u (0x%x)\n", val, val);
    else         printf("Bad expression\n");

    return 0;
}

static int cmd_si(char* args){
	char* arg = strtok(NULL, " ");
	int n = 1;
	if (arg){
		char* end = NULL;
		n = (int) strtol(arg, &end, 10);
		if (n < 0 || *end){
			printf("Usage: si [N], where N is a non-negative integer\n");
			return 0;
		}
	}

	cpu_exec(n);
	return 0;
}

static int cmd_info(char* args){
	char* arg = strtok(NULL, " ");
	if (arg && !strcmp(arg, "r")){
		isa_reg_display();
		return 0;
	}

	printf("Usage: info r\n");
	return 0;
}

static int cmd_x(char *args) {
    char *arg_n    = strtok(NULL, " ");
    char *arg_addr = strtok(NULL, " ");

    if (arg_n == NULL || arg_addr == NULL) {
        printf("Usage: x N EXPR, where EXPR is an expression (e.g. 0x80000000, $pc)\n");
        return 0;
    }

    char *end = NULL;
    int n = strtol(arg_n, &end, 10);
    if (*end != '\0' || n < 0) {
        printf("N should be a non-negative integer\n");
        return 0;
    }

    bool success = false;
    vaddr_t addr = (vaddr_t)expr(arg_addr, &success);
    if (!success) {
        printf("Bad expression: %s\n", arg_addr);
        return 0;
    }

    for (int i = 0; i < n; i++) {
        if (i % 4 == 0) printf(FMT_WORD ":", addr + i * 4);
        printf(" " FMT_WORD, vaddr_read(addr + i * 4, 4));
        if (i % 4 == 3) printf("\n");
    }

	// 补上最后一行不满 4 个时的换行
    if (n % 4 != 0) printf("\n");

    return 0;
}

static int cmd_help(char *args);

static struct {
  const char *name;
  const char *description;
  int (*handler) (char *);
} cmd_table [] = {
  { "help", "Display information about all supported commands", cmd_help },
  { "c", "Continue the execution of the program", cmd_c },
  { "q", "Exit NEMU", cmd_q },
  { "si", "Execute N instructions step by step (default: 1)", cmd_si },
  { "info", "Display information: info r for registers", cmd_info },
  { "x", "Scan N words of memory starting from EXPR: x N EXPR", cmd_x },
  { "p", "Evaluate the expression EXPR: p EXPR", cmd_p },
  /* TODO: Add more commands */

};

#define NR_CMD ARRLEN(cmd_table)

static int cmd_help(char *args) {
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  int i;

  if (arg == NULL) {
    /* no argument given */
    for (i = 0; i < NR_CMD; i ++) {
      printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  }
  else {
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(arg, cmd_table[i].name) == 0) {
        printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    printf("Unknown command '%s'\n", arg);
  }
  return 0;
}

/* 批量表达式测试: 读取 gen-expr 生成的用例文件 (每行 "结果 表达式"),
 * 调用 expr() 求值并与期望结果比对. */
void sdb_expr_test(char *file) {
    /*
     * 默认用 stdin 作为输入.后续调用表达式求值的测试则可以直接:
     * ./tools/gen-expr/build/gen-expr 10 | ./build/riscv32-nemu-interpreter -e
     */
    FILE *fp = stdin;

    if (strcmp(file, "-") != 0){
        fp = fopen(file, "r");
    }
    Assert(fp, "Can not open '%s'", file);

    char line[4096];
    int pass = 0, fail = 0;

    /*
     * fgets(char* line[], int MAX_TOKEN, FILE* fp):
     * 从 fp 里读取一整行（遇到 \n 或缓冲区满为止）
     * 读到的内容（包括末尾的 \n）存入 line（最多读MAX_TOKEN个字符）
     * 成功返回 line 指针，失败/到文件尾返回 NULL
     */
    while (fgets(line, sizeof(line), fp)) {
        /* 
         * 第一个空格前是期望值, 之后全部是表达式。结构是: result expr\n
         * strchr 用于查找字符串中指定字符并返回该字符的指针
         * (表达式内部可能含空格, 所以不能用 sscanf("%u %s") 拆) 
         */
        char *space = strchr(line,  ' ');
        char *endl  = strchr(line, '\n');
        if (space == NULL || endl == NULL) continue;

        *space = '\0';
        word_t expect = (word_t)strtoul(line, NULL, 10);

        *endl = '\0';     // 去掉 fgets 留下的行尾换行
        char *e = space + 1;

        bool success = false;
        word_t val = expr(e, &success);

        if (success && val == expect) {
            pass++;
        } else {
            fail++;
            printf(ANSI_FMT("FAIL: %s = %u (expect %u)\n", ANSI_FG_RED), e, val, expect);
        }
    }

    fclose(fp);
    printf(ANSI_FMT("expr test: %d passed, %d failed\n", ANSI_FG_GREEN), pass, fail);
    exit(0);        // 测完直接退出, 不进交互界面
}

void sdb_set_batch_mode() {
  is_batch_mode = true;
}

void sdb_mainloop() {
  if (is_batch_mode) {
    cmd_c(NULL);
    return;
  }

  for (char *str; (str = rl_gets()) != NULL; ) {
    char *str_end = str + strlen(str);

    /* extract the first token as the command */
    char *cmd = strtok(str, " ");
    if (cmd == NULL) { continue; }

    /* treat the remaining string as the arguments,
     * which may need further parsing
     */
    char *args = cmd + strlen(cmd) + 1;
    if (args >= str_end) {
      args = NULL;
    }

#ifdef CONFIG_DEVICE
    extern void sdl_clear_event_queue();
    sdl_clear_event_queue();
#endif

    int i;
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(cmd, cmd_table[i].name) == 0) {
        if (cmd_table[i].handler(args) < 0) { return; }
        break;
      }
    }

    if (i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
  }
}

void init_sdb() {
  /* Compile the regular expressions. */
  init_regex();

  /* Initialize the watchpoint pool. */
  init_wp_pool();
}
