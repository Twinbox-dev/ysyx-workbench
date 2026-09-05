#include <Vtop.h>

#ifdef SIM_ONLY
#include <cstdio>
#include <cassert>

int main() {
    Vtop *dut = new Vtop;

    struct TestCase {
        unsigned X0 : 2;
        unsigned X1 : 2;
        unsigned X2 : 2;
        unsigned X3 : 2;
        unsigned Y  : 2;
        unsigned F  : 2;   // expected output
    };

    TestCase tests[] = {
        { 1, 2, 3, 0, 0b00, 1 },
        { 3, 2, 1, 0, 0b00, 3 },
        { 0, 1, 2, 3, 0b00, 0 },
        { 0, 2, 1, 3, 0b01, 2 },
        { 3, 1, 0, 2, 0b01, 1 },
        { 1, 2, 3, 0, 0b10, 3 },
        { 2, 0, 1, 3, 0b10, 1 },
        { 1, 2, 3, 0, 0b11, 0 },
        { 3, 2, 1, 3, 0b11, 3 },
    };

    int n = sizeof(tests) / sizeof(tests[0]);
    int pass = 0;

    printf("===== MUX 4-to-1 Verilator Simulation =====\n");

    for (int i = 0; i < n; i++) {
        dut->X0 = tests[i].X0;
        dut->X1 = tests[i].X1;
        dut->X2 = tests[i].X2;
        dut->X3 = tests[i].X3;
        dut->Y  = tests[i].Y;
        dut->eval();

        unsigned actual = dut->F;
        unsigned expect = tests[i].F;

        printf("Test %2d: X0=%d X1=%d X2=%d X3=%d Y=%d => F=%d (expect %d) %s\n",
               i + 1,
               tests[i].X0, tests[i].X1, tests[i].X2, tests[i].X3,
               tests[i].Y, actual, expect,
               actual == expect ? "[PASS]" : "[FAIL]");

        assert(actual == expect && "MUX output mismatch!");
        pass++;
    }

    printf("===== All %d tests passed! =====\n", pass);

    dut->final();
    delete dut;
    return 0;
}
#else
#include <nvboard.h>

static Vtop *dut = new Vtop;

int main() {
    nvboard_bind_pin(&dut->X0, 2, SW1, SW0);
    nvboard_bind_pin(&dut->X1, 2, SW3, SW2);
    nvboard_bind_pin(&dut->X2, 2, SW5, SW4);
    nvboard_bind_pin(&dut->X3, 2, SW7, SW6);
    nvboard_bind_pin(&dut->Y, 2, SW9, SW8);
    nvboard_bind_pin(&dut->F, 2, LD1, LD0);

    nvboard_init();

    while (1) {
        nvboard_update();
        dut->eval();
    }

    dut->final();
    return 0;
}
#endif