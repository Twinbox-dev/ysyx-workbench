module encode42(
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

