// csrc/main.cpp
// 实验七 PS/2 键盘：仿真测试 + nvboard 展示
//
// 用法：
//   make DIR=7_keyboard sim   # 纯仿真自测（内建 PS/2 键盘行为模型）
//   make DIR=7_keyboard run   # nvboard 图形版本，用真实键盘输入
//
// 七段数码管：
//   seg1 seg0 = 当前键码, seg3 seg2 = ASCII, seg5 seg4 = 按键次数
// LED：
//   LD0=Shift LD1=Ctrl LD2=Alt LD3=CapsLock LD4=ready LD5=overflow

#include <cstdio>
#include <cassert>
#include <cstdint>
#include <Vtop.h>

#ifndef SIM_ONLY
#include <nvboard.h>
#endif

static Vtop *dut = new Vtop;

// ===== 时钟驱动 =====
static void single_cycle() {
    dut->clk = 0; dut->eval();
    dut->clk = 1; dut->eval();
}

static void reset(int n) {
    dut->rst = 1;
    while (n-- > 0) single_cycle();
    dut->rst = 0;
    dut->eval();
}

#ifdef SIM_ONLY

// =====================================================================
// PS/2 键盘行为模型
//
// 对应 Listing 23 中的 ps2_keyboard_model：把一个字节按
// 起始位 + 8 位数据(低位在前) + 奇校验 + 停止位 的顺序串行送出，
// 每位在 ps2_clk 的下降沿被 DUT 采样。
// =====================================================================

// 每个 PS/2 半周期占用的 clk 周期数（真实键盘远慢于此，这里为加速仿真）
static const int PS2_HALF_PERIOD = 8;

static void ps2_idle(int cycles) {
    dut->ps2_clk  = 1;
    dut->ps2_data = 1;
    while (cycles-- > 0) single_cycle();
}

// 送出一位：先在 ps2_clk 高电平期间建立数据，再拉低产生下降沿
static void ps2_send_bit(int bit) {
    dut->ps2_data = bit;
    dut->ps2_clk  = 1;
    for (int i = 0; i < PS2_HALF_PERIOD; i++) single_cycle();
    dut->ps2_clk  = 0;
    for (int i = 0; i < PS2_HALF_PERIOD; i++) single_cycle();
}

// 送出一个完整的 11 位数据帧
static void ps2_send_byte(uint8_t code) {
    int parity = 1;                       // 奇校验：数据位与校验位中 1 的个数为奇数
    for (int i = 0; i < 8; i++) parity ^= (code >> i) & 1;

    ps2_send_bit(0);                      // 起始位
    for (int i = 0; i < 8; i++) ps2_send_bit((code >> i) & 1);
    ps2_send_bit(parity);                 // 奇校验位
    ps2_send_bit(1);                      // 停止位

    ps2_idle(PS2_HALF_PERIOD * 4);        // 帧间空闲，同时给 DUT 处理时间
}

// 按下一个普通键（送通码）
static void key_press(uint8_t code) {
    ps2_send_byte(code);
}

// 放开一个普通键（送断码 F0 + 通码）
static void key_release(uint8_t code) {
    ps2_send_byte(0xF0);
    ps2_send_byte(code);
}

// 按下 / 放开一个扩展键（E0 前缀）
static void key_press_ext(uint8_t code) {
    ps2_send_byte(0xE0);
    ps2_send_byte(code);
}

static void key_release_ext(uint8_t code) {
    ps2_send_byte(0xE0);
    ps2_send_byte(0xF0);
    ps2_send_byte(code);
}

// ===== 从段码反解出十六进制数字，用于检查数码管显示 =====
// seg = {~A,~B,~C,~D,~E,~F,~G, 1}，低有效
static const uint8_t seg_pattern[16] = {
    0x7E, 0x30, 0x6D, 0x79, 0x33, 0x5B, 0x5F, 0x70,
    0x7F, 0x7B, 0x77, 0x1F, 0x4E, 0x3D, 0x4F, 0x47
};

// 返回 0~15 表示显示的数字，-1 表示熄灭，-2 表示无法识别
static int seg_decode(uint8_t seg) {
    if (seg == 0xFF) return -1;           // 全灭
    uint8_t pat = (~seg >> 1) & 0x7F;     // 取反并去掉小数点位
    for (int i = 0; i < 16; i++) {
        if (seg_pattern[i] == pat) return i;
    }
    return -2;
}

static int pass = 0, fail = 0;

static void check(int actual, int expect, const char *what) {
    if (actual != expect) {
        printf("FAIL: %-28s got=%d expect=%d\n", what, actual, expect);
        fail++;
    } else {
        pass++;
    }
}

static void check_hex(uint8_t hi_seg, uint8_t lo_seg, int expect, const char *what) {
    int hi = seg_decode(hi_seg);
    int lo = seg_decode(lo_seg);
    int val = (hi < 0 || lo < 0) ? -1 : (hi << 4 | lo);
    if (val != expect) {
        printf("FAIL: %-28s got=0x%02X expect=0x%02X\n", what, val, expect);
        fail++;
    } else {
        pass++;
    }
}

// 当前显示的键码 / ASCII / 计数，-1 表示熄灭
static int shown_code()  { int h = seg_decode(dut->seg1), l = seg_decode(dut->seg0);
                           return (h < 0 || l < 0) ? -1 : (h << 4 | l); }
static int shown_ascii() { int h = seg_decode(dut->seg3), l = seg_decode(dut->seg2);
                           return (h < 0 || l < 0) ? -1 : (h << 4 | l); }
static int shown_count() { int h = seg_decode(dut->seg5), l = seg_decode(dut->seg4);
                           return (h < 0 || l < 0) ? -1 : (h << 4 | l); }

// LED 位定义
static const uint8_t LED_SHIFT = 1 << 0;
static const uint8_t LED_CTRL  = 1 << 1;
static const uint8_t LED_ALT   = 1 << 2;
static const uint8_t LED_CAPS  = 1 << 3;
static const uint8_t LED_OVFL  = 1 << 5;

static void run_sim_test() {
    printf("===== PS/2 Keyboard Simulation =====\n");

    reset(10);
    ps2_idle(20);

    check(shown_code(),  -1, "reset: code blank");
    check(shown_ascii(), -1, "reset: ascii blank");
    check(shown_count(),  0, "reset: count = 0");

    // ---- 按下并放开 'A'（通码 1C，小写 ASCII 0x61）----
    key_press(0x1C);
    check_hex(dut->seg1, dut->seg0, 0x1C, "press A: code");
    check_hex(dut->seg3, dut->seg2, 0x61, "press A: ascii 'a'");
    check(shown_count(), 1, "press A: count = 1");

    key_release(0x1C);
    check(shown_code(),  -1, "release A: code blank");
    check(shown_ascii(), -1, "release A: ascii blank");
    check(shown_count(),  1, "release A: count stays 1");

    // ---- 按住不放：重复通码只算一次按键 ----
    key_press(0x1B);                      // 'S'
    check_hex(dut->seg1, dut->seg0, 0x1B, "press S: code");
    check_hex(dut->seg3, dut->seg2, 0x73, "press S: ascii 's'");
    check(shown_count(), 2, "press S: count = 2");

    key_press(0x1B);                      // typematic repeat
    key_press(0x1B);
    key_press(0x1B);
    check(shown_count(), 2, "hold S: count still 2");
    check_hex(dut->seg1, dut->seg0, 0x1B, "hold S: code unchanged");

    key_release(0x1B);
    check(shown_code(), -1, "release S: code blank");
    check(shown_count(), 2, "release S: count stays 2");

    // ---- 数字键 '1' -> ASCII 0x31 ----
    key_press(0x16);
    check_hex(dut->seg3, dut->seg2, 0x31, "press 1: ascii '1'");
    check(shown_count(), 3, "press 1: count = 3");
    key_release(0x16);

    // ---- Shift 组合键：LED 指示 + 大写字母 ----
    key_press(0x12);                                   // 左 Shift 按下
    check((dut->led & LED_SHIFT) ? 1 : 0, 1, "LShift down: LED on");
    check(shown_count(), 3, "LShift down: not counted");
    check(shown_code(), -1, "LShift down: code blank");

    key_press(0x1C);                                   // Shift + A -> 'A' = 0x41
    check_hex(dut->seg1, dut->seg0, 0x1C, "Shift+A: code");
    check_hex(dut->seg3, dut->seg2, 0x41, "Shift+A: ascii 'A'");
    check(shown_count(), 4, "Shift+A: count = 4");
    check((dut->led & LED_SHIFT) ? 1 : 0, 1, "Shift+A: LED still on");

    key_release(0x1C);
    check(shown_code(), -1, "Shift+A release: blank");
    check((dut->led & LED_SHIFT) ? 1 : 0, 1, "Shift+A release: Shift held");

    key_press(0x16);                                   // Shift + 1 -> '!' = 0x21
    check_hex(dut->seg3, dut->seg2, 0x21, "Shift+1: ascii '!'");
    key_release(0x16);

    key_release(0x12);                                 // 左 Shift 放开
    check((dut->led & LED_SHIFT) ? 1 : 0, 0, "LShift up: LED off");

    // ---- 右 Shift 同样有效 ----
    key_press(0x59);
    check((dut->led & LED_SHIFT) ? 1 : 0, 1, "RShift down: LED on");
    key_press(0x1A);                                   // Shift + Z -> 'Z' = 0x5A
    check_hex(dut->seg3, dut->seg2, 0x5A, "Shift+Z: ascii 'Z'");
    key_release(0x1A);
    key_release(0x59);
    check((dut->led & LED_SHIFT) ? 1 : 0, 0, "RShift up: LED off");

    // ---- 左 Ctrl / 左 Alt ----
    key_press(0x14);
    check((dut->led & LED_CTRL) ? 1 : 0, 1, "LCtrl down: LED on");
    key_release(0x14);
    check((dut->led & LED_CTRL) ? 1 : 0, 0, "LCtrl up: LED off");

    key_press(0x11);
    check((dut->led & LED_ALT) ? 1 : 0, 1, "LAlt down: LED on");
    key_release(0x11);
    check((dut->led & LED_ALT) ? 1 : 0, 0, "LAlt up: LED off");

    // ---- 右 Ctrl / 右 Alt：扩展键，E0 前缀区分 ----
    key_press_ext(0x14);
    check((dut->led & LED_CTRL) ? 1 : 0, 1, "RCtrl down: LED on");
    key_release_ext(0x14);
    check((dut->led & LED_CTRL) ? 1 : 0, 0, "RCtrl up: LED off");

    key_press_ext(0x11);
    check((dut->led & LED_ALT) ? 1 : 0, 1, "RAlt down: LED on");
    key_release_ext(0x11);
    check((dut->led & LED_ALT) ? 1 : 0, 0, "RAlt up: LED off");

    // ---- Caps Lock：按一次翻转锁定状态，字母变大写 ----
    int cnt_before_caps = shown_count();
    key_press(0x58);
    key_release(0x58);
    check((dut->led & LED_CAPS) ? 1 : 0, 1, "CapsLock on: LED on");
    check(shown_count(), cnt_before_caps, "CapsLock: not counted");

    key_press(0x1C);                                   // Caps + A -> 'A'
    check_hex(dut->seg3, dut->seg2, 0x41, "Caps: ascii 'A'");
    key_release(0x1C);

    key_press(0x16);                                   // Caps 不影响数字键
    check_hex(dut->seg3, dut->seg2, 0x31, "Caps: digit unaffected");
    key_release(0x16);

    key_press(0x12);                                   // Caps + Shift + A -> 'a'
    key_press(0x1C);
    check_hex(dut->seg3, dut->seg2, 0x61, "Caps+Shift: ascii 'a'");
    key_release(0x1C);
    key_release(0x12);

    key_press(0x58);                                   // 再按一次关闭 Caps
    key_release(0x58);
    check((dut->led & LED_CAPS) ? 1 : 0, 0, "CapsLock off: LED off");

    // ---- Caps Lock 按住不放：只翻转一次 ----
    key_press(0x58);
    check((dut->led & LED_CAPS) ? 1 : 0, 1, "CapsLock hold: toggled once");
    key_press(0x58);                                   // 重复通码
    key_press(0x58);
    check((dut->led & LED_CAPS) ? 1 : 0, 1, "CapsLock hold: still on");
    key_release(0x58);
    check((dut->led & LED_CAPS) ? 1 : 0, 1, "CapsLock hold: on after release");
    key_press(0x58);                                   // 再按一次关闭
    key_release(0x58);
    check((dut->led & LED_CAPS) ? 1 : 0, 0, "CapsLock: off again");

    // ---- 非字符键：键码显示，ASCII 熄灭 ----
    key_press_ext(0x75);                               // 方向键 上
    check_hex(dut->seg1, dut->seg0, 0x75, "Up key: code shown");
    check(shown_ascii(), -1, "Up key: ascii blank");
    key_release_ext(0x75);

    key_press(0x05);                                   // F1
    check_hex(dut->seg1, dut->seg0, 0x05, "F1: code shown");
    check(shown_ascii(), -1, "F1: ascii blank");
    key_release(0x05);

    // ---- ASCII 表抽查 ----
    struct { uint8_t code; uint8_t plain; uint8_t shifted; const char *name; } tbl[] = {
        { 0x15, 0x71, 0x51, "Q" },
        { 0x4D, 0x70, 0x50, "P" },
        { 0x4B, 0x6C, 0x4C, "L" },
        { 0x45, 0x30, 0x29, "0" },
        { 0x3E, 0x38, 0x2A, "8" },
        { 0x29, 0x20, 0x20, "Space" },
        { 0x5A, 0x0D, 0x0D, "Enter" },
        { 0x4E, 0x2D, 0x5F, "Minus" },
        { 0x4C, 0x3B, 0x3A, "Semicolon" },
        { 0x4A, 0x2F, 0x3F, "Slash" },
    };

    for (unsigned i = 0; i < sizeof(tbl) / sizeof(tbl[0]); i++) {
        key_press(tbl[i].code);
        check_hex(dut->seg3, dut->seg2, tbl[i].plain, tbl[i].name);
        key_release(tbl[i].code);

        key_press(0x12);
        key_press(tbl[i].code);
        check_hex(dut->seg3, dut->seg2, tbl[i].shifted, tbl[i].name);
        key_release(tbl[i].code);
        key_release(0x12);
    }

    // ---- 计数递增：连续按 5 个键 ----
    int base = shown_count();
    const uint8_t seq[5] = { 0x1C, 0x1B, 0x23, 0x2B, 0x34 };  // A S D F G
    for (int i = 0; i < 5; i++) {
        key_press(seq[i]);
        key_release(seq[i]);
        check(shown_count(), base + i + 1, "sequence: count increments");
    }

    // ---- FIFO 未溢出 ----
    check((dut->led & LED_OVFL) ? 1 : 0, 0, "no FIFO overflow");
}

// =====================================================================
// nvboard 键盘模型复现
//
// nvboard 的 KEYBOARD::update_state() 并不是"空闲高电平"的模型，而是每
// CLK_NUM 个周期把 PS2_CLK 翻转一次：
//   翻转成高（上升沿）时放置一个数据位；
//   翻转成低（下降沿）时什么都不做，正是留给 DUT 采样的时刻。
//
// 因此 ps2_clk 的初值决定了第一个边沿是上升还是下降：初值为 0 时第一个
// 边沿是上升沿（先放数据再采样，正确）；初值为 1 时会先产生一个多余的
// 下降沿，让 DUT 多采一位，整个帧永久错位。
//
// 这里按 nvboard 的算法逐字节复现，用来验证顶层在 nvboard 下能正常工作。
// =====================================================================

static const int NV_CLK_NUM = 10;      // 与 nvboard 的 CLK_NUM 一致

static void nv_toggle_wait() {
    for (int i = 0; i < NV_CLK_NUM; i++) single_cycle();
}

// nvboard 送出待发字节队列的方式：每 CLK_NUM 周期翻转 ps2_clk 一次，
// 翻转成高时放置下一个数据位，翻转成低时留给 DUT 采样。
// 注意这里用的是翻转（和 nvboard 一样），而不是直接赋值，这样 ps2_clk
// 的初值才会真实地影响相位。
static void nv_send_frames(const uint8_t *frames, int nr_frames) {
    int data_idx = 0;                  // 对应 nvboard 的 data_idx
    int total_edges = nr_frames * 22;  // 每帧 11 位，每位 2 个边沿

    for (int e = 0; e < total_edges; e++) {
        nv_toggle_wait();
        dut->ps2_clk = !dut->ps2_clk;  // 翻转，和 nvboard 一致

        if (dut->ps2_clk) {
            // 上升沿：放置当前位
            dut->ps2_data = frames[data_idx];
            data_idx++;
        }
        // 下降沿：DUT 在此采样，不改变 ps2_data
        dut->eval();
    }
    nv_toggle_wait();
}

// 把一个字节展开成 11 位的帧
static void nv_build_frame(uint8_t code, uint8_t *out) {
    int parity = 1;
    for (int i = 0; i < 8; i++) parity ^= (code >> i) & 1;
    out[0] = 0;                                          // 起始位
    for (int i = 0; i < 8; i++) out[1 + i] = (code >> i) & 1;
    out[9]  = (uint8_t)parity;                           // 奇校验
    out[10] = 1;                                         // 停止位
}

static void nv_send_byte(uint8_t code) {
    uint8_t f[11];
    nv_build_frame(code, f);
    nv_send_frames(f, 1);
}

static void nv_press(uint8_t code)   { nv_send_byte(code); }
static void nv_release(uint8_t code) { nv_send_byte(0xF0); nv_send_byte(code); }

// 用 nvboard 的模型跑一遍，确认顶层在图形版本下也能正确接收
static void run_nvboard_model_test() {
    printf("----- nvboard keyboard model -----\n");

    // nvboard 模型要求 ps2_clk 从 0 开始（见上面的说明）
    dut->ps2_clk  = 0;
    dut->ps2_data = 1;
    reset(10);

    check(shown_count(), 0, "nv: count = 0 after reset");

    nv_press(0x1C);                                      // 'A'
    check_hex(dut->seg1, dut->seg0, 0x1C, "nv: press A code");
    check_hex(dut->seg3, dut->seg2, 0x61, "nv: press A ascii");
    check(shown_count(), 1, "nv: count = 1");

    nv_release(0x1C);
    check(shown_code(), -1, "nv: release A blank");
    check(shown_count(), 1, "nv: count stays 1");

    nv_press(0x12);                                      // Shift
    check((dut->led & LED_SHIFT) ? 1 : 0, 1, "nv: Shift LED on");
    nv_press(0x1B);                                      // Shift + S -> 'S'
    check_hex(dut->seg3, dut->seg2, 0x53, "nv: Shift+S ascii 'S'");
    nv_release(0x1B);
    nv_release(0x12);
    check((dut->led & LED_SHIFT) ? 1 : 0, 0, "nv: Shift LED off");
    check(shown_count(), 2, "nv: count = 2");

    check((dut->led & LED_OVFL) ? 1 : 0, 0, "nv: no overflow");
}

int main() {
    run_sim_test();
    run_nvboard_model_test();

    printf("===== total %d passed, %d failed =====\n", pass, fail);
    assert(fail == 0 && "keyboard behaviour mismatch!");

    printf("Simulation done, exit (SIM_ONLY mode).\n");
    return 0;
}

#else   // ===================== nvboard =====================

int main() {
    // ===== nvboard 引脚绑定 =====
    // 输入：PS/2 键盘的两根信号线（由 nvboard 的键盘模型驱动）
    nvboard_bind_pin(&dut->ps2_clk,  1, PS2_CLK);
    nvboard_bind_pin(&dut->ps2_data, 1, PS2_DAT);

    // 输出：LED 状态指示
    nvboard_bind_pin(&dut->led, 8, LD7, LD6, LD5, LD4, LD3, LD2, LD1, LD0);

    // 输出：8 位七段数码管，全部低有效
    nvboard_bind_pin(&dut->seg0, 8, SEG0A, SEG0B, SEG0C, SEG0D, SEG0E, SEG0F, SEG0G, DEC0P);
    nvboard_bind_pin(&dut->seg1, 8, SEG1A, SEG1B, SEG1C, SEG1D, SEG1E, SEG1F, SEG1G, DEC1P);
    nvboard_bind_pin(&dut->seg2, 8, SEG2A, SEG2B, SEG2C, SEG2D, SEG2E, SEG2F, SEG2G, DEC2P);
    nvboard_bind_pin(&dut->seg3, 8, SEG3A, SEG3B, SEG3C, SEG3D, SEG3E, SEG3F, SEG3G, DEC3P);
    nvboard_bind_pin(&dut->seg4, 8, SEG4A, SEG4B, SEG4C, SEG4D, SEG4E, SEG4F, SEG4G, DEC4P);
    nvboard_bind_pin(&dut->seg5, 8, SEG5A, SEG5B, SEG5C, SEG5D, SEG5E, SEG5F, SEG5G, DEC5P);
    nvboard_bind_pin(&dut->seg6, 8, SEG6A, SEG6B, SEG6C, SEG6D, SEG6E, SEG6F, SEG6G, DEC6P);
    nvboard_bind_pin(&dut->seg7, 8, SEG7A, SEG7B, SEG7C, SEG7D, SEG7E, SEG7F, SEG7G, DEC7P);

    nvboard_init();

    // nvboard 的键盘模型每隔固定周期翻转 PS2_CLK，翻转成高时才放置数据位。
    // 所以这里必须让 ps2_clk 从 0 开始：第一个边沿是上升沿（先放数据、后
    // 由 DUT 在下降沿采样）。若初值为 1，第一个边沿会是下降沿，DUT 会多
    // 采一位，整个数据帧永久错位。SIM_ONLY 下的 run_nvboard_model_test()
    // 复现了这一时序并会因错误初值而失败。
    dut->ps2_clk  = 0;
    dut->ps2_data = 1;
    reset(10);

    while (1) {
        nvboard_update();   // 驱动 PS/2 键盘模型、刷新 LED 与数码管
        single_cycle();
    }

    dut->final();
    return 0;
}

#endif
