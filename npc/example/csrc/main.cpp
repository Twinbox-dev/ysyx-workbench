// sim_main.cpp - C++ 仿真激励，带 FST 波形输出
#include <cstdio>
#include "Vtop.h"
#include "verilated.h"
#include "verilated_fst_c.h"   // FST 波形支持

int main(int argc, char** argv) {
    // 1. 创建 Verilator 上下文和顶层模块
    VerilatedContext* contextp = new VerilatedContext;
    contextp->commandArgs(argc, argv);
	Verilated::traceEverOn(true);
    Vtop* top = new Vtop{contextp};

    // 2. 开启 FST 波形记录
    VerilatedFstC* tfp = new VerilatedFstC;
    top->trace(tfp, 0);            // 0 = 不追踪线网级别
    tfp->open("waveform.fst");     // 输出文件

    // 3. 仿真参数
    int  cycle     = 0;
    int  max_cycle = 500;
    bool clk       = 0;

    // 4. 仿真主循环
    while (cycle < max_cycle) {
        // 4.1 时钟翻转（半个周期一次）
        clk = !clk;
        top->clk = clk;

        // 4.2 前 5 个周期保持复位
        top->rst = (cycle < 5) ? 1 : 0;

        // 4.3 上升沿时打印 led
        if (top->clk) {
            printf("cycle=%3d, led=0x%04x\n", cycle, top->led);
        }

        // 4.4 驱动模型计算
        top->eval();

        // 4.5 写入波形
        tfp->dump(contextp->time());

        // 4.6 时间前进
        cycle++;
        contextp->timeInc(1);
    }

    // 5. 收尾
    top->final();
    tfp->close();
    delete tfp;
    delete top;
    delete contextp;
    return 0;
}