#include "Vtop.h"
#include "verilated.h"
#include "verilated_fst_c.h"

#include <cassert>
#include <iostream>
#include <cstdlib>
#include <ctime>

int main(int argc,char* argv[]){
	Verilated::commandArgs(argc,argv);
	Verilated::traceEverOn(true);
	Vtop* top = new Vtop;
	VerilatedFstC* tfp = new VerilatedFstC;
	top->trace(tfp, 99);
	tfp->open("dump.fst");

	srand(time(NULL));

	for(int i = 0; i < 50; i++){
		top->a = rand() & 1;
		top->b = rand() & 1;
		top->eval();
		tfp->dump(i);
		assert(top->result == (top->a ^ top->b));
		std::cout << "a="  << +top->a 
     	          << " b=" << +top->b 
                  << " result=" << (int)top->result 
                  << std::endl;
	}
	tfp->close();
	delete top;
	delete tfp;
	return 0;
}