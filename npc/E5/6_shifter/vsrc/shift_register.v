module shift_register (
    input  wire        clk,      // 时钟信号
    input  wire        rst_n,    // 异步复位(低有效)
    input  wire [2:0]  ctrl,     // 控制位 [2:0]
    input  wire        sin,      // 左端串行输入(1位)
    input  wire [7:0]  d,        // 并行置数数据输入
    output reg  [7:0]  q         // 并行输出
);

    // 控制位定义 (对应 Table 6)
    localparam CLR   = 3'b000; // 清0
    localparam LOAD  = 3'b001; // 置数
    localparam SRL   = 3'b010; // 逻辑右移
    localparam SLL   = 3'b011; // 逻辑左移
    localparam SRA   = 3'b100; // 算术右移
    localparam SER   = 3'b101; // 左端串行输入1位
    localparam ROR   = 3'b110; // 循环右移
    localparam ROL   = 3'b111; // 循环左移

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            q <= 8'b0;
        end else begin
            case (ctrl)
                CLR:  q <= 8'b0;
                LOAD: q <= d;
                SRL:  q <= {1'b0, q[7:1]};
                SLL:  q <= {q[6:0], 1'b0};
                SRA:  q <= {q[7], q[7:1]};
                SER:  q <= {sin, q[7:1]};
                ROR:  q <= {q[0], q[7:1]};
                ROL:  q <= {q[6:0], q[7]};
                default: q <= q;
            endcase
        end
    end

endmodule
