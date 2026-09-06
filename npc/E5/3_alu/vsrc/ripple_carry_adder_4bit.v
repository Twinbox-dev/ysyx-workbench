// 文件名：ripple_carry_adder_4bit.v
// 单文件包含全加器子模块和四位波纹进位加法器顶层模块

// ==========================================
// 模块1：1-bit 全加器 (子模块)
// ==========================================
module full_adder (
    input  wire a,      // 加数1
    input  wire b,      // 加数2
    input  wire cin,    // 进位输入
    output wire sum,    // 本位和
    output wire cout    // 进位输出
);
    assign sum  = a ^ b ^ cin;
    assign cout = (a & b) | (b & cin) | (a & cin);
endmodule


// ==========================================
// 模块2：4-bit 波纹进位加法器 (顶层模块)
// ==========================================
module ripple_carry_adder_4bit (
    input  wire [3:0] a,    // 4位操作数 A
    input  wire [3:0] b,    // 4位操作数 B
    input  wire       cin,  // 初始进位
    output wire [3:0] sum,  // 4位和,
    output wire       cout  // 表示最终进位 <- sum 溢出
);

    // 内部进位连线
    wire c1, c2, c3;
    // 实例化4个全加器，级联形成波纹进位
    full_adder fa0 (.a(a[0]), .b(b[0]), .cin(cin), .sum(sum[0]), .cout(c1));
    full_adder fa1 (.a(a[1]), .b(b[1]), .cin(c1) , .sum(sum[1]), .cout(c2));
    full_adder fa2 (.a(a[2]), .b(b[2]), .cin(c2) , .sum(sum[2]), .cout(c3));
    full_adder fa3 (.a(a[3]), .b(b[3]), .cin(c3) , .sum(sum[3]), .cout(cout));
endmodule

/*
module top(
	input  wire [3:0] a,
	input  wire [3:0] b,
	input  wire       cin,
	output wire [3:0] sum,
    output wire       cout       
);
	ripple_carry_adder_4bit u0 (
		.a(a),
		.b(b),
		.cin(cin),
		.sum(sum),
        .cout(cout)
	);

endmodule
*/
