// =====================================================================
// top.v 时序逻辑的 Verilator 平铺代码 · 可读改写版 (仅学习参考, 不参与编译)
// 原始生成文件: build/Vtop___024root__DepSet_heccd7ead__0.cpp
// 函数: Vtop___024root___nba_sequent__TOP__0()
//
// 术语:
//   nba    = Non-Blocking Assignment, 非阻塞赋值区 (<= 赋值)
//   sequent = sequential, 时序逻辑 (always @(posedge clk))
//   __DOT__ = Verilog 里的 '.', top__DOT__count 即模块内信号 count
//
// 原始 Verilog (对照用):
//   always @(posedge clk) begin
//       if (rst)               led <= 16'h0001;      count <= 0;
//       else if (count >= 10)  led <= {led[14:0], led[15]}; count <= 0;
//       else                   led <= led;           count <= count + 1;
//   end
// =====================================================================

// 非阻塞赋值语义: 先用旧值把所有新值算好, always 块结束再统一写回,
// 保证同一拍内多个赋值互不干扰。这就是下面两个 next_ 临时变量的由来。
void nba_sequent(Vtop___024root* self) {
    uint16_t next_led   = self->led;              // led 的新值
    uint32_t next_count = self->top__DOT__count;  // count 的新值

    if (self->rst) {                                  // if (rst)
        next_led   = 0x0001;                          //   led   <= 16'h0001
        next_count = 0;                               //   count <= 0
    } else if (next_count >= 10) {                    // else if (count >= 32'd10)
        next_led   = (next_led << 1) | (next_led >> 15); //   led <= {led[14:0], led[15]} 循环左移
        next_count = 0;                               //   count <= 0
    } else {                                          // else
        next_count = next_count + 1;                  //   count <= count + 1 (led 不变)
    }

    self->led             = next_led;                 // 统一写回
    self->top__DOT__count = next_count;
}
