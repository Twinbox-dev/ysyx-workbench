module top(
    input        en,      // SW8 - enable
    input  [7:0] x,       // SW7-SW0 - 8-bit input
    output [3:0] led,     // LD3-LD0: led[2:0] = encoded result, led[3] = valid bit
    output [7:0] seg      // SEG0: 7-segment display output (active-low)
);

// --- 8-to-3 priority encoder (MSB first) with valid bit ---
reg [2:0] code;
reg       valid;

always @(*) begin
    if (en) begin
        valid = (x != 8'b0);
        casez (x)
            8'b1???????: code = 3'd7;
            8'b01??????: code = 3'd6;
            8'b001?????: code = 3'd5;
            8'b0001????: code = 3'd4;
            8'b00001???: code = 3'd3;
            8'b000001??: code = 3'd2;
            8'b0000001?: code = 3'd1;
            8'b00000001: code = 3'd0;
            default:     code = 3'd0;
        endcase
    end else begin
        code  = 3'd0;
        valid = 1'b0;
    end
end

assign led = {valid, code};

// --- 7-segment decoder (active-low, 0=on) ---
// seg = {DEC_P, G, F, E, D, C, B, A}
reg [7:0] seg_pattern;

always @(*) begin
    if (!en) begin
        seg_pattern = 8'hFF;  // all off when disabled
    end else begin
        case (code)
            3'd0: seg_pattern = 8'b00000010;  // 0
            3'd1: seg_pattern = 8'b10011110;  // 1
            3'd2: seg_pattern = 8'b00100100;  // 2
            3'd3: seg_pattern = 8'b00001100;  // 3
            3'd4: seg_pattern = 8'b10011000;  // 4
            3'd5: seg_pattern = 8'b01001000;  // 5
            3'd6: seg_pattern = 8'b01000000;  // 6
            3'd7: seg_pattern = 8'b00011110;  // 7
            default: seg_pattern = 8'hFF;     // off
        endcase
    end
end

assign seg = seg_pattern;

endmodule
