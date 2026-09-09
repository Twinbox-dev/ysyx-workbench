// =============================================================================
// 按键处理状态机
//
// 从 ps2_keyboard 的 FIFO 中逐字节取出扫描码，识别出"哪个键被按下/放开"，
// 并维护显示所需的全部状态。
//
// 这里其实是两级状态机的组合：
//
// 1) 取数握手状态机（state）：负责与 ps2_keyboard 的 ready/nextdata_n 握手
//      S_IDLE --ready--> S_ACK --> S_IDLE
//    在 S_ACK 这一个周期内把 nextdata_n 拉低，同时完成对该字节的处理；
//    ps2_keyboard 在该周期末把读指针前移，因此 data 在整个 S_ACK 期间稳定。
//
// 2) 扫描码序列状态机（prefix）：负责识别多字节的扫描码序列
//      P_NORMAL --0xE0--> P_EXT     --0xF0--> P_EXT_BRK --键值--> P_NORMAL
//          |  \--0xF0--> P_BRK --键值--> P_NORMAL
//          \--键值--> P_NORMAL
//    即 0xE0 表示扩展键前缀，0xF0 表示断码前缀，两者可以叠加（E0 F0 xx）。
//
// 组合键（Shift / Ctrl / Alt）不参与"当前按键"的显示，也不计入按键次数，
// 它们只维护自己的按下状态，供 ASCII 转换和 LED 指示使用。这样组合键与
// 字符键同时按下时互不冲突。
// =============================================================================

module key_ctrl (
    input  wire       clk,
    input  wire       rst,          // 高有效复位

    // 与 ps2_keyboard 的接口
    input  wire [7:0] data,
    input  wire       ready,
    output wire       nextdata_n,

    // 当前按键
    output reg  [7:0] key_code,     // 通码
    output reg  [7:0] key_ascii,    // 对应 ASCII（按下瞬间锁存）
    output reg        key_has_ascii,// 1 = 该键有对应的可显示字符
    output reg        key_down,     // 1 = 当前有字符键被按住

    // 按键计数（按住不放只算一次）
    output reg  [7:0] key_count,

    // 组合键状态
    output reg        lshift, rshift,
    output reg        lctrl,  rctrl,
    output reg        lalt,   ralt,
    output reg        capslock
);

    // ---------------- 取数握手状态机 ----------------
    localparam S_IDLE = 1'b0;
    localparam S_ACK  = 1'b1;

    reg state;

    always @(posedge clk) begin
        if (rst)                       state <= S_IDLE;
        else if (state == S_IDLE)      state <= ready ? S_ACK : S_IDLE;
        else                           state <= S_IDLE;
    end

    // 在 S_ACK 周期拉低一个时钟，通知 ps2_keyboard 数据已取走
    assign nextdata_n = (state == S_ACK) ? 1'b0 : 1'b1;

    wire byte_valid = (state == S_ACK);

    // ---------------- 扫描码序列状态机 ----------------
    localparam P_NORMAL  = 2'b00;
    localparam P_EXT     = 2'b01;   // 收到 0xE0
    localparam P_BRK     = 2'b10;   // 收到 0xF0
    localparam P_EXT_BRK = 2'b11;   // 收到 0xE0 0xF0

    reg [1:0] prefix;

    wire is_e0 = (data == 8'hE0);
    wire is_f0 = (data == 8'hF0);

    wire is_ext = (prefix == P_EXT) || (prefix == P_EXT_BRK);
    wire is_brk = (prefix == P_BRK) || (prefix == P_EXT_BRK);

    // 当前字节是一个真正的键值（不是前缀）
    wire is_key = byte_valid && !is_e0 && !is_f0;

    always @(posedge clk) begin
        if (rst) prefix <= P_NORMAL;
        else if (byte_valid) begin
            if (is_e0)      prefix <= P_EXT;                       // 进入扩展键前缀
            else if (is_f0) prefix <= is_ext ? P_EXT_BRK : P_BRK;  // 叠加断码前缀
            else            prefix <= P_NORMAL;                    // 键值收完，复位
        end
    end

    // ---------------- 组合键识别 ----------------
    // 左右 Ctrl / Alt 的扫描码相同，靠扩展前缀区分
    wire hit_lshift = is_key && !is_ext && (data == 8'h12);
    wire hit_rshift = is_key && !is_ext && (data == 8'h59);
    wire hit_lctrl  = is_key && !is_ext && (data == 8'h14);
    wire hit_rctrl  = is_key &&  is_ext && (data == 8'h14);
    wire hit_lalt   = is_key && !is_ext && (data == 8'h11);
    wire hit_ralt   = is_key &&  is_ext && (data == 8'h11);
    wire hit_caps   = is_key && !is_ext && (data == 8'h58);

    wire is_modifier = hit_lshift | hit_rshift | hit_lctrl
                     | hit_rctrl  | hit_lalt   | hit_ralt | hit_caps;

    reg caps_held;   // Caps Lock 当前是否按住，用于抑制重复通码

    always @(posedge clk) begin
        if (rst) begin
            lshift <= 1'b0; rshift <= 1'b0;
            lctrl  <= 1'b0; rctrl  <= 1'b0;
            lalt   <= 1'b0; ralt   <= 1'b0;
            capslock  <= 1'b0;
            caps_held <= 1'b0;
        end
        else begin
            // 通码置位、断码清零；按住时重复送来的通码是幂等的
            if (hit_lshift) lshift <= ~is_brk;
            if (hit_rshift) rshift <= ~is_brk;
            if (hit_lctrl)  lctrl  <= ~is_brk;
            if (hit_rctrl)  rctrl  <= ~is_brk;
            if (hit_lalt)   lalt   <= ~is_brk;
            if (hit_ralt)   ralt   <= ~is_brk;

            // Caps Lock 是锁定键，只在"按下的那一次"翻转。
            // 键盘按住不放会重复送出通码，靠 caps_held 做边沿检测，
            // 否则按住 Caps 会让锁定状态反复翻转。
            if (hit_caps) begin
                caps_held <= ~is_brk;
                if (!is_brk && !caps_held) capslock <= ~capslock;
            end
        end
    end

    // ---------------- ASCII 转换 ----------------
    wire shift_on = lshift | rshift;
    wire [7:0] ascii;
    wire       ascii_valid;

    scancode2ascii rom (
        .code(data),
        .shift(shift_on),
        .caps(capslock),
        .ascii(ascii),
        .valid(ascii_valid)
    );

    // ---------------- 当前按键 + 按键计数 ----------------
    // held_ext 一起比较，保证扩展键与同码的普通键不会互相误判
    reg held_ext;

    // 按住不放时键盘会重复送出通码，只有"新的键"才计数
    wire same_key = key_down && (key_code == data) && (held_ext == is_ext);
    wire key_make  = is_key && !is_modifier && !is_brk;
    wire key_break = is_key && !is_modifier &&  is_brk;

    always @(posedge clk) begin
        if (rst) begin
            key_code      <= 8'h00;
            key_ascii     <= 8'h00;
            key_has_ascii <= 1'b0;
            key_down      <= 1'b0;
            held_ext      <= 1'b0;
            key_count     <= 8'h00;
        end
        else if (key_make) begin
            if (!same_key) begin
                key_code      <= data;
                key_ascii     <= ascii;        // 按下瞬间锁存，之后改变 Shift 不影响显示
                key_has_ascii <= ascii_valid;
                key_down      <= 1'b1;
                held_ext      <= is_ext;
                key_count     <= key_count + 8'd1;
            end
        end
        else if (key_break) begin
            // 只有放开的正是当前按住的键，才熄灭显示
            if (same_key) key_down <= 1'b0;
        end
    end

endmodule
