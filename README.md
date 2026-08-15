# 一生一芯 学习项目

本人基于"一生一芯"(ysyx)计划的学习仓库: 从零开始设计、实现并验证一颗 RISC-V 处理器(NPC)。

## 目录结构

| 目录 | 说明 |
| ---- | ---- |
| `npc/` | 自研 RISC-V 处理器 RTL 源码(Verilog), 通过 Verilator 仿真 |
| `nvboard/` | 虚拟 FPGA 开发板(基于 SDL2), 模拟 LED/数码管/开关/VGA/UART 等外设 |
| `nemu/` | NEMU 教学模拟器, 作为处理器实现的参考模型 |
| `abstract-machine/` | AM 抽象机器, 提供裸机程序运行时 |
| `init.sh` | 子项目初始化脚本(克隆仓库并写入环境变量) |
| `Makefile` | 顶层构建规则, 内含 git 自动提交机制 |

## 环境变量

各子项目根目录通过 `init.sh` 写入 `~/.bashrc`:

- `NEMU_HOME` — nemu
- `NPC_HOME` — npc
- `NVBOARD_HOME` — nvboard
- `AM_HOME` — abstract-machine

## 快速开始

以虚拟板卡上的流水灯为例:

```bash
cd $NVBOARD_HOME/simple-example
make run
```

## 当前进度

- [x] 接入 NVBoard, 在虚拟板卡上跑通流水灯
- [x] NPC 支持 Verilator 仿真与 FST 波形导出(`make view`)
- [x] Makefile 添加 lint 规则

更多资料可参考["一生一芯"实验讲义](https://ysyx.oscc.cc/docs/2407/e/5.html)。
