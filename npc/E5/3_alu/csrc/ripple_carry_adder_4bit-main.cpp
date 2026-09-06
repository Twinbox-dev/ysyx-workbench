// csrc/main.cpp
// 4-bit 波纹进位加法器: 单个 cpp 同时完成「仿真测试」与「nvboard 展示」
//
// 用法(见 Makefile):
//   make DIR=3_alu MODULE=ripple_carry_adder_4bit sim  # 仅仿真测试(不显示 nvboard)
//   make DIR=3_alu MODULE=ripple_carry_adder_4bit run  # 仿真测试 + nvboard 展示
#include <cstdio>
#include <cassert>
#include <Vripple_carry_adder_4bit.h>
#ifndef SIM_ONLY
#include <nvboard.h>
#endif

static Vripple_carry_adder_4bit *dut = new Vripple_carry_adder_4bit;

// ===== 仿真测试: 穷举 a, b ∈ [0, 15], cin ∈ {0, 1}, 共 512 组 =====
static void run_sim_test() {
    int pass = 0, fail = 0;
    printf("===== Ripple Carry Adder 4-bit Simulation =====\n");
    for (unsigned cin = 0; cin <= 1; cin++) {
        for (unsigned a = 0; a < 16; a++) {
            for (unsigned b = 0; b < 16; b++) {
                dut->a = a;
                dut->b = b;
                dut->cin = cin;
                dut->eval();

                unsigned expect = a + b + cin;                 // 5 位期望值
                unsigned actual = (dut->cout << 4) | dut->sum; // {cout, sum} 拼成 5 位实际值

                if (actual != expect) {
                    printf("FAIL: a=%u b=%u cin=%u => sum=%u cout=%u (expect sum=%u cout=%u)\n",
                           a, b, cin, dut->sum, dut->cout,
                           expect & 0xF, (expect >> 4) & 1);
                    fail++;
                } else {
                    pass++;
                }
            }
        }
    }
    printf("===== %d passed, %d failed =====\n", pass, fail);
    assert(fail == 0 && "Ripple carry adder output mismatch!");
}

int main() {
    // 先跑仿真测试
    run_sim_test();

#ifdef SIM_ONLY
    printf("Simulation done, exit (SIM_ONLY mode).\n");
    return 0;
#else
    // 复位 dut，避免将仿真测试最后的状态带入 nvboard
    dut->a = 0;
    dut->b = 0;
    dut->cin = 0;
    dut->eval();

    // ===== nvboard 引脚绑定 =====
    // 输入: a[3:0] <- SW3..SW0, b[3:0] <- SW7..SW4, cin <- SW8
    // 输出: sum[3:0] -> LD3..LD0, cout -> LD4 (LD4 为最高位, 即最终进位)
    nvboard_bind_pin(&dut->a, 4, SW3, SW2, SW1, SW0);
    nvboard_bind_pin(&dut->b, 4, SW7, SW6, SW5, SW4);
    nvboard_bind_pin(&dut->cin, 1, SW8);
    nvboard_bind_pin(&dut->sum, 4, LD3, LD2, LD1, LD0);
    nvboard_bind_pin(&dut->cout, 1, LD4);

    nvboard_init();
    while (1) {
        nvboard_update();  // 读取开关状态并刷新 LED 显示
        dut->eval();       // 组合逻辑求值
    }

    dut->final();
    return 0;
#endif
}
