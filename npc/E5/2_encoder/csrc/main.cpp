#include <Vtop.h>

#ifdef SIM_ONLY
#include <cstdio>
#include <cassert>

int main() {
    Vtop *dut = new Vtop;

    printf("===== Encoder 8-to-3 Verilator Simulation =====\n");
    int pass = 0;
    int total = 0;

    // Test: enable=1, various inputs
    dut->en = 1;

    // Test all 8 single-bit inputs
    for (int i = 0; i < 8; i++) {
        dut->x = (1 << i);
        dut->eval();
        total++;
        unsigned actual_code = dut->led & 0x7;
        unsigned actual_valid = (dut->led >> 3) & 1;
        printf("Test %2d: en=1 x=0x%02x => code=%u valid=%u %s\n",
               total, dut->x, actual_code, actual_valid,
               (actual_code == (unsigned)i && actual_valid == 1) ? "[PASS]" : "[FAIL]");
        assert(actual_code == (unsigned)i && "Code mismatch!");
        assert(actual_valid == 1 && "Valid mismatch!");
        pass++;
    }

    // Test: all zeros input -> valid should be 0
    dut->x = 0x00;
    dut->eval();
    total++;
    {
        unsigned actual_code = dut->led & 0x7;
        unsigned actual_valid = (dut->led >> 3) & 1;
        printf("Test %2d: en=1 x=0x00 => code=%u valid=%u %s\n",
               total, actual_code, actual_valid,
               (actual_code == 0 && actual_valid == 0) ? "[PASS]" : "[FAIL]");
        assert(actual_code == 0 && "All-zero code mismatch!");
        assert(actual_valid == 0 && "All-zero valid mismatch!");
        pass++;
    }

    // Test: multi-bit inputs (priority to MSB)
    struct MultiBitTest {
        unsigned x;
        unsigned expected_code;
    } multi_tests[] = {
        {0xC0, 7},  // 11000000
        {0x60, 6},  // 01100000
        {0x30, 5},  // 00110000
        {0x18, 4},  // 00011000
        {0x0C, 3},  // 00001100
        {0x06, 2},  // 00000110
        {0x03, 1},  // 00000011
        {0xAA, 7},  // 10101010
        {0x55, 6},  // 01010101
    };

    int n_multi = sizeof(multi_tests) / sizeof(multi_tests[0]);
    for (int i = 0; i < n_multi; i++) {
        dut->x = multi_tests[i].x;
        dut->eval();
        total++;
        unsigned actual_code = dut->led & 0x7;
        unsigned actual_valid = (dut->led >> 3) & 1;
        printf("Test %2d: en=1 x=0x%02x => code=%u valid=%u (expect code=%u) %s\n",
               total, dut->x, actual_code, actual_valid, multi_tests[i].expected_code,
               (actual_code == multi_tests[i].expected_code && actual_valid == 1) ? "[PASS]" : "[FAIL]");
        assert(actual_code == multi_tests[i].expected_code && "Multi-bit code mismatch!");
        assert(actual_valid == 1 && "Multi-bit valid mismatch!");
        pass++;
    }

    // Test: enable=0 -> everything should be zero
    dut->en = 0;
    dut->x = 0xFF;
    dut->eval();
    total++;
    {
        unsigned actual_code = dut->led & 0x7;
        unsigned actual_valid = (dut->led >> 3) & 1;
        printf("Test %2d: en=0 x=0xff => code=%u valid=%u %s\n",
               total, actual_code, actual_valid,
               (actual_code == 0 && actual_valid == 0) ? "[PASS]" : "[FAIL]");
        assert(actual_code == 0 && "Enable-off code mismatch!");
        assert(actual_valid == 0 && "Enable-off valid mismatch!");
        pass++;
    }

    // Test: enable=0 with all zeros
    dut->en = 0;
    dut->x = 0x00;
    dut->eval();
    total++;
    {
        unsigned actual_code = dut->led & 0x7;
        unsigned actual_valid = (dut->led >> 3) & 1;
        printf("Test %2d: en=0 x=0x00 => code=%u valid=%u %s\n",
               total, actual_code, actual_valid,
               (actual_code == 0 && actual_valid == 0) ? "[PASS]" : "[FAIL]");
        assert(actual_code == 0 && "Enable-off zeros code mismatch!");
        assert(actual_valid == 0 && "Enable-off zeros valid mismatch!");
        pass++;
    }

    printf("===== All %d/%d tests passed! =====\n", pass, total);

    dut->final();
    delete dut;
    return 0;
}

#else
#include <nvboard.h>

static Vtop *dut = new Vtop;

int main() {
    /* ===== Pin Bindings ===== */
    // Enable: SW8 -> en
    nvboard_bind_pin(&dut->en, 1, SW8);

    // 8-bit input: SW7-SW0 -> x[7:0]
    nvboard_bind_pin(&dut->x, 8, SW7, SW6, SW5, SW4, SW3, SW2, SW1, SW0);

    // LED output: led[3:0] -> LD3-LD0
    // led[3] = valid bit, led[2:0] = encoded result
    nvboard_bind_pin(&dut->led, 4, LD3, LD2, LD1, LD0);

    // 7-segment display: seg[7:0] -> SEG0 (active-low)
    nvboard_bind_pin(&dut->seg, 8, SEG0A, SEG0B, SEG0C, SEG0D, SEG0E, SEG0F, SEG0G, DEC0P);

    /* ===== Init & Run ===== */
    nvboard_init();

    while (1) {
        nvboard_update();   // read switch states, refresh LED/display
        dut->eval();        // combinational logic eval, no clock needed
    }

    dut->final();
    return 0;
}
#endif
