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
  nemu_state.state = NEMU_QUIT;
  return -1;
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
        printf("Usage: x N EXPR\n");
        return 0;
    }

    char *end = NULL;
    int n = strtol(arg_n, &end, 10);
    if (*end != '\0' || n < 0) {
        printf("N should be a non-negative integer\n");
        return 0;
    }

    vaddr_t addr = (vaddr_t) strtoul(arg_addr, &end, 16);
    if (*end != '\0') {
        printf("Bad address: %s\n", arg_addr);
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
