// 触发器模板
/*
Tip:这是一个同步复位的触发器(所有信号都随时钟同步更新)，
    而我的仿真集都是异步复位(复位信号也能触发always块，只要是受复位信号影响的信号，都称该信号是异步的)
    就会导致这个模板无法通过部分仿真集，这边建议在自包含的调用的时候将模板改成异步复位！
*/
module Reg #(
    WIDTH     = 1,
    RESET_VAL = 0
) (
    input                  clk,
    input                  rst,
    input  [WIDTH-1:0]     din,
    output reg [WIDTH-1:0] dout,
    input                  wen
);
    always @(posedge clk) begin
        if (rst) begin
            dout <= RESET_VAL;
        end else if (wen) begin
            dout <= din;
        end
    end
endmodule

// 使用触发器模板的示例
module example (
    input      [3:0] in,
    output     [3:0] out,
    input            clk,
    input            rst
);
    // 位宽为1比特, 复位值为1'b1, 写使能一直有效
    Reg #(
        1,
        1'b1
    ) i0 (
        .clk (clk),
        .rst (rst),
        .din (in[0]),
        .dout(out[0]),
        .wen (1'b1)
    );

    // 位宽为3比特, 复位值为3'b0, 写使能为out[0]
    Reg #(
        3,
        3'b0
    ) i1 (
        .clk (clk),
        .rst (rst),
        .din (in[3:1]),
        .dout(out[3:1]),
        .wen (out[0])
    );
endmodule
