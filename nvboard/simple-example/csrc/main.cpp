#include <nvboard.h>
#include <Vtop.h>

static Vtop *dut = new Vtop;

// 一个时钟周期: 拉低 clk 仿真一次, 拉高 clk 仿真一次
static void single_cycle() {
    dut->clk = 0;
    dut->eval();
    dut->clk = 1;
    dut->eval();
}

// 复位 n 个时钟周期
static void reset(int n) {
    dut->rst = 1;
    while (n-- > 0) {
        single_cycle();
    }
    dut->rst = 0;
}

int main() {
    // 向量信号按 MSB 到 LSB 绑定: bit15 -> LD15, ..., bit0 -> LD0
    nvboard_bind_pin(&dut->led, 16,
        LD15, LD14, LD13, LD12, LD11, LD10, LD9, LD8,
        LD7, LD6, LD5, LD4, LD3, LD2, LD1, LD0);
    nvboard_init();
    reset(10);

    while (1) {
        nvboard_update();  // 更新虚拟板子: 读取 led 刷新屏幕
        single_cycle();    // 电路跑一个时钟周期
    }
}
