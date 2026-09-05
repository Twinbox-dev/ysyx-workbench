module top(
	input [1:0] X0,
	input [1:0] X1,
	input [1:0] X2,
	input [1:0] X3,
	input [1:0] Y ,

	output [1:0] F
);
	MuxKeyWithDefault #(
		.NR_KEY(4),
		.KEY_LEN(2),
		.DATA_LEN(2)
		) u1 (
		.out(F),
		.key(Y),
		.default_out(2'b00),
		.lut({
			2'b00,X0,
			2'b01,X1,
			2'b10,X2,
			2'b11,X3
			})
		);

endmodule
