// csrc/main.cpp
// 8-bit 桶形移位器 仿真测试
//
// 用法：
//   make DIR=6_shifter MODULE=barrel_shifter sim

#include <cstdio>
#include <cassert>
#include <cstdint>
#include <Vbarrel_shifter.h>

static Vbarrel_shifter *dut = new Vbarrel_shifter;

// 参考模型：软件精确计算移位结果
static uint8_t ref_shift(uint8_t din, uint8_t shamt, bool lr, bool al) {
    int32_t s = shamt & 7;   // 0~7
    if (lr) {
        // 左移：算术/逻辑都是低位补0
        return static_cast<uint8_t>((din << s) & 0xFF);
    } else {
        // 右移
        if (al) {
            // 算术右移：保持符号位
            return static_cast<uint8_t>((int8_t)din >> s);
        } else {
            // 逻辑右移：高位补0
            return static_cast<uint8_t>(din >> s);
        }
    }
}

static void run_sim_test() {
    int pass = 0, fail = 0;

    printf("===== Barrel Shifter 8-bit Simulation =====\n");

    // 遍历所有输入组合：din[0..255], shamt[0..7], lr[0..1], al[0..1]
    for (int din = 0; din < 256; din++) {
        for (int shamt = 0; shamt < 8; shamt++) {
            for (int lr = 0; lr < 2; lr++) {
                for (int al = 0; al < 2; al++) {
                    dut->din   = (uint8_t)din;
                    dut->shamt = (uint8_t)shamt;
                    dut->lr    = (uint8_t)lr;
                    dut->al    = (uint8_t)al;
                    dut->eval();

                    uint8_t expect = ref_shift(
                        (uint8_t)din,
                        (uint8_t)shamt,
                        (bool)lr,
                        (bool)al
                    );
                    uint8_t actual = dut->dout;

                    if (actual != expect) {
                        printf(
                            "FAIL: din=0x%02X shamt=%u lr=%u al=%u => "
                            "dout=0x%02X (expect 0x%02X)\n",
                            din, shamt, lr, al, actual, expect
                        );
                        fail++;
                    } else {
                        pass++;
                    }
                }
            }
        }
    }

    printf("===== %d passed, %d failed =====\n", pass, fail);
    assert(fail == 0 && "Barrel shifter output mismatch!");
}

int main() {
    run_sim_test();
    printf("Simulation done.\n");
    return 0;
}