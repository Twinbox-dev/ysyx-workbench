#include "Vtop.h"       // Verilator生成的头文件
#include "verilated.h"  // Verilator自身定义的头文件工具
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

int main(int argc, char** argv) {
    // 初始化Verilator环境
    Verilated::commandArgs(argc, argv);
    // 创建模块实例
    Vtop* top = new Vtop;
    
    // 初始化随机数种子
    srand(time(NULL));
    
    // 测试20次随机输入
    for (int i = 0; i < 20; i++) {
        // 生成随机输入
        int a = rand() & 1;
        int b = rand() & 1;

        // 驱动输入端口 
        top->a = a;
        top->b = b;
        
        // 更新电路状态
        top->eval();
        
        printf("Test %2d: a = %d, b = %d, result = %d\n", 
               i+1, a, b, top->result);
        
        // 验证异或功能是否正确
        assert(top->result == (a ^ b));
    }
    
    printf("\33[1:32mAll tests passed!\033[0m \n");
    delete top;
    return 0;
}
