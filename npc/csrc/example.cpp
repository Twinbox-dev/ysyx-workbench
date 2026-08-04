// 通用头文件
#include "V[module_name].h"             // 替换为模块名
#include "verilated_[wave_format].h"    // vcd_c或fst_c
#include "verilated.h"
#include <iostream>

// 通用仿真控制部分
int main(int argc, char** argv) {
    // 1. 初始化部分
    Verilated::commandArgs(argc, argv);
    V[module_name] * top = new V[module_name];  // 实例化模块

    // 2. 波形初始化（可选）
    Verilated::traceEverOn(true);
    VerilatedFstC* tfp = new VerilatedFstC;
    top->trace(tfp, 99);
    tfp->open("wave.fst");

    // 3. 测试激励生成
    for (int i = 0; i < [test_cycles]; ++i) {
        // 设置输入信号（根据测试需求变化）
        top->input_a = i % 2;
        top->input_b = (i / 2) % 2;

        // 评估电路
        top->eval();

        // 记录波形
        tfp->dump(i * 10);  // 时间刻度可根据需要调整

        // 输出调试信息
        std::cout << "Cycle " << i
            << ": out = " << top->output_signal
            << std::endl;

        // 验证逻辑（自定义）
        assert(top->output_signal == expected_value);
    }

    // 4. 清理
    tfp->close();
    delete top;
    delete tfp;
    return 0;
}




/*
    // 初始化随机数种子
    srand(time(NULL));

    // 测试i次的随机输入
    for(int i = 0; i < 20; i++) {
        top->input1 = rand() & 1;
        top->input2 = rand() & 1;

        // 更新电路状态
        top->eval();
*/
