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

#include "sdb.h"

typedef struct watchpoint {
	int NO;
	struct watchpoint *next;

	/* TODO: Add more members if necessary */
	char expr[128];   // 存储被监视的表达式
	word_t old_val;   // 存储表达式最近的值
} WP;

#define NR_WP 32
static WP wp_pool[NR_WP] = {};
static WP *head = NULL, *free_ = NULL;	// head free_ 都是静态 WP* 类型的指针

void init_wp_pool() {
	for (int i = 0; i < NR_WP; i ++) {
		wp_pool[i].NO = i;
		wp_pool[i].next = (i == NR_WP - 1 ? NULL : &wp_pool[i + 1]);
	}
	head = NULL;
	free_ = wp_pool;    // 不应忘记: 数组赋值/计算，会自动变成数组首元素的地址。也即 free_ = &wp_pool[0];
/*
 * init之后的内存布局:
 * wp_pool: [WP0] → [WP1] → [WP2] → ... → [WP31] → NULL
 *            ↑
 *      free_ 指向这里	 head 指向 → NULL
 *
 * (和数据结构课的链表的区别在于，后者要用malloc开辟新节点，而nemu的链表节点直接通过free_指针开辟)
 */
}

/* TODO: Implement the functionality of watchpoint */
static WP* new_wp() {
	assert(free_ != NULL && "Watchpoints pool is full!");

	WP* wp = free_;			// 将free_指针赋值给wp,如果用传统链表的角度来看,这一步是malloc出内存空间后强转成WP*指针赋值给wp
	free_ = free_->next;	// 可用节点(内存)空间往后移动

	wp->next = head;		// 头插法插入新节点
	head = wp;
	return wp;
/*
 * 第一次调用 new_wp() 后的内存布局:
 * wp_pool: NULL ← [WP0]   [WP1] → [WP2] → ... → [WP31] → NULL
 *               	 ↑       ↑
 *           		head    free_
 *
 * 全程无非是head wp free_三个指针的交换游戏, 共四步(必须按顺序):
 *		1. 将 free_ 赋值给寄存变量 wp
 * 		2. free_ 指向下一个节点 free_->next
 * 		3. wp 的下一个节点指向 head
 * 		4. head 指向 wp
 */
}

void free_wp(WP *wp){
/*
 * 共四步(必须按顺序), 和新建节点反过来四步走(不过为了支持随机释放则需要更多考量):
 * 		1. 找到待释放的节点 wp 及其前置节点 prev
 *			a. (待释放的节点是head指向的节点) head 指向 wp 的下一个节点
 *			b. (待释放的是中间节点) prev 的下一节点指向 wp 的下一节点
 * 		2. wp 的下一个节点指向 free_
 * 		3. free_ 指向 wp
 */
	WP* prev = NULL;
	for (WP* p = head; p != NULL; p = p->next){
		if (p == wp){
			if (prev != NULL)
				prev->next = wp->next;
			else 
				head = wp->next; // 头指针指向的节点就是要释放的那个
			
			wp->next = free_;
			free_ = wp;
			return;
		}
		prev = p;
	}

	// 企图释放一个还没被开辟出来的链表节点, 调用者应该保证不可能发生该事件
	assert(0 && "Freed a linked list node which has not been allocated.");
}
