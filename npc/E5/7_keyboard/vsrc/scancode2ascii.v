// =============================================================================
// 扫描码 -> ASCII 码 转换 ROM
//
// 用 MuxKeyWithDefault 实现的查找表，每个表项的数据为 16 位：
//   {未按 Shift 时的 ASCII, 按下 Shift 时的 ASCII}
//
// 只收录字符键和数字键（以及常用符号），不收录组合键与小键盘。
// 表项数据为 0 表示该扫描码不是可显示字符，valid 输出 0。
//
// Caps Lock 只影响字母键：is_letter 由未按 Shift 时的 ASCII 是否落在
// 'a'~'z' 判断，因此数字键上的 Caps Lock 不会误把 1 变成 !。
// =============================================================================

module scancode2ascii (
    input  wire [7:0] code,   // 通码（第二套扫描码）
    input  wire       shift,  // Shift 是否按下
    input  wire       caps,   // Caps Lock 是否锁定
    output wire [7:0] ascii,
    output wire       valid   // 1 = 该扫描码有对应的可显示字符
);

    wire [15:0] pair;

    MuxKeyWithDefault #(52, 8, 16) rom (
        .out(pair),
        .key(code),
        .default_out(16'h0000),
        .lut({
            // ---- 字母键：小写 / 大写 ----
            8'h1C, 16'h6141,   // A  a / A
            8'h32, 16'h6242,   // B  b / B
            8'h21, 16'h6343,   // C  c / C
            8'h23, 16'h6444,   // D  d / D
            8'h24, 16'h6545,   // E  e / E
            8'h2B, 16'h6646,   // F  f / F
            8'h34, 16'h6747,   // G  g / G
            8'h33, 16'h6848,   // H  h / H
            8'h43, 16'h6949,   // I  i / I
            8'h3B, 16'h6A4A,   // J  j / J
            8'h42, 16'h6B4B,   // K  k / K
            8'h4B, 16'h6C4C,   // L  l / L
            8'h3A, 16'h6D4D,   // M  m / M
            8'h31, 16'h6E4E,   // N  n / N
            8'h44, 16'h6F4F,   // O  o / O
            8'h4D, 16'h7050,   // P  p / P
            8'h15, 16'h7151,   // Q  q / Q
            8'h2D, 16'h7252,   // R  r / R
            8'h1B, 16'h7353,   // S  s / S
            8'h2C, 16'h7454,   // T  t / T
            8'h3C, 16'h7555,   // U  u / U
            8'h2A, 16'h7656,   // V  v / V
            8'h1D, 16'h7757,   // W  w / W
            8'h22, 16'h7858,   // X  x / X
            8'h35, 16'h7959,   // Y  y / Y
            8'h1A, 16'h7A5A,   // Z  z / Z

            // ---- 数字键：数字 / 上档符号 ----
            8'h16, 16'h3121,   // 1  1 / !
            8'h1E, 16'h3240,   // 2  2 / @
            8'h26, 16'h3323,   // 3  3 / #
            8'h25, 16'h3424,   // 4  4 / $
            8'h2E, 16'h3525,   // 5  5 / %
            8'h36, 16'h365E,   // 6  6 / ^
            8'h3D, 16'h3726,   // 7  7 / &
            8'h3E, 16'h382A,   // 8  8 / *
            8'h46, 16'h3928,   // 9  9 / (
            8'h45, 16'h3029,   // 0  0 / )

            // ---- 符号键 ----
            8'h0E, 16'h607E,   // `  ` / ~
            8'h4E, 16'h2D5F,   // -  - / _
            8'h55, 16'h3D2B,   // =  = / +
            8'h54, 16'h5B7B,   // [  [ / {
            8'h5B, 16'h5D7D,   // ]  ] / }
            8'h5D, 16'h5C7C,   // \  \ / |
            8'h4C, 16'h3B3A,   // ;  ; / :
            8'h52, 16'h2722,   // '  ' / "
            8'h41, 16'h2C3C,   // ,  , / <
            8'h49, 16'h2E3E,   // .  . / >
            8'h4A, 16'h2F3F,   // /  / / ?

            // ---- 控制字符键 ----
            8'h29, 16'h2020,   // Space
            8'h5A, 16'h0D0D,   // Enter   CR
            8'h0D, 16'h0909,   // Tab     HT
            8'h66, 16'h0808,   // Back    BS
            8'h76, 16'h1B1B    // Esc     ESC
        })
    );

    wire [7:0] ascii_normal = pair[15:8];
    wire [7:0] ascii_shift  = pair[7:0];

    // Caps Lock 仅对字母生效，且与 Shift 叠加时相互抵消
    wire is_letter = (ascii_normal >= 8'h61) && (ascii_normal <= 8'h7A);
    wire use_shift = shift ^ (caps & is_letter);

    assign ascii = use_shift ? ascii_shift : ascii_normal;
    assign valid = (pair != 16'h0000);

endmodule
