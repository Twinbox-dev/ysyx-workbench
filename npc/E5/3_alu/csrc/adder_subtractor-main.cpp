// csrc/adder_substractor-main.cpp
// 4-bit 加法/减法器: 单个 cpp 同时完成「仿真测试」与「nvboard 展示」

#include <cstdio>
#include <cassert>
#include <Vadder_subtractor.h>
#ifndef SIM_ONLY
#include <nvboard.h>
#endif

static Vadder_subtractor *dut = new Vadder_subtractor;

// 参考模型: 复现 RTL 的行为, 用于对照
struct Ref {
    unsigned result, cout, overflow, zero;
};

static Ref ref_model(unsigned sub, unsigned a, unsigned b) {
    unsigned b_in = sub ? (~b & 0xF) : b;   // sub=1 时逐位取反
    unsigned full = a + b_in + sub;         // sub 兼任最低位进位
    unsigned low3 = (a & 0x7) + (b_in & 0x7) + sub;  // 用于取出进入最高位的进位 c3

    Ref r;
    r.result   = full & 0xF;
    r.cout     = (full >> 4) & 1;
    r.overflow = ((low3 >> 3) & 1) ^ r.cout;  // c3 ^ cout
    r.zero     = (r.result == 0);
    return r;
}

// ===== 仿真测试: 穷举 sub ∈ {0,1}, a, b ∈ [0, 15], 共 512 组 =====
static void run_sim_test() {
    int pass = 0, fail = 0;
    printf("===== Adder/Subtractor 4-bit Simulation =====\n");
    for (unsigned sub = 0; sub <= 1; sub++) {
        for (unsigned a = 0; a < 16; a++) {
            for (unsigned b = 0; b < 16; b++) {
                dut->sub = sub;
                dut->a = a;
                dut->b = b;
                dut->eval();

                Ref e = ref_model(sub, a, b);
                if (dut->result != e.result || dut->cout != e.cout ||
                    dut->overflow != e.overflow || dut->zero != e.zero) {
                    printf("FAIL: sub=%u a=%2u b=%2u => result=%2u cout=%u ovf=%u zero=%u"
                           "  (expect result=%2u cout=%u ovf=%u zero=%u)\n",
                           sub, a, b, dut->result, dut->cout, dut->overflow, dut->zero,
                           e.result, e.cout, e.overflow, e.zero);
                    fail++;
                } else {
                    pass++;
                }
            }
        }
    }
    printf("===== %d passed, %d failed =====\n", pass, fail);
    assert(fail == 0 && "Adder/subtractor output mismatch!");
}

// 打印几组有代表性的用例, 直观展示「同一串比特, 两种解读」
static void show_cases() {
    struct { unsigned sub, a, b; const char *note; } cases[] = {
        {0, 3,  2, "两种解读都正常"},
        {0, 3, 14, "无符号溢出; 补码 3+(-2)=1 正常"},
        {0, 7,  1, "无符号正常; 补码溢出"},
        {0, 9,  8, "两种解读都溢出"},
        {1, 3,  2, "两种解读都正常"},
        {1, 3,  5, "无符号借位; 补码 -2 正确"},
        {1, 5,  5, "结果为 0, zero 置位"},
        {1, 8,  1, "无符号正常; 补码溢出"},
    };

    printf("\n===== 代表性用例 (同一串比特, 两种解读) =====\n");
    printf("sub  a    b    | result cout ovfl zero | unsigned(cout)  signed(ovf)\n");
    printf("---------------+-----------------------+-------------------------------------\n");
    for (auto &c : cases) {
        dut->sub = c.sub; dut->a = c.a; dut->b = c.b;
        dut->eval();

        unsigned r = dut->result;
        char ubuf[40], sbuf[40];

        // --- 无符号解读: 输入按 0~15 读 ---
        if (c.sub) {
            // 减法: cout=1 表示无借位, 结果可信; cout=0 表示借位, 寄存器里是绕回值
            if (dut->cout) snprintf(ubuf, sizeof ubuf, "%u-%u = %u", c.a, c.b, r);
            else snprintf(ubuf, sizeof ubuf, "%u-%u -> %u BORROW", c.a, c.b, r);
        } else {
            // 加法: cout 就是第 5 位, 拼上去即为真值
            if (dut->cout) snprintf(ubuf, sizeof ubuf, "%u+%u -> %u CARRY", c.a, c.b, r);
            else snprintf(ubuf, sizeof ubuf, "%u+%u = %u", c.a, c.b, r);
        }

        // --- 补码解读: 输入按 -8~+7 读 ---
        int sa = (c.a & 0x8) ? (int)c.a - 16 : (int)c.a;
        int sb = (c.b & 0x8) ? (int)c.b - 16 : (int)c.b;
        int sr = (r & 0x8) ? (int)r - 16 : (int)r;
        if (dut->overflow) snprintf(sbuf, sizeof sbuf, "%d%c%d -> %d OVF", sa, c.sub ? '-' : '+', sb, sr);
        else snprintf(sbuf, sizeof sbuf, "%d%c%d = %d", sa, c.sub ? '-' : '+', sb, sr);

        printf(" %u   %u%u%u%u %u%u%u%u |  %u%u%u%u    %u    %u    %u  | %-16s %-16s %s\n",
               c.sub,
               (c.a>>3)&1, (c.a>>2)&1, (c.a>>1)&1, c.a&1,
               (c.b>>3)&1, (c.b>>2)&1, (c.b>>1)&1, c.b&1,
               (r>>3)&1, (r>>2)&1, (r>>1)&1, r&1,
               dut->cout, dut->overflow, dut->zero,
               ubuf, sbuf, c.note);
    }
    printf("\n注: 无符号解读下 a/b 读作 0~15, 只看 cout (加法 cout=1 溢出, 减法 cout=0 借位)\n");
    printf("    补码解读下 a/b 读作 -8~+7, 只看 overflow\n");
}



int main() {
    // 先跑仿真测试
    run_sim_test();
    show_cases();

#ifdef SIM_ONLY
    printf("Simulation done, exit (SIM_ONLY mode).\n");
    return 0;
#else
    dut->a = 0;
    dut->b = 0;
    dut->sub = 0;
    dut->eval();
    // ===== nvboard 引脚绑定 =====
    // 输入: a[3:0] <- SW3..SW0, b[3:0] <- SW7..SW4, sub <- SW15 (拨上=减法)
    // 输出: result[3:0] -> LD3..LD0
    //       cout -> LD8, overflow -> LD9, zero -> LD10 (与结果隔开, 便于分辨)
    nvboard_bind_pin(&dut->a, 4, SW3, SW2, SW1, SW0);
    nvboard_bind_pin(&dut->b, 4, SW7, SW6, SW5, SW4);
    nvboard_bind_pin(&dut->sub, 1, SW8);
    nvboard_bind_pin(&dut->result, 4, LD3, LD2, LD1, LD0);
    nvboard_bind_pin(&dut->cout, 1, LD4);
    nvboard_bind_pin(&dut->overflow, 1, LD14);
    nvboard_bind_pin(&dut->zero, 1, LD15);

    nvboard_init();
    while (1) {
        nvboard_update();  // 读取开关状态并刷新 LED 显示
        dut->eval();       // 组合逻辑求值
    }

    dut->final();
    return 0;
#endif
}
