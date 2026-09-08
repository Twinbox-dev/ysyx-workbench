// =====================================================================
// MuxKey 系列
// =====================================================================
/* verilator lint_off DECLFILENAME */
module MuxKeyInternal #(
    NR_KEY = 2,
    KEY_LEN = 1,
    DATA_LEN = 1,
    HAS_DEFAULT = 0
) (
    output reg [DATA_LEN-1:0] out,
    input  [KEY_LEN-1:0] key,
    input  [DATA_LEN-1:0] default_out,
    input  [NR_KEY*(KEY_LEN + DATA_LEN)-1:0] lut
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
        hit = 0;
        for (i = 0; i < NR_KEY; i = i + 1) begin
            lut_out = lut_out | ({DATA_LEN{key == key_list[i]}} & data_list[i]);
            hit = hit | (key == key_list[i]);
        end
        if (!HAS_DEFAULT) begin
            out = lut_out;
        end
        else begin
            out = (hit ? lut_out : default_out);
        end
    end

endmodule


module MuxKey #(
    NR_KEY = 2,
    KEY_LEN = 1,
    DATA_LEN = 1
) (
    output [DATA_LEN-1:0] out,
    input  [KEY_LEN-1:0] key,
    input  [NR_KEY*(KEY_LEN + DATA_LEN)-1:0] lut
);
    MuxKeyInternal #(NR_KEY, KEY_LEN, DATA_LEN, 0) i0 (
        .out(out),
        .key(key),
        .default_out({DATA_LEN{1'b0}}),
        .lut(lut)
    );
endmodule
/* verilator lint_on DECLFILENAME */

// =====================================================================
// 8位桶形移位器 - 需要实例化3个4路8bit输入的选择器
//
// 控制编码（每个 MUX4_1）：
//   sel[1] = L/R (1=左移, 0=右移)
//   sel[0] = 当前级是否移动 (shamt[i])
//
//   key  ->  选择内容
//   00   ->  原值（右移但不移动）
//   01   ->  右移结果（右移且移动）
//   10   ->  原值（左移但不移动）
//   11   ->  左移结果（左移且移动）
// =====================================================================

module barrel_shifter (
    input  wire [7:0] din,
    input  wire [2:0] shamt,
    input  wire       lr,      // 1=左移, 0=右移
    input  wire       al,      // 1=算术, 0=逻辑
    output wire [7:0] dout
);

    // ================= 第1级：移 1 位 =================
    wire [7:0] right1 = al ? {din[7], din[7:1]} : {1'b0, din[7:1]};
    wire [7:0] left1  = {din[6:0], 1'b0};
    wire [7:0] s1;
    wire [1:0] sel1 = {lr, shamt[0]};

    MuxKey #(4, 2, 8) mux_stage1 (
        .out(s1),
        .key(sel1),
        .lut({
            {2'b11, left1},
            {2'b10, din},
            {2'b01, right1},
            {2'b00, din}
        })
    );

    // ================= 第2级：移 2 位 =================
    wire [7:0] right2 = al ? {{2{s1[7]}}, s1[7:2]} : {2'b00, s1[7:2]};
    wire [7:0] left2  = {s1[5:0], 2'b00};
    wire [7:0] s2;
    wire [1:0] sel2 = {lr, shamt[1]};

    MuxKey #(4, 2, 8) mux_stage2 (
        .out(s2),
        .key(sel2),
        .lut({
            {2'b11, left2},
            {2'b10, s1},
            {2'b01, right2},
            {2'b00, s1}
        })
    );

    // ================= 第3级：移 4 位 =================
    wire [7:0] right4 = al ? {{4{s2[7]}}, s2[7:4]} : {4'b0000, s2[7:4]};
    wire [7:0] left4  = {s2[3:0], 4'b0000};
    wire [7:0] s3;
    wire [1:0] sel3 = {lr, shamt[2]};

    MuxKey #(4, 2, 8) mux_stage3 (
        .out(s3),
        .key(sel3),
        .lut({
            {2'b11, left4},
            {2'b10, s2},
            {2'b01, right4},
            {2'b00, s2}
        })
    );

    assign dout = s3;

endmodule
