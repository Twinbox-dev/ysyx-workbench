/*
 * RISC-V 指令模拟器 (基础版) - 带图形显示支持
 * 
 * 支持指令集: add, addi, jalr, lbu, lui, lw, sb, sw
 * 新增功能: sw 指令写入 [0x20000000, 0x20040000) 时保存像素到 framebuffer
 *           程序结束后通过 AM API 显示图像，并进入死循环
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <am.h>
#include <klib-macros.h>

// =======================================================================================
/* 寄存器定义 */
typedef enum {
    X0=0, X1, X2, X3, X4, X5, X6, X7,
    X8, X9, X10, X11, X12, X13, X14, X15,
    X16, X17, X18, X19, X20, X21, X22, X23,
    X24, X25, X26, X27, X28, X29, X30, X31
} riscv_reg;

#define zero X0
#define ra   X1
#define sp   X2
#define gp   X3
#define tp   X4
#define t0   X5
#define t1   X6
#define t2   X7
#define s0   X8
#define fp   X8
#define s1   X9
#define a0   X10
#define a1   X11
#define a2   X12
#define a3   X13
#define a4   X14
#define a5   X15
#define a6   X16
#define a7   X17
#define s2   X18
#define s3   X19
#define s4   X20
#define s5   X21
#define s6   X22
#define s7   X23
#define s8   X24
#define s9   X25
#define s10  X26
#define s11  X27
#define t3   X28
#define t4   X29
#define t5   X30
#define t6   X31

/* 操作码定义 (7-bit) */
#define OP_ADD     0b0110011
#define OP_ADDI    0b0010011
#define OP_EBREAK  0b1110011
#define OP_JALR	   0b1100111
#define OP_LUI     0b0110111
#define OP_LOAD    0b0000011
#define OP_STORE   0b0100011

/* 模拟器状态结构体 - 内存大小 4 MB */
#define MEM_SIZE  (1024 * 1024)  // 1M words = 4 MB
struct SEMU_STATE {
    uint32_t PC;
    uint32_t R[32];
    uint32_t M[MEM_SIZE];
} semu = { .PC = 0, .R = {0}, .M = {0} };

// ==============================================================================================

/* framebuffer 相关全局变量 */
static uint32_t *fb = NULL;
static int fb_w = 256, fb_h = 256; // 强制 256x256
static const uint32_t FB_BASE = 0x20000000; // 显存基地址

/* 符号扩展函数 */
static inline uint32_t sign_extend(uint32_t imm12) {
    if (imm12 & 0x800) return imm12 | 0xFFFFF000;
    else return imm12;
}

/* 写寄存器函数：保证0号寄存器始终为0 */
static inline void write_reg(uint32_t rd, uint32_t value) {
    if (rd != 0) semu.R[rd] = value;
}

/* 内存字节访问辅助函数 (小端序) - 增加边界检查 */
static inline uint8_t mem_read_byte(uint32_t addr) {
    uint32_t idx = addr / 4;
    if (idx >= MEM_SIZE) return 0;
    uint32_t byte_off = addr % 4;
    return (semu.M[idx] >> (byte_off * 8)) & 0xFF;
}

static inline void mem_write_byte(uint32_t addr, uint8_t value) {
    uint32_t idx = addr / 4;
    if (idx >= MEM_SIZE) return;
    uint32_t byte_off = addr % 4;
    uint32_t mask = ~(0xFF << (byte_off * 8));
    semu.M[idx] = (semu.M[idx] & mask) | ((uint32_t)value << (byte_off * 8));
}

static inline uint32_t mem_read_word(uint32_t addr) {
    uint32_t idx = addr / 4;
    if (idx >= MEM_SIZE) return 0;
    return semu.M[idx];
}

static inline void mem_write_word(uint32_t addr, uint32_t value) {
    uint32_t idx = addr / 4;
    if (idx >= MEM_SIZE) return;
    semu.M[idx] = value;
}

/* 指令周期 */
void inst_cycle(void) {
    if (semu.PC / 4 >= MEM_SIZE) {
        printf("PC 越界: 0x%08X\n", semu.PC);
        return;
    }

    uint32_t inst = semu.M[semu.PC / 4];
    uint32_t opcode = inst & 0x7F;

    switch (opcode) {
        case OP_ADD: {
            uint32_t rd  = (inst >> 7) & 0x1F;
            uint32_t rs1 = (inst >> 15) & 0x1F;
            uint32_t rs2 = (inst >> 20) & 0x1F;
            write_reg(rd, semu.R[rs1] + semu.R[rs2]);
            break;
        }
        case OP_ADDI: {
            uint32_t rd  = (inst >> 7) & 0x1F;
            uint32_t rs1 = (inst >> 15) & 0x1F;
            uint32_t imm = sign_extend((inst >> 20) & 0xFFF);
            write_reg(rd, semu.R[rs1] + imm);
            break;
        }
        case OP_JALR: {
            uint32_t rd  = (inst >> 7) & 0x1F;
            uint32_t rs1 = (inst >> 15) & 0x1F;
            uint32_t imm = sign_extend((inst >> 20) & 0xFFF);
            uint32_t t = semu.PC + 4;
            uint32_t target = (semu.R[rs1] + imm) & ~1;
            write_reg(rd, t);
            semu.PC = target;
            return;
        }
        case OP_LUI: {
            uint32_t rd  = (inst >> 7) & 0x1F;
            uint32_t imm = (inst >> 12) & 0xFFFFF;
            if (imm & 0x80000) imm |= 0xFFF00000;
            write_reg(rd, imm << 12);
            break;
        }
        case OP_LOAD: {
            uint32_t rd     = (inst >> 7) & 0x1F;
            uint32_t rs1    = (inst >> 15) & 0x1F;
            uint32_t funct3 = (inst >> 12) & 0x7;
            uint32_t imm    = sign_extend((inst >> 20) & 0xFFF);
            uint32_t addr   = semu.R[rs1] + imm;

            // 显存读取（从 fb 读取）
            if (fb != NULL && addr >= FB_BASE && addr < FB_BASE + fb_w * fb_h * 4) {
                uint32_t index = (addr - FB_BASE) / 4;
                if (index < fb_w * fb_h) {
                    if (funct3 == 0x2) write_reg(rd, fb[index]);
                    else if (funct3 == 0x4) write_reg(rd, (fb[index] >> ((addr % 4)*8)) & 0xFF);
                }
                break;
            }

            // 正常内存读取
            if (funct3 == 0x4) write_reg(rd, mem_read_byte(addr));
            else if (funct3 == 0x2) write_reg(rd, mem_read_word(addr));
            break;
        }
        case OP_STORE: {
            uint32_t rs1    = (inst >> 15) & 0x1F;
            uint32_t rs2    = (inst >> 20) & 0x1F;
            uint32_t funct3 = (inst >> 12) & 0x7;
            uint32_t imm = ((inst >> 25) << 5) | ((inst >> 7) & 0x1F);
            imm = sign_extend(imm);
            uint32_t addr = semu.R[rs1] + imm;

            // 显存写入（只更新 fb，不写入主存）
            if (fb != NULL && addr >= FB_BASE && addr < FB_BASE + fb_w * fb_h * 4) {
                uint32_t index = (addr - FB_BASE) / 4;
                if (index < fb_w * fb_h) {
                    if (funct3 == 0x2) fb[index] = semu.R[rs2];
                }
                break;
            }

            // 正常内存写入
            if (funct3 == 0x0) mem_write_byte(addr, semu.R[rs2] & 0xFF);
            else if (funct3 == 0x2) mem_write_word(addr, semu.R[rs2]);
            break;
        }
        case OP_EBREAK: {
            // 忽略 ebreak，继续执行
            break;
        }
        default: {
            printf("未知指令: 0x%08X (PC=0x%08X)\n", inst, semu.PC);
            exit(1);
        }
    }
    semu.PC += 4;
}

/* 从文件加载程序 */
void load_program_from_file(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        printf("无法打开文件: %s\n", filename);
        exit(1);
    }
    uint32_t addr = 0;
    uint32_t buffer;
    while (fread(&buffer, 1, 4, file) == 4) {
        if (addr / 4 >= MEM_SIZE) {
            printf("文件太大，超出内存容量 (最大 %d 字节)\n", MEM_SIZE * 4);
            exit(1);
        }
        semu.M[addr / 4] = buffer;
        addr += 4;
    }
    fclose(file);
}

int main(int argc, char *argv[]) {
    ioe_init();
    // 强制设为 256x256，不从 AM 读取
    fb = (uint32_t*)malloc(fb_w * fb_h * sizeof(uint32_t));
    if (!fb) {
        printf("无法分配 framebuffer\n");
        return 1;
    }
    memset(fb, 0, fb_w * fb_h * sizeof(uint32_t));

    load_program_from_file("./vga.bin");
    semu.PC = 0;

    // 添加指令计数上限，防止死循环导致无法显示
    const long long MAX_INSTR = 5000000; // 500万条指令
    long long instr_count = 0;

    while (semu.PC / 4 < MEM_SIZE && instr_count < MAX_INSTR) {
        inst_cycle();
        instr_count++;
    }

    printf("程序执行完毕（执行 %lld 条指令），显示图像...\n", instr_count);
    io_write(AM_GPU_FBDRAW, 0, 0, fb, fb_w, fb_h, true);

    while (1); // 保持窗口
    return 0;
}