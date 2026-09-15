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
#include "local-include/reg.h"

const char *regs[] = {
    "$0", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
    "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
    "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7",
    "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"
};

void isa_reg_display() {
	  /*
	   * MUXDEF(CONFIG_RVE, 16, 32): 编译期三目运算符—>CONlFIG_RVE被定义过就取 16,否则取32
	   * nemu/include/common.h:#define FMT_WORD MUXDEF(CONFIG_ISA64, "0x%016" PRIx64, "0x%08"PRIx32)
	   * PRIx64 PRIx32是C语言的预定义宏。具体见后续文档
	   */
    for (int i = 0; i < MUXDEF(CONFIG_RVE, 16, 32); i++) {
        printf("%-4s " FMT_WORD, reg_name(i), gpr(i));
        if (i % 4 == 3) printf("\n");     // 每 4 个换行
        else printf(" ");
    }
    printf("%-4s " FMT_WORD "\n", "pc", cpu.pc);
}

word_t isa_reg_str2val(const char *s, bool *success) {
    /* 兼容 "$a0" 和 "a0" 两种写法 */
    const char *name = (*s == '$') ? s + 1 : s;

    if (strcmp(name, "pc") == 0) {      // pc 不在 regs[] 里, 单独处理
        *success = true;
        return cpu.pc;
    }

    for (int i = 0; i < MUXDEF(CONFIG_RVE, 16, 32); i++) {
        const char *reg = regs[i];
        if (*reg == '$') reg++;         // regs[0] 是 "$0", 去掉前缀再比
        if (strcmp(name, reg) == 0) {
            *success = true;
            return gpr(i);
        }
    }

    *success = false;
    return 0;
}
