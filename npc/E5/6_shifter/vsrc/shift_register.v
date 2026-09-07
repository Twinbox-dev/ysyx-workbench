// =============================================================================
// 触发器模板(改成了异步复位->复位信号不受时钟控制，因为原始提供的是同步复位的寄存器，会导致仿真无法通过)
// =============================================================================
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
    always @(posedge clk or posedge rst) begin
        if (rst) begin
            dout <= RESET_VAL;
        end else if (wen) begin
            dout <= din;
        end
    end
endmodule

// =============================================================================
// 选择器模板
// =============================================================================
/* verilator lint_off DECLFILENAME */
module MuxKeyInternal #(
	NR_KEY      = 2,
	KEY_LEN     = 1,
	DATA_LEN    = 1,
	HAS_DEFAULT = 0
) (
	output reg [DATA_LEN-1:0] out,
	input      [KEY_LEN-1:0] key,
	input      [DATA_LEN-1:0] default_out,
	input      [NR_KEY*(KEY_LEN + DATA_LEN)-1:0] lut
);

	localparam PAIR_LEN = KEY_LEN + DATA_LEN;

	wire [PAIR_LEN-1:0] pair_list [NR_KEY-1:0];
	wire [KEY_LEN-1:0]  key_list  [NR_KEY-1:0];
	wire [DATA_LEN-1:0] data_list [NR_KEY-1:0];

	generate
		for (genvar n = 0; n < NR_KEY; n = n + 1) begin : gen_lut
			assign pair_list[n] = lut[PAIR_LEN*(n+1)-1 : PAIR_LEN*n];
			assign data_list[n] = pair_list[n][DATA_LEN-1:0];
			assign key_list[n]  = pair_list[n][PAIR_LEN-1:DATA_LEN];
		end
	endgenerate

	reg [DATA_LEN-1:0] lut_out;
	reg hit;
	integer i;

	always @(*) begin
		lut_out = 0;
		hit     = 0;
		for (i = 0; i < NR_KEY; i = i + 1) begin
			lut_out = lut_out | ({DATA_LEN{key == key_list[i]}} & data_list[i]);
			hit     = hit | (key == key_list[i]);
		end
		if (!HAS_DEFAULT)
			out = lut_out;
		else
			out = (hit ? lut_out : default_out);
	end

endmodule

module MuxKeyWithDefault #(
	NR_KEY   = 2,
	KEY_LEN  = 1,
	DATA_LEN = 1
) (
	output [DATA_LEN-1:0] out,
	input  [KEY_LEN-1:0]  key,
	input  [DATA_LEN-1:0] default_out,
	input  [NR_KEY*(KEY_LEN + DATA_LEN)-1:0] lut
);
	MuxKeyInternal #(NR_KEY, KEY_LEN, DATA_LEN, 1) i0 (
		.out        (out),
		.key        (key),
		.default_out(default_out),
		.lut        (lut)
	);
endmodule
/* verilator lint_on DECLFILENAME */

// =============================================================================
// 顶层移位寄存器 shift_register
// =============================================================================
module shift_register (
    input  wire        clk,
    input  wire        rst_n,
    input  wire [2:0]  ctrl,
    input  wire        sin,
    input  wire [7:0]  d,
    output wire [7:0]  q
);

    localparam CLR  = 3'b000;
    localparam LOAD = 3'b001;
    localparam SRL  = 3'b010;
    localparam SLL  = 3'b011;
    localparam SRA  = 3'b100;
    localparam SER  = 3'b101;
    localparam ROR  = 3'b110;
    localparam ROL  = 3'b111;

    wire [7:0] q_r;
    wire [7:0] q_next;

    MuxKeyWithDefault #(
        .NR_KEY   (8),
        .KEY_LEN  (3),
        .DATA_LEN (8)
    ) u_next (
        .out         (q_next),
        .key         (ctrl),
        .default_out (q_r),
        .lut         ({
            {CLR,  8'b0000_0000},
            {LOAD, d},
            {SRL,  1'b0, q_r[7:1]},
            {SLL,  q_r[6:0], 1'b0},
            {SRA,  q_r[7], q_r[7:1]},
            {SER,  sin, q_r[7:1]},
            {ROR,  q_r[0], q_r[7:1]},
            {ROL,  q_r[6:0], q_r[7]}
        })
    );

    Reg #(
        .WIDTH     (8),
        .RESET_VAL (8'b0)
    ) u_q (
        .clk  (clk),
        .rst  (!rst_n),
        .din  (q_next),
        .dout (q_r),
        .wen  (1'b1)
    );

    assign q = q_r;

endmodule

/*
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
*/
