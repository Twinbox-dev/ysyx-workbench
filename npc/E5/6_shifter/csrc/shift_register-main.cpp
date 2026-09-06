// csrc/main.cpp
// 8-bit 移位寄存器: 仿真测试 + nvboard 展示
//
// 用法：
//   make DIR=3_shift_register MODULE=shift_register sim
//   make DIR=3_shift_register MODULE=shift_register run

#include <cstdio>
#include <cassert>
#include <cstdint>
#include <Vshift_register.h>

#ifndef SIM_ONLY
#include <nvboard.h>
#endif

static Vshift_register *dut = new Vshift_register;

// 控制编码，和 Verilog 中一致
static const uint8_t CTRL_CLR  = 0b000; // 清0
static const uint8_t CTRL_LOAD = 0b001; // 置数
static const uint8_t CTRL_SRL  = 0b010; // 逻辑右移
static const uint8_t CTRL_SLL  = 0b011; // 逻辑左移
static const uint8_t CTRL_SRA  = 0b100; // 算术右移
static const uint8_t CTRL_SER  = 0b101; // 左端串行输入
static const uint8_t CTRL_ROR  = 0b110; // 循环右移
static const uint8_t CTRL_ROL  = 0b111; // 循环左移

// 执行一个时钟上升沿：先 clk=0，再 clk=1
static void posedge() {
    dut->clk = 0;
    dut->eval();
    dut->clk = 1;
    dut->eval();
}

// 用 LOAD 命令把 q 置为 val
static void load(uint8_t val) {
    dut->rst_n = 1;
    dut->ctrl  = CTRL_LOAD;
    dut->d     = val;
    dut->sin   = 0;
    posedge();
}

// 执行一个移位/置数/清0操作
static void do_op(uint8_t ctrl, uint8_t d, uint8_t sin) {
    dut->rst_n = 1;
    dut->ctrl  = ctrl;
    dut->d     = d;
    dut->sin   = sin;
    posedge();
}

// ===== 仿真测试 =====
static void run_sim_test() {
    int pass = 0, fail = 0;

    auto check = [&](uint8_t expect, const char* name) {
        if (dut->q != expect) {
            printf("FAIL: %-8s q=%02X expect=%02X\n", name, dut->q, expect);
            fail++;
        } else {
            pass++;
        }
    };

    printf("===== 8-bit Shift Register Simulation =====\n");

    // 异步复位
    dut->rst_n = 0;
    dut->clk = 0;
    dut->ctrl = 0;
    dut->d = 0;
    dut->sin = 0;
    dut->eval();
    check(0x00, "Reset");

    dut->rst_n = 1;
    dut->eval();

    // 清0
    load(0xAA);
    do_op(CTRL_CLR, 0, 0);
    check(0x00, "CLR");

    // 置数
    do_op(CTRL_LOAD, 0x3C, 0);
    check(0x3C, "LOAD");

    // 逻辑右移 SRL
    load(0x80);
    do_op(CTRL_SRL, 0, 0);
    check(0x40, "SRL");

    load(0x01);
    do_op(CTRL_SRL, 0, 0);
    check(0x00, "SRL2");

    // 逻辑左移 SLL
    load(0x01);
    do_op(CTRL_SLL, 0, 0);
    check(0x02, "SLL");

    load(0x80);
    do_op(CTRL_SLL, 0, 0);
    check(0x00, "SLL2");

    // 算术右移 SRA
    load(0x80);
    do_op(CTRL_SRA, 0, 0);
    check(0xC0, "SRA");

    load(0x40);
    do_op(CTRL_SRA, 0, 0);
    check(0x20, "SRA2");

    load(0xFF);
    do_op(CTRL_SRA, 0, 0);
    check(0xFF, "SRA3");

    // 左端串行输入 SER
    load(0x00);
    do_op(CTRL_SER, 0, 1);
    check(0x80, "SER1");

    load(0x01);
    do_op(CTRL_SER, 0, 1);
    check(0x80, "SER2");

    load(0x80);
    do_op(CTRL_SER, 0, 0);
    check(0x40, "SER3");

    // 循环右移 ROR
    load(0x01);
    do_op(CTRL_ROR, 0, 0);
    check(0x80, "ROR");

    load(0x80);
    do_op(CTRL_ROR, 0, 0);
    check(0x40, "ROR2");

    load(0xFF);
    do_op(CTRL_ROR, 0, 0);
    check(0xFF, "ROR3");

    // 循环左移 ROL
    load(0x80);
    do_op(CTRL_ROL, 0, 0);
    check(0x01, "ROL");

    load(0x01);
    do_op(CTRL_ROL, 0, 0);
    check(0x02, "ROL2");

    // 再次复位
    dut->rst_n = 0;
    dut->clk = 0;
    dut->eval();
    check(0x00, "Reset2");

    printf("===== %d passed, %d failed =====\n", pass, fail);
    assert(fail == 0 && "Shift register output mismatch!");
}

int main() {
    // 先跑仿真测试
    run_sim_test();

#ifdef SIM_ONLY
    printf("Simulation done, exit (SIM_ONLY mode).\n");
    return 0;
#else
    // 复位 dut，避免将仿真测试最后的状态带入 nvboard
    dut->clk = 0;
    dut->rst_n = 1;
    dut->ctrl = 0;
    dut->d = 0;
    dut->sin = 0;
    dut->eval();

    // ===== nvboard 引脚绑定 =====
    // 输入：
    //   d[7:0] <- SW7..SW0
    //   ctrl[2:0] <- SW11..SW9
    //   sin      <- SW8
    //   rst_n    <- SW15 (0=复位, 1=正常工作)
    //
    // 输出：
    //   q[7:0] -> LD7..LD0

    nvboard_bind_pin(&dut->d, 8, SW7, SW6, SW5, SW4, SW3, SW2, SW1, SW0);
    nvboard_bind_pin(&dut->ctrl, 3, SW11, SW10, SW9);
    nvboard_bind_pin(&dut->sin, 1, SW8);
    nvboard_bind_pin(&dut->rst_n, 1, SW15);

    nvboard_bind_pin(&dut->q, 8, LD7, LD6, LD5, LD4, LD3, LD2, LD1, LD0);

    nvboard_init();

    while (1) {
        nvboard_update();
        posedge();
    }

    dut->final();
    return 0;
#endif
}
