/* 行为级建模的方式构建的42优先编码器 */
module priority_encode42(
    input  [3:0] x,
    input  en,
    output reg [1:0] y
);

    integer i;

    always @(*) begin
        if (en) begin
            y = 2'b00;
            for (i = 0; i < 4; i = i + 1) begin
                if (x[i]) y = i[1:0];
            end
        end else begin
            y = 2'b00;
        end
    end

endmodule



/* 结构化建模搭建的42优先编码器 */
/* verilator lint_off DECLFILENAME */
/* verilator lint_off UNUSEDSIGNAL */
module priority_encode42_struct(
    input  [3:0] x,
    input  en,
    output [1:0] y
);
    // 先思考真值表，从真值表能推到门级结构
    // y[0] = en & (x[3] | (~x[2] & x[1]))
    // y[1] = en & (x[3] | x[2])

    wire n_x2;
    wire t1, t2, t3;

    not (n_x2, x[2]);        // n_x2 = ~x[2]
    and (t2, n_x2, x[1]);    // t2 = ~x[2] & x[1]
    or  (t3, x[3], t2);      // t3 = x[3] | (~x[2] & x[1])
    and (y[0], en, t3);      // y[0] = en & t3

    or  (t1, x[3], x[2]);    // t1 = x[3] | x[2]
    and (y[1], en, t1);      // y[1] = en & t1

endmodule
