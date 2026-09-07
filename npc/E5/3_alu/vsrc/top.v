// =============================================================================
// 4位带符号补码 ALU
//   输入: A[3:0], B[3:0] (补码), S[2:0] (功能选择)
//   输出: Result[3:0], Zero, Overflow, Carry, Lt, Eq
//
// 功能表 (S[2:0]):
//   000 : A + B             (加法)
//   001 : A - B             (减法)
//   010 : ~A                (按位取反)
//   011 : A & B             (按位与)
//   100 : A | B             (按位或)
//   101 : A ^ B             (按位异或)
//   110 : A < B             (比较，结果 1/0)
//   111 : A == B            (相等，结果 1/0)
// =============================================================================

// 全加器（底层模块）
module full_adder (
    input  wire a, b, cin,
    output wire sum, cout
);
    assign sum  = a ^ b ^ cin;
    assign cout = (a & b) | (b & cin) | (a & cin);
endmodule


// 加减法器（补码：减法 = A + ~B + 1）
module adder_subtractor (
    input  wire        sub,     // 0=加法, 1=减法
    input  wire [3:0]  a,
    input  wire [3:0]  b,
    output wire [3:0]  result,  // 补码结果
    output wire        cout,    // 无符号进位/借位
    output wire        overflow,// 有符号溢出
    output wire        zero     // 零标志
);
    wire [3:0] b_in;
    wire c1, c2, c3;

    // sub=0 -> b_in=b; sub=1 -> b_in=~b
    assign b_in = b ^ {4{sub}};

    full_adder fa0 (.a(a[0]), .b(b_in[0]), .cin(sub), .sum(result[0]), .cout(c1));
    full_adder fa1 (.a(a[1]), .b(b_in[1]), .cin(c1),  .sum(result[1]), .cout(c2));
    full_adder fa2 (.a(a[2]), .b(b_in[2]), .cin(c2),  .sum(result[2]), .cout(c3));
    full_adder fa3 (.a(a[3]), .b(b_in[3]), .cin(c3),  .sum(result[3]), .cout(cout));

    // 溢出 = 最高位进位输入(c3) 异或 最高位进位输出(cout)
    assign overflow = c3 ^ cout;
    // 零标志 = 直接用result和0比较
    assign zero = (result == 4'b0000);
endmodule


/* verilator lint_off DECLFILENAME */
// =============================================================================
// 提供结构化建模的多路选择器模块
// =============================================================================
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

module MuxKey #(
	NR_KEY   = 2,
	KEY_LEN  = 1,
	DATA_LEN = 1
) (
	output [DATA_LEN-1:0] out,
	input  [KEY_LEN-1:0] key,
	input  [NR_KEY*(KEY_LEN + DATA_LEN)-1:0] lut
);
	MuxKeyInternal #(NR_KEY, KEY_LEN, DATA_LEN, 0) i0 (
		.out        (out),
		.key        (key),
		.default_out({DATA_LEN{1'b0}}),
		.lut        (lut)
	);
endmodule

module MuxKeyWithDefault #(
	NR_KEY   = 2,
	KEY_LEN  = 1,
	DATA_LEN = 1
) (
	output [DATA_LEN-1:0] out,
	input  [KEY_LEN-1:0] key,
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
// 顶层 ALU
// =============================================================================
module top (
    input  wire [3:0] A, B,    // 操作数（补码）
    input  wire [2:0] S,       // 功能选择
    output wire [3:0] Result,  // 运算结果
    output wire       Zero,    // 零标志 (Result == 0)
    output wire       Overflow,// 有符号溢出
    output wire       Carry,   // 无符号进位/借位
    output wire       Lt,      // A < B (有符号)
    output wire       Eq       // A == B
);

    // ---------- 算术运算单元 ----------
    wire [3:0] add_result, sub_result;
    wire       add_cout  , sub_cout;
    wire       add_of    , sub_of;
    /* verilator lint_off UNUSEDSIGNAL */
    wire       add_zero  , sub_zero;
    /* verilator lint_on UNUSEDSIGNAL */

    adder_subtractor add_u (
        .sub(1'b0), .a(A), .b(B),
        .result(add_result), .cout(add_cout),
        .overflow(add_of), .zero(add_zero)
    );

    adder_subtractor sub_u (
        .sub(1'b1), .a(A), .b(B),
        .result(sub_result), .cout(sub_cout),
        .overflow(sub_of), .zero(sub_zero)
    );

    // ---------- 逻辑运算单元 ----------
    wire [3:0] notA  = ~A;
    wire [3:0] andAB = A & B;
    wire [3:0] orAB  = A | B;
    wire [3:0] xorAB = A ^ B;

    // ---------- 比较单元（有符号） ----------
    // sub_result = A - B (补码)
    // 有符号 A < B:
    //   若 A-B 为负(Result[3]=1) 且 未溢出 -> 真
    //   若 A-B 为正(Result[3]=0) 且 溢出   -> 真   <-- Tip: 加减法器执行减法操作时，溢出意味着两个输入数符号相反
    //   Lt = (sub_result[3] & ~sub_of) | (~sub_result[3] & sub_of)
    //      = sub_result[3] ^ sub_of
    assign Lt = (S == 3'b110) ? (sub_result[3] ^ sub_of) : 1'b0;
    assign Eq = (S == 3'b111) ? sub_zero : 1'b0;

    // ---------- 多路选择器 (4位8选1) ----------
    MuxKeyWithDefault #(
	    .NR_KEY(4'd8),
	    .KEY_LEN(3),
	    .DATA_LEN(4)
        ) u1 (
        .out(Result),
        .key(S),
        .default_out(4'b0000),
        .lut({
            3'b000,add_result,
            3'b001,sub_result,
            3'b010,notA,
            3'b011,andAB,
            3'b100,orAB,
            3'b101,xorAB,
            3'b110,{3'b000, Lt},
            3'b111,{3'b000, Eq}
            })
        );

    /*
    reg [3:0] result_mux;
    always @(*) begin
        case (S)
            3'b000: result_mux = add_result;  // A + B
            3'b001: result_mux = sub_result;  // A - B
            3'b010: result_mux = notA;        // ~A
            3'b011: result_mux = andAB;       // A & B
            3'b100: result_mux = orAB;        // A | B
            3'b101: result_mux = xorAB;       // A ^ B
            3'b110: result_mux = {3'b000, Lt}; // A < B (结果 0/1)
            3'b111: result_mux = {3'b000, Eq}; // A == B (结果 0/1)
            default: result_mux = 4'b0000;
        endcase
    end
    assign Result = result_mux;
    */

    // ---------- 标志位生成 ----------
    // Zero: Result 全0
    assign Zero = (Result == 4'b0000);

    // Overflow / Carry: 仅加减法有意义，其余情况置0
    assign Overflow = (S == 3'b000) ? add_of :
                      (S == 3'b001) ? sub_of : 1'b0;

    assign Carry = (S == 3'b000) ? add_cout :
                   (S == 3'b001) ? sub_cout : 1'b0;

endmodule
