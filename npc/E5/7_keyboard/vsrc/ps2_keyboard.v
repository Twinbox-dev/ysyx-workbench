// =============================================================================
// PS/2 键盘接收控制器
//
// 只负责把键盘串行送来的 11 位数据帧还原成 8 位扫描码，并放入一个 8 字节的
// FIFO 队列。识别"按下的是哪个键"由上层的 key_ctrl 模块负责。
//
// PS/2 数据帧格式（每位在 ps2_clk 下降沿有效）：
//   起始位(0) + 8 位数据(低位在前) + 奇校验位 + 停止位(1)
//
// 握手时序：
//   ready = 1 表示 FIFO 中有数据，此时 data 就是队首字节；
//   上层取走数据后，把 nextdata_n 拉低 1 个周期，读指针前移。
// =============================================================================

module ps2_keyboard (
    input  wire       clk,
    input  wire       clrn,        // 低有效复位
    input  wire       ps2_clk,     // 键盘时钟
    input  wire       ps2_data,    // 键盘串行数据
    input  wire       nextdata_n,  // 低有效：确认已取走当前数据
    output wire [7:0] data,        // FIFO 队首字节
    output reg        ready,       // FIFO 非空
    output reg        overflow     // FIFO 曾经溢出
);

    // ---------------- ps2_clk 下降沿检测 ----------------
    // 先用 3 级触发器把异步的 ps2_clk 同步到 clk 域，再检测下降沿。
    // 下降沿正好落在数据位的中间，此时 ps2_data 已经稳定。
    reg [2:0] ps2_clk_sync;

    always @(posedge clk) begin
        ps2_clk_sync <= {ps2_clk_sync[1:0], ps2_clk};
    end

    wire sampling = ps2_clk_sync[2] & ~ps2_clk_sync[1];

    // ---------------- 接收缓冲 + FIFO ----------------
    // buffer 采用右移方式接收：最先收到的位最终落在 buffer[0]
    //   buffer[0]   = 起始位
    //   buffer[8:1] = 8 位扫描码
    //   buffer[9]   = 奇校验位
    // 停止位不进 buffer，在收到第 11 位时直接看 ps2_data
    reg [9:0] buffer;
    reg [7:0] fifo [0:7];
    reg [2:0] w_ptr, r_ptr;
    reg [3:0] count;          // 已接收的位数

    always @(posedge clk) begin
        if (!clrn) begin
            count    <= 4'd0;
            w_ptr    <= 3'd0;
            r_ptr    <= 3'd0;
            buffer   <= 10'd0;
            overflow <= 1'b0;
            ready    <= 1'b0;
        end
        else begin
            // 上层取走一个字节：读指针前移，若队列变空则撤销 ready
            if (ready && (nextdata_n == 1'b0)) begin
                r_ptr <= r_ptr + 3'd1;
                if (w_ptr == (r_ptr + 3'd1)) ready <= 1'b0;
            end

            if (sampling) begin
                if (count == 4'd10) begin
                    // 第 11 位（停止位），此时校验整帧
                    if ((buffer[0] == 1'b0) &&   // 起始位
                        (ps2_data == 1'b1) &&    // 停止位
                        (^buffer[9:1])) begin    // 奇校验
                        fifo[w_ptr] <= buffer[8:1];
                        w_ptr       <= w_ptr + 3'd1;
                        ready       <= 1'b1;     // 覆盖上面的 ready<=0，新数据优先
                        overflow    <= overflow | (r_ptr == (w_ptr + 3'd1));
                    end
                    count <= 4'd0;
                end
                else begin
                    buffer <= {ps2_data, buffer[9:1]};
                    count  <= count + 4'd1;
                end
            end
        end
    end

    assign data = fifo[r_ptr];

endmodule
