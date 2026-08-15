module top(
  input        clk,
  input        rst,
  output reg [15:0] led
);

  reg [23:0] cnt;

  // LD0 在屏幕上最右, LD15 在最左, 所以"从右到左" = led 左移 (bit0 -> bit15)
  always @(posedge clk) begin
    if (rst) begin
      cnt <= 0;
      led <= 16'h0001;                    // 先让最右边的 LD0 亮
    end else if (cnt == 24'hffffff) begin // 计数器计满一个周期, 灯挪一步
      cnt <= 0;
      led <= {led[14:0], led[15]};        // 循环左移: 亮的位置往左挪, 原灯熄灭
    end else begin
      cnt <= cnt + 1;
    end
  end

endmodule
