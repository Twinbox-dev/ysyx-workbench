#include "Vtop.h"
#include "verilated.h"

#include <cassert>
#include <iostream>
#include <cstdlib>
#include <ctime>

int main(int argc,char* argv[]){
	Verilated::commandArgs(argc,argv);
	Vtop* top = new Vtop;
	srand(time(NULL));

	for(int i = 0; i < 50; i++){
		top->a = rand() & 1;
		top->b = rand() & 1;
		top->eval();
		assert(top->result == top->a ^ top->b);
        std::cout << "a="  << +top->a 
     	          << " b=" << +top->b 
                  << " result=" << (int)top->result 
                  << std::endl;
	}
	delete top;
	return 0;
}