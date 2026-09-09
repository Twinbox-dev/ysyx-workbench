// =============================================================================
// 实验七 顶层模块：PS/2 键盘按键的键码与 ASCII 码显示
//
// 数据通路：
//   PS/2 串行输入 -> ps2_keyboard(接收+FIFO) -> key_ctrl(状态机) -> 七段数码管
//
// 七段数码管分配（seg0 是最右边一位）：
//   seg1 seg0 : 当前按键的键码（通码，两位十六进制）
//   seg3 seg2 : 对应的 ASCII 码（两位十六进制）
//   seg5 seg4 : 按键总次数（两位十六进制，按住不放只算一次）
//   seg7 seg6 : 不使用，恒灭
//
// 按键松开时低四位（seg3..seg0）全灭，按键次数保持显示。
//
// LED 指示：
//   LD0 : Shift（左右任一）      LD1 : Ctrl（左右任一）
//   LD2 : Alt（左右任一）        LD3 : Caps Lock 锁定
//   LD4 : 键盘控制器 ready       LD5 : FIFO overflow
// =============================================================================

module top (
    input  wire       clk,
    input  wire       rst,        // 高有效复位

    input  wire       ps2_clk,
    input  wire       ps2_data,

    output wire [7:0] led,
    output wire [7:0] seg0,
    output wire [7:0] seg1,
    output wire [7:0] seg2,
    output wire [7:0] seg3,
    output wire [7:0] seg4,
    output wire [7:0] seg5,
    output wire [7:0] seg6,
    output wire [7:0] seg7
);

    // ---------------- PS/2 接收控制器 ----------------
    wire [7:0] ps2_byte;
    wire       ps2_ready;
    wire       ps2_overflow;
    wire       nextdata_n;

    ps2_keyboard kbd (
        .clk        (clk),
        .clrn       (~rst),
        .ps2_clk    (ps2_clk),
        .ps2_data   (ps2_data),
        .nextdata_n (nextdata_n),
        .data       (ps2_byte),
        .ready      (ps2_ready),
        .overflow   (ps2_overflow)
    );

    // ---------------- 按键处理状态机 ----------------
    wire [7:0] key_code;
    wire [7:0] key_ascii;
    wire       key_has_ascii;
    wire       key_down;
    wire [7:0] key_count;
    wire       lshift, rshift, lctrl, rctrl, lalt, ralt, capslock;

    key_ctrl ctrl (
        .clk           (clk),
        .rst           (rst),
        .data          (ps2_byte),
        .ready         (ps2_ready),
        .nextdata_n    (nextdata_n),
        .key_code      (key_code),
        .key_ascii     (key_ascii),
        .key_has_ascii (key_has_ascii),
        .key_down      (key_down),
        .key_count     (key_count),
        .lshift        (lshift),
        .rshift        (rshift),
        .lctrl         (lctrl),
        .rctrl         (rctrl),
        .lalt          (lalt),
        .ralt          (ralt),
        .capslock      (capslock)
    );

    // ---------------- 七段数码管 ----------------
    // 键码在按住期间显示；ASCII 还要求该键确实是可显示字符
    wire blank_code  = ~key_down;
    wire blank_ascii = ~key_down | ~key_has_ascii;

    seg7_hex s0 (.hex(key_code [3:0]), .blank(blank_code),  .seg(seg0));
    seg7_hex s1 (.hex(key_code [7:4]), .blank(blank_code),  .seg(seg1));
    seg7_hex s2 (.hex(key_ascii[3:0]), .blank(blank_ascii), .seg(seg2));
    seg7_hex s3 (.hex(key_ascii[7:4]), .blank(blank_ascii), .seg(seg3));

    // 按键次数常显
    seg7_hex s4 (.hex(key_count[3:0]), .blank(1'b0), .seg(seg4));
    seg7_hex s5 (.hex(key_count[7:4]), .blank(1'b0), .seg(seg5));

    // 高两位不使用
    assign seg6 = 8'hFF;
    assign seg7 = 8'hFF;

    // ---------------- LED 状态指示 ----------------
    assign led = {
        2'b00,
        ps2_overflow,
        ps2_ready,
        capslock,
        lalt   | ralt,
        lctrl  | rctrl,
        lshift | rshift
    };

endmodule
