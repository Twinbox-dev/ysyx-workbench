/// csrc/top-main.cpp
// 8 位 LFSR 伪随机数发生器：仿真测试 + nvboard 展示
//
// 用法：
//   make DIR=6_shifter MODULE=top VSRC="lfsr.v top.v" sim
//   make DIR=6_shifter MODULE=top VSRC="lfsr.v top.v" run
//
// nvboard 操作：
//   SW15 拨到 1 解除复位，然后反复点 BTNC，看数码管和 LED 的变化
//   SW14 拨到 1 可把 SW7~SW0 装入作为新初值

#include <cstdio>
#include <cstring>
#include <cassert>
#include <cstdint>
#include <Vtop.h>

#ifndef SIM_ONLY
#include <nvboard.h>
#endif

static Vtop *dut = new Vtop;

static void single_cycle() {
    dut->clk = 0; dut->eval();
    dut->clk = 1; dut->eval();
}

static void reset(int n) {
    dut->rst_n = 0;
    while (n-- > 0) single_cycle();
    dut->rst_n = 1;
    dut->eval();
}

#ifdef SIM_ONLY

// 与 top.v 里实例化时的参数一致
static const int STABLE_CNT = 50000;

static int pass = 0, fail = 0;

static void check(long actual, long expect, const char *what) {
    if (actual != expect) {
        printf("FAIL: %-42s got=%ld expect=%ld\n", what, actual, expect);
        fail++;
    } else {
        pass++;
    }
}

static void check_once(bool ok, const char *what, int *reported) {
    if (!ok) {
        if (*reported == 0) { printf("FAIL: %s\n", what); fail++; }
        (*reported)++;
    }
}

// ===== 软件参考模型：独立实现讲义的反馈式 =====
// x8 = x4 ^ x3 ^ x2 ^ x0，右移
static uint8_t ref_step(uint8_t s) {
    int x0 = (s >> 0) & 1;
    int x2 = (s >> 2) & 1;
    int x3 = (s >> 3) & 1;
    int x4 = (s >> 4) & 1;
    int fb = x4 ^ x3 ^ x2 ^ x0;
    return (uint8_t)((s >> 1) | (fb << 7));
}

// ===== 按钮操作：按下并松开，产生一个 step 脉冲 =====
// 消抖模块要求电平稳定 STABLE_CNT 拍才认可，所以每个电平都要保持足够久
static void press_button() {
    dut->btn = 1;
    for (int i = 0; i < STABLE_CNT + 10; i++) single_cycle();
    dut->btn = 0;
    for (int i = 0; i < STABLE_CNT + 10; i++) single_cycle();
}

// ===== 段码反解，用于检查数码管显示 =====
static const uint8_t seg_pattern[16] = {
    0x7E, 0x30, 0x6D, 0x79, 0x33, 0x5B, 0x5F, 0x70,
    0x7F, 0x7B, 0x77, 0x1F, 0x4E, 0x3D, 0x4F, 0x47
};

static int seg_decode(uint8_t seg) {
    uint8_t pat = (uint8_t)((~seg >> 1) & 0x7F);
    for (int i = 0; i < 16; i++) if (seg_pattern[i] == pat) return i;
    return -1;
}

static int shown_hex() {
    int hi = seg_decode(dut->seg1);
    int lo = seg_decode(dut->seg0);
    return (hi < 0 || lo < 0) ? -1 : (hi << 4 | lo);
}

// =====================================================================
// 测试 1：复位后的初值，以及讲义给出的示例序列
//
// 讲义原文（初值 00000001）：
//   00000001 -> 10000000 -> 01000000 -> 00100000 -> 00010000 -> 10001000 -> ...
// 这里逐拍核对，确认反馈式实现与讲义一致。
// =====================================================================
static void test_lecture_sequence() {
    printf("----- lecture example sequence -----\n");

    dut->btn = 0;
    dut->load = 0;
    dut->din = 0;
    reset(4);

    check(dut->q, 0x01, "reset value == 0x01");

    // 讲义列出的前 6 个状态
    const uint8_t expect[6] = {
        0b00000001,
        0b10000000,
        0b01000000,
        0b00100000,
        0b00010000,
        0b10001000,
    };

    for (int i = 0; i < 6; i++) {
        if (dut->q != expect[i]) {
            printf("FAIL: state %d mismatch\n", i);
            printf("      got  = ");
            for (int b = 7; b >= 0; b--) printf("%d", (dut->q >> b) & 1);
            printf("\n      want = ");
            for (int b = 7; b >= 0; b--) printf("%d", (expect[i] >> b) & 1);
            printf("\n");
            fail++;
            return;
        }
        pass++;
        if (i < 5) press_button();
    }
    printf("  first 6 states match the lecture note\n");
}

// =====================================================================
// 测试 2：周期必须是 255，且遍历所有非零状态
//
// 这是讲义的核心要求（2^8 - 1 = 255）。做法是走 255 步看是否回到初值，
// 同时用一张表记录访问过的状态，确认 255 个非零状态每个恰好出现一次。
// =====================================================================
static void test_period_255() {
    printf("----- period and coverage -----\n");

    dut->btn = 0;
    dut->load = 0;
    dut->din = 0;
    reset(4);

    uint8_t start = dut->q;
    int visit[256];
    memset(visit, 0, sizeof(visit));

    visit[start]++;
    for (int i = 0; i < 255; i++) {
        press_button();
        visit[dut->q]++;
    }

    // 走完 255 步应该回到起点
    check(dut->q, start, "returns to start after 255 steps");

    // 全零状态绝不能出现
    check(visit[0], 0, "all-zero state never visited");

    // 255 个非零状态各出现一次（起点被记了两次：开头和结尾）
    int seen_once = 0, wrong = 0;
    for (int s = 1; s < 256; s++) {
        int want = (s == start) ? 2 : 1;
        if (visit[s] == want) seen_once++;
        else wrong++;
    }
    check(seen_once, 255, "all 255 nonzero states visited exactly once");
    check(wrong, 0, "no state visited an unexpected number of times");
}

// =====================================================================
// 测试 3：与软件参考模型逐步比对
//
// 期望值由独立实现的 ref_step() 算出，不从 DUT 读取——否则恒等成立、
// 什么也没验证。
// =====================================================================
static void test_against_reference() {
    printf("----- reference model -----\n");

    dut->btn = 0;
    dut->load = 0;
    dut->din = 0;
    reset(4);

    uint8_t model = dut->q;
    int err = 0;

    for (int i = 0; i < 300; i++) {     // 跑超过一个周期
        press_button();
        model = ref_step(model);
        check_once(dut->q == model, "DUT diverges from reference model", &err);
    }

    if (err == 0) { pass++; printf("  300 steps match the reference model\n"); }
    else printf("      (mismatches: %d)\n", err);
}

// =====================================================================
// 测试 4：数码管显示的十六进制值正确
// =====================================================================
static void test_seg_display() {
    printf("----- seven-segment display -----\n");

    dut->btn = 0;
    dut->load = 0;
    dut->din = 0;
    reset(4);

    int err = 0;
    for (int i = 0; i < 64; i++) {
        check_once(shown_hex() == dut->q, "seg display != q", &err);
        press_button();
    }
    if (err == 0) { pass++; printf("  64 states displayed correctly\n"); }

    // 抽查几个具体值：装入后数码管应立刻显示对应的十六进制
    struct { uint8_t v; } cases[] = { {0xA5}, {0x3C}, {0xFF}, {0x01}, {0x80} };
    for (unsigned i = 0; i < sizeof(cases)/sizeof(cases[0]); i++) {
        dut->load = 1;
        dut->din  = cases[i].v;
        single_cycle();
        dut->load = 0;
        dut->eval();
        char buf[48];
        snprintf(buf, sizeof(buf), "load 0x%02X -> display", cases[i].v);
        check(shown_hex(), cases[i].v, buf);
    }
}

// =====================================================================
// 测试 5：装入功能，以及全零锁死的规避
//
// 讲义提醒全零是锁死状态。这里验证两件事：
//   - 装入非零值时正常工作
//   - 装入全零时被替换成 SEED，不会锁死
// =====================================================================
static void test_load_and_zero_lockup() {
    printf("----- load / zero lock-up -----\n");

    dut->btn = 0;
    dut->load = 0;
    dut->din = 0;
    reset(4);

    // 装入一个非零值，之后应按反馈式演进
    dut->load = 1;
    dut->din  = 0xA5;
    single_cycle();
    dut->load = 0;
    dut->eval();
    check(dut->q, 0xA5, "load 0xA5");

    uint8_t model = 0xA5;
    for (int i = 0; i < 10; i++) {
        press_button();
        model = ref_step(model);
    }
    check(dut->q, model, "evolves correctly after load");

    // 试图装入全零：应被替换成 SEED(0x01)，且 zero_flag 不亮
    dut->load = 1;
    dut->din  = 0x00;
    single_cycle();
    dut->load = 0;
    dut->eval();
    check(dut->q, 0x01, "load 0x00 is replaced by SEED");
    check(dut->zero_flag, 0, "zero_flag not set");

    // 确认之后仍能正常前进（没有锁死）
    press_button();
    check(dut->q, ref_step(0x01), "still advancing after zero load attempt");

    // 整个周期内 zero_flag 都不应亮起
    reset(4);
    int err = 0;
    for (int i = 0; i < 255; i++) {
        check_once(dut->zero_flag == 0, "zero_flag set during normal operation", &err);
        press_button();
    }
    if (err == 0) { pass++; printf("  zero_flag stays low for a full period\n"); }
}

// =====================================================================
// 测试 6：按钮消抖
//
// 讲义要求"用按钮作为时钟"，但机械按钮有弹跳。这里模拟弹跳，验证一次按下
// 只前进一步。
// =====================================================================
static void test_button_debounce() {
    printf("----- button debounce -----\n");

    dut->btn = 0;
    dut->load = 0;
    dut->din = 0;
    reset(4);

    uint8_t before = dut->q;

    // 模拟触点弹跳：短促地抖动若干次（每次都远短于 STABLE_CNT）
    for (int i = 0; i < 20; i++) {
        dut->btn = 1;
        for (int k = 0; k < 100; k++) single_cycle();
        dut->btn = 0;
        for (int k = 0; k < 100; k++) single_cycle();
    }
    check(dut->q, before, "bouncing alone does not advance");

    // 然后是一次真正的按下（保持足够久）
    dut->btn = 1;
    for (int i = 0; i < STABLE_CNT + 10; i++) single_cycle();
    check(dut->q, ref_step(before), "one clean press advances exactly one step");

    // 一直按住不放，不应连续前进（只在上升沿动一次）
    uint8_t held = dut->q;
    for (int i = 0; i < STABLE_CNT * 3; i++) single_cycle();
    check(dut->q, held, "holding the button does not repeat");

    dut->btn = 0;
    for (int i = 0; i < STABLE_CNT + 10; i++) single_cycle();
    check(dut->q, held, "releasing does not advance either");
}

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);

    printf("===== 8-bit LFSR Simulation =====\n");

    test_lecture_sequence();
    test_period_255();
    test_against_reference();
    test_seg_display();
    test_load_and_zero_lockup();
    test_button_debounce();

    printf("===== %d passed, %d failed =====\n", pass, fail);
    assert(fail == 0 && "LFSR behaviour mismatch!");

    printf("Simulation done, exit (SIM_ONLY mode).\n");
    return 0;
}

#else   // ===================== nvboard =====================

int main() {
    // 输入
    nvboard_bind_pin(&dut->btn,   1, BTNC);    // 前进一步
    nvboard_bind_pin(&dut->rst_n, 1, SW15);    // 复位（拨到 0 复位）
    nvboard_bind_pin(&dut->load,  1, SW14);    // 装入初值
    nvboard_bind_pin(&dut->din,   8, SW7, SW6, SW5, SW4, SW3, SW2, SW1, SW0);

    // 输出：8 位状态同时用 LED（二进制）和数码管（十六进制）显示
    nvboard_bind_pin(&dut->q, 8, LD7, LD6, LD5, LD4, LD3, LD2, LD1, LD0);
    nvboard_bind_pin(&dut->zero_flag, 1, LD15);
    nvboard_bind_pin(&dut->seg0, 8, SEG0A, SEG0B, SEG0C, SEG0D, SEG0E, SEG0F, SEG0G, DEC0P);
    nvboard_bind_pin(&dut->seg1, 8, SEG1A, SEG1B, SEG1C, SEG1D, SEG1E, SEG1F, SEG1G, DEC1P);

    dut->btn  = 0;
    dut->load = 0;
    dut->din  = 0;

    nvboard_init();
    reset(10);

    while (1) {
        nvboard_update();
        single_cycle();
    }

    dut->final();
    return 0;
}

#endif
