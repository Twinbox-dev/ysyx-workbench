#include <verilated_vcd_c.h>
#include <Vencode42.h>

#include <cstdio>
#include <cstdint>
#include <cassert>

VerilatedContext* contextp = NULL;
VerilatedVcdC* tfp = NULL;
static Vencode42* top = NULL;

void step_and_dump_wave() {
    top->eval();
    contextp->timeInc(1);
    tfp->dump(contextp->time());
}

void sim_init() {
    contextp = new VerilatedContext;
    tfp = new VerilatedVcdC;
    top = new Vencode42;

    contextp->traceEverOn(true);
    top->trace(tfp, 0);
    tfp->open("dump.vcd");
}

void sim_exit() {
    step_and_dump_wave();
    top->final();
    tfp->close();
    delete top;
    delete tfp;
    delete contextp;
}

int main() {
    sim_init();

    struct TestCase {
        uint8_t en;
        uint8_t x;
        uint8_t expect_y;
    };

    TestCase tests[] = {
        {0, 0x0, 0},
        {0, 0x1, 0},
        {0, 0xF, 0},

        {1, 0x0, 0},
        {1, 0x1, 0},
        {1, 0x2, 1},
        {1, 0x4, 2},
        {1, 0x8, 3},

        {1, 0x3, 1},
        {1, 0x5, 2},
        {1, 0x9, 3},
        {1, 0x6, 2},
        {1, 0xA, 3},
        {1, 0xC, 3},
        {1, 0xF, 3},
    };

    int n = sizeof(tests) / sizeof(tests[0]);
    int pass = 0;

    printf("===== Encode42 Verilator Simulation =====\n");

    for (int i = 0; i < n; i++) {
        top->en = tests[i].en;
        top->x  = tests[i].x;
        step_and_dump_wave();

        uint8_t actual = top->y;
        uint8_t expect = tests[i].expect_y;
        bool ok = (actual == expect);

        printf("Test %2d: en=%d x=0x%X => y=%d (expect %d) %s\n",
               i + 1,
               top->en,
               top->x,
               actual,
               expect,
               ok ? "[PASS]" : "[FAIL]");

        assert(ok && "encode42 output mismatch!");
        pass++;
    }

    printf("===== All %d tests passed! =====\n", pass);

    sim_exit();
    return 0;
}
