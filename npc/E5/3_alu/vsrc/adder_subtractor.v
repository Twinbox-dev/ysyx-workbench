// 注意事项：计算机中存储的是补码,且正数的原码和补码相同。用于计算的也是补码(更准确的说法是加减法器只是忠实的执行加减法操作,其并不关注操作数是什么/怎么解读)
// 最终结果是由人类解读决定的：如果是按照无符号解读，则不看overflow;按照有符号解读则不看cout
/*
* cout    标志位：仅服务于无符号的解读方式。且sub的选择会影响其解读方式
* 	   - sub = 1：表示此时是减法，cout=1表示没有借位
* 	   - sub = 0: 表示此时是加法，cout=1表示有进位
*
* overflow标志位：服务于有符号的解读方式；1表示结果溢出不可信
*/
module adder_subtractor (
    input  wire        sub,     // 模式：0=加法，1=减法
    input  wire [3:0]  a,       // 操作数 A
    input  wire [3:0]  b,       // 操作数 B
    output wire [3:0]  result,  // 结果（永远是补码形式）
    output wire        cout,    // 无符号借位/进位标志 (1=无借位/有进位, 0=有借位/无进位)
    output wire        overflow,// 有符号溢出标志 (1=溢出)
    output wire        zero     // 零标志 (1=结果为0)
);

    wire [3:0] b_in; // 实际送给加法器的B
    wire c1, c2, c3;

    // 核心：sub=0时 b_in=b; sub=1时 b_in=~b (取反)
    assign b_in = b ^ {4{sub}};

    // 4个全加器级联
    full_adder fa0 (.a(a[0]), .b(b_in[0]), .cin(sub), .sum(result[0]), .cout(c1));
    full_adder fa1 (.a(a[1]), .b(b_in[1]), .cin(c1),  .sum(result[1]), .cout(c2));
    full_adder fa2 (.a(a[2]), .b(b_in[2]), .cin(c2),  .sum(result[2]), .cout(c3));
    full_adder fa3 (.a(a[3]), .b(b_in[3]), .cin(c3),  .sum(result[3]), .cout(cout));

    // 溢出 = 最高位进位输入(c3) 异或 最高位进位输出(cout)
    // 考虑以下三种加法的情况：
    // 两个正数相加结果变成负数：0 + 0 + cin <- cout必定等于0，那就只有cin=1时溢出
    // 两个负数相加结果变成正数：1 + 1 + cin <- cout必定等于1，那就只有cin=0时溢出
    // 一正一负相加：           0 + 1 + cin <- 结果不可能溢出，正好此时永远有cin==cout

    // 对应的减法是这四种：
    // 一个正数减去一个负数
    // 一个负数减去一个正数
    // 一个正数减去一个正数；一个负数减去一个负数
    /* 由此可总结出：减法操作溢出时->两个操作数的符号绝对不同；加法操作溢出时->两个操作数的符号绝对相同 */
    assign overflow = c3 ^ cout;

    // 结果全0则 zero=1
    assign zero = (result == 4'b0000);

endmodule

// 全加器（底层模块）
module full_adder (
    input  wire a, b, cin,
    output wire sum, cout
);
    assign sum  = a ^ b ^ cin;
    assign cout = (a & b) | (b & cin) | (a & cin);
endmodule
