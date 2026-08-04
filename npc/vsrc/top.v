module top(
    input a,
    input b,    
    output result
);
    // 双控开关逻辑：当a和b不同时输出1，相同时输出0
    assign result = a^b;
endmodule
