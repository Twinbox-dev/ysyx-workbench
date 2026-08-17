/* top.v */
module top(
    input clk,
    input rst,
    output reg [15:0] led
);
    reg [31:0] count;
	always @(posedge clk) begin;
		if (rst) begin
			led   <= 16'b0000_0000_0000_0001;
			count <= 32'b0;
		end else begin
			if (count >= 32'd10) begin
            	led   <= {led[14:0], led[15]};
            	count <= 32'b0;
        	end else begin
            	led   <= led;
            	count <= count + 32'd1;
			end
		end
	end
endmodule
