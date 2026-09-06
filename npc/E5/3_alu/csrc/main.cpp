// csrc/main.cpp
// 4-bit ALU: 同时完成仿真测试与 nvboard 展示
//
// 用法：
//   make DIR=3_alu MODULE=top sim   # 仅仿真测试
//   make DIR=3_alu MODULE=top run   # 仿真测试 + nvboard 展示

#include <cstdio>
#include <cassert>
#include <cstdint>
#include <Vtop.h>

#ifndef SIM_ONLY
#include <nvboard.h>
#endif

static Vtop *dut = new Vtop;

// 把 4 位无符号数看成 4 位补码，转换成有符号数
static inline int s4(uint8_t x) {
    x &= 0xF;
    return (x & 0x8) ? (int)x - 16 : (int)x;
}

// 4 位无符号加法结果
static inline uint8_t add_result(uint8_t a, uint8_t b) {
    return static_cast<uint8_t>((a + b) & 0xF);
}

// 无符号加法进位输出 Cout
static inline uint8_t add_carry(uint8_t a, uint8_t b) {
    return static_cast<uint8_t>(((uint32_t)a + b) >> 4);
}

// 补码减法：A + ~B + 1，取低 4 位
static inline uint8_t sub_result(uint8_t a, uint8_t b) {
    uint32_t tmp = (uint32_t)a + ((~b) & 0xF) + 1;
    return static_cast<uint8_t>(tmp & 0xF);
}

// 减法对应的进位输出 Cout：1=无借位，0=有借位
static inline uint8_t sub_carry(uint8_t a, uint8_t b) {
    uint32_t tmp = (uint32_t)a + ((~b) & 0xF) + 1;
    return static_cast<uint8_t>((tmp >> 4) & 1);
}

// 有符号补码加法是否溢出
static inline bool add_overflow(uint8_t a, uint8_t b) {
    int r = s4(a) + s4(b);
    return r < -8 || r > 7;
}

// 有符号补码减法是否溢出
static inline bool sub_overflow(uint8_t a, uint8_t b) {
    int r = s4(a) - s4(b);
    return r < -8 || r > 7;
}

// ===== 仿真测试：遍历 S=0..7, A=0..15, B=0..15，共 2048 组 =====
static void run_sim_test() {
    int pass = 0, fail = 0;

    printf("===== ALU 4-bit Simulation =====\n");

    for (unsigned s = 0; s < 8; s++) {
        for (unsigned a = 0; a < 16; a++) {
            for (unsigned b = 0; b < 16; b++) {
                dut->A = a;
                dut->B = b;
                dut->S = s;
                dut->eval();

                uint8_t exp_result = 0;
                bool exp_zero = false;
                bool exp_overflow = false;
                bool exp_carry = false;
                bool exp_lt = false;
                bool exp_eq = false;

                switch (s) {
                    case 0: // A + B
                        exp_result = add_result(a, b);
                        exp_overflow = add_overflow(a, b);
                        exp_carry = add_carry(a, b);
                        break;

                    case 1: // A - B
                        exp_result = sub_result(a, b);
                        exp_overflow = sub_overflow(a, b);
                        exp_carry = sub_carry(a, b);
                        break;

                    case 2: // ~A
                        exp_result = static_cast<uint8_t>((~a) & 0xF);
                        break;

                    case 3: // A & B
                        exp_result = a & b;
                        break;

                    case 4: // A | B
                        exp_result = a | b;
                        break;

                    case 5: // A ^ B
                        exp_result = a ^ b;
                        break;

                    case 6: // A < B（有符号比较）
                        exp_lt = s4(a) < s4(b);
                        exp_result = exp_lt ? 1 : 0;
                        break;

                    case 7: // A == B
                        exp_eq = (a == b);
                        exp_result = exp_eq ? 1 : 0;
                        break;
                }

                exp_zero = (exp_result == 0);

                uint8_t got_result = dut->Result;
                bool got_zero = dut->Zero;
                bool got_overflow = dut->Overflow;
                bool got_carry = dut->Carry;
                bool got_lt = dut->Lt;
                bool got_eq = dut->Eq;

                bool ok = (got_result == exp_result) &&
                          (got_zero == exp_zero) &&
                          (got_overflow == exp_overflow) &&
                          (got_carry == exp_carry) &&
                          (got_lt == exp_lt) &&
                          (got_eq == exp_eq);

                if (!ok) {
                    printf("FAIL: S=%u A=%u B=%u\n", s, a, b);
                    printf("  Result  got=%u exp=%u\n", got_result, exp_result);
                    printf("  Zero    got=%d exp=%d\n", got_zero, exp_zero);
                    printf("  Overflw got=%d exp=%d\n", got_overflow, exp_overflow);
                    printf("  Carry   got=%d exp=%d\n", got_carry, exp_carry);
                    printf("  Lt      got=%d exp=%d\n", got_lt, exp_lt);
                    printf("  Eq      got=%d exp=%d\n", got_eq, exp_eq);
                    fail++;
                } else {
                    pass++;
                }
            }
        }
    }

    printf("===== %d passed, %d failed =====\n", pass, fail);
    assert(fail == 0 && "ALU output mismatch!");
}

int main() {
    // 先跑穷举仿真测试
    run_sim_test();

#ifdef SIM_ONLY
    printf("Simulation done, exit (SIM_ONLY mode).\n");
    return 0;
#else
    // 复位 dut，避免将仿真测试最后的状态带入 nvboard
    dut->A = 0;
    dut->B = 0;
    dut->S = 0;
    dut->eval();

    // ===== nvboard 引脚绑定 =====
    // 输入：
    //   A[3:0] <- SW3..SW0
    //   B[3:0] <- SW7..SW4
    //   S[2:0] <- SW10, SW9, SW8
    //
    // 输出：
    //   Result[3:0] -> LD3..LD0
    //   Overflow    -> LD4
    //   Carry       -> LD5
    //   Zero        -> LD6
    //   Lt          -> LD7
    //   Eq          -> LD8

    nvboard_bind_pin(&dut->A, 4, SW3, SW2, SW1, SW0);
    nvboard_bind_pin(&dut->B, 4, SW7, SW6, SW5, SW4);
    nvboard_bind_pin(&dut->S, 3, SW10, SW9, SW8);

    nvboard_bind_pin(&dut->Result, 4, LD3, LD2, LD1, LD0);
    nvboard_bind_pin(&dut->Overflow, 1, LD4);
    nvboard_bind_pin(&dut->Carry, 1, LD5);
    nvboard_bind_pin(&dut->Zero, 1, LD6);
    nvboard_bind_pin(&dut->Lt, 1, LD7);
    nvboard_bind_pin(&dut->Eq, 1, LD8);

    nvboard_init();

    while (1) {
        nvboard_update();  // 读取开关状态并刷新 LED
        dut->eval();       // 组合逻辑求值
    }

    dut->final();
    return 0;
#endif
}
