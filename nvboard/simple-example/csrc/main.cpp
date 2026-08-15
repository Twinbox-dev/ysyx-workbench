#include <nvboard.h>
#include <Vtop.h>

static Vtop dut;

int main() {
  nvboard_bind_pin(&dut.sw, 2, SW1, SW0);
  nvboard_bind_pin(&dut.led, 1, LD0);
  nvboard_init();

  while (1) {
    nvboard_update();
    dut.eval();
  }
}