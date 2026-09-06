# =============================================================================
# 根目录唯一 Makefile —— 具体帮助见下方注释/执行make help
# -----------------------------------------------------------------------------
# 用法:
#   make                            # 显示帮助（默认目标）
#   make help                       # 显示帮助（默认目标）
#   make DIR=0_flow_led sim         # 编译并运行纯仿真
#   make DIR=0_flow_led run         # 编译并运行 nvboard 图形版本
#   make DIR=0_flow_led lint        # 只做语法/ lint 检查
#   make DIR=0_flow_led clean       # 删除该模块的构建产物
#   make clean_all                  # 删除根目录下整个 build 目录
#
# 可用变量:
#   DIR     (必需，除 help/clean_all 外) 模块文件夹，里面应有 vsrc/ 和 csrc/
#   VSRC    (可选) 指定 Verilog 文件，可直接写文件名(自动去 DIR/vsrc/ 下找)
#           也可写完整路径; 缺省使用 DIR/vsrc/*.v
#   MODULE  (可选) 顶层模块名，缺省 top
#   CSRC    (可选) 指定 C++ 文件；缺省按顶层模块名自动在 csrc/ 下找
# =============================================================================

# ---- 用户传入参数 ----
DIR    ?=            # 模块目录，例如 0_flow_led
VSRC   ?=            # 可选的 verilog 文件（支持文件名或路径）
MODULE ?= top        # 顶层模块名
CSRC   ?=            # 可选的 cpp 文件（一般不用手动指定）
WAVES  ?= 0          # 默认不生成波形 (0: 关闭, 1: VCD, 2: FST)

# ---- 检查 DIR（仅当用户执行了除 help/clean_all/lint 之外的目标时才强制）----
ifneq ($(filter-out help clean_all lint,$(MAKECMDGOALS)),)
ifeq ($(strip $(DIR)),)
$(error 用法: make DIR=<模块文件夹> [VSRC=...] [MODULE=...] sim|run|lint|clean)
endif
endif

# ---- 构建目录 ----
# 所有产物都放在 build/<模块目录名>/ 下面，避免污染源码目录
# 例如 build/0_flow_led/
BUILD_DIR := build/$(notdir $(patsubst %/,%,$(DIR)))

# ---- 推导 Verilog 文件列表 ----
# 如果没有指定 VSRC，就自动拿 DIR/vsrc/ 下所有 .v 文件
ifeq ($(strip $(VSRC)),)
VSRCS := $(wildcard $(DIR)/vsrc/*.v)
else
# 如果指定了 VSRC，则逐个文件尝试：
#   1. 如果 VSRC 只给了文件名，优先在 $(DIR)/vsrc/ 下找
#   2. 如果当前目录能直接找到该文件，使用当前目录的
#   3. 如果给了相对路径，尝试相对 DIR 目录
VSRCS := $(foreach f,$(VSRC),$(if $(wildcard $(DIR)/vsrc/$(f)),$(DIR)/vsrc/$(f),$(if $(wildcard $(f)),$(f),$(if $(wildcard $(DIR)/$(f)),$(DIR)/$(f),))))
ifneq ($(words $(VSRC)),$(words $(VSRCS)))
$(error 找不到 VSRC 文件: $(VSRC)，请检查文件名或路径)
endif
endif

# 如果最终没有 verilog 文件，也要报错
# 只有执行 run/sim/lint/clean 时才检查，make help/clean_all 时不触发
ifneq ($(filter-out help clean_all,$(MAKECMDGOALS)),)
ifeq ($(strip $(VSRCS)),)
$(error $(DIR)/vsrc/ 下没有 .v 文件，请用 VSRC=... 指定)
endif
endif

# ---- 推导 C++ 文件 ----
# 按顶层模块名 MODULE 寻找对应的 cpp：
#   1. DIR/csrc/<MODULE>-main.cpp  （例如 top-main.cpp）
#   2. DIR/csrc/main.cpp
# 取第一个存在的
ifeq ($(strip $(CSRC)),)
CPP_CAND := $(DIR)/csrc/$(MODULE)-main.cpp $(DIR)/csrc/main.cpp
CSRC     := $(firstword $(foreach f,$(CPP_CAND),$(if $(wildcard $(f)),$(f),)))
endif

# 如果没有找到 cpp 文件，报错提示用户
# 同样只在非 help/clean_all/lint 目标时检查
ifneq ($(filter-out help clean_all lint,$(MAKECMDGOALS)),)
ifeq ($(strip $(CSRC)),)
$(error 找不到 C++ 文件，请提供 $(DIR)/csrc/main.cpp 或 $(DIR)/csrc/$(MODULE)-main.cpp，或用 CSRC=... 指定)
endif
endif

# 统一转成绝对路径，方便后续 verilator 命令使用
VSRCS_ABS := $(abspath $(VSRCS))
CSRCS     := $(abspath $(CSRC))

# ---- 仿真 C++ 文件的额外处理 ----
# 如果存在 csrc/sim_main.cpp，仿真时单独用它；
# 否则使用我们上面找到的 main.cpp，并加上 -DSIM_ONLY 宏，
# 这样同一个 cpp 文件里可以用 #ifdef SIM_ONLY 区分仿真和 nvboard 模式
SIMCPP := $(wildcard $(DIR)/csrc/sim_main.cpp)
ifneq ($(SIMCPP),)
SIM_CSRCS  := $(abspath $(SIMCPP))   # 独立仿真 cpp
SIM_CFLAGS :=                        # 不需要额外宏
else
SIM_CSRCS  := $(CSRCS)               # 复用 nvboard 的 cpp
SIM_CFLAGS := -CFLAGS "-DSIM_ONLY"   # 加上宏
endif

# ---- verilator 编译配置 ----
VERILATOR_FLAGS      = -Wall --cc --exe --build -j $(nproc) -Wno-DECLFILENAME
ifeq ($(WAVES),1)
    VERILATOR_FLAGS += --trace
else ifeq ($(WAVES),2)
    VERILATOR_FLAGS += --trace-fst
endif

# ---- nvboard 相关配置 ----
# NVBOARD_HOME 是 nvboard 的安装目录，需要提前 export
NVBOARD_SRCS := $(wildcard $(NVBOARD_HOME)/src/*.cpp)   # nvboard 源码
SDL_CFLAGS   := $(shell sdl2-config --cflags)           # SDL2 编译参数
SDL_LIBS     := $(shell sdl2-config --libs) -lSDL2_image -lSDL2_ttf   # SDL2 链接参数
NVBOARD_LIB  := $(abspath $(BUILD_DIR)/nvboard.a)       # 生成的 nvboard 静态库
# 用不同子目录，彻底避免 nvboard / sim 可执行文件同名
BIN          := $(BUILD_DIR)/nvboard/$(MODULE)          # nvboard 可执行文件路径
SIMBIN       := $(BUILD_DIR)/sim/$(MODULE)              # 仿真可执行文件路径

# 默认目标：如果没有指定DIR则是help（直接 make 显示帮助）; 否则执行sim
ifeq ($(strip $(DIR)),)
    .DEFAULT_GOAL := help
else
    .DEFAULT_GOAL := sim
endif

# ---- 构建 nvboard 静态库 ----
# 把 nvboard 的所有 .cpp 编译成 .o，再打包成静态库
$(NVBOARD_LIB): $(NVBOARD_SRCS)
	mkdir -p $(BUILD_DIR)/nvboard
	cd $(BUILD_DIR)/nvboard && g++ -I$(NVBOARD_HOME)/include $(SDL_CFLAGS) -c $(NVBOARD_SRCS)
	ar rcs $@ $(BUILD_DIR)/nvboard/*.o

# ---- 构建 nvboard 版本的仿真程序 ----
$(BIN): $(VSRCS_ABS) $(CSRCS) $(NVBOARD_LIB)
	mkdir -p $(dir $@)
	verilator $(VERILATOR_FLAGS) --top-module $(MODULE) $(VSRCS_ABS) $(CSRCS) $(NVBOARD_LIB) \
		--Mdir $(BUILD_DIR)/obj_dir -o $(abspath $@) \
		-CFLAGS "-I$(NVBOARD_HOME)/usr/include $(SDL_CFLAGS)" \
		-LDFLAGS "$(SDL_LIBS)"
	$(call git_commit, "run RTL") # DO NOT REMOVE THIS LINE!!!

# ---- 构建纯仿真版本（不依赖 nvboard）----
$(SIMBIN): $(VSRCS_ABS) $(SIM_CSRCS)
	mkdir -p $(dir $@)
	verilator $(VERILATOR_FLAGS) --top-module $(MODULE) $(VSRCS_ABS) $(SIM_CSRCS) \
		--Mdir $(BUILD_DIR)/obj_dir_sim -o $(abspath $@) \
		$(SIM_CFLAGS)
	$(call git_commit, "sim RTL") # DO NOT REMOVE THIS LINE!!!

# ---- 用户目标 ----
# run：编译并运行 nvboard 版本
run: $(BIN)
	@$^

# sim：编译并运行纯仿真版本
sim: $(SIMBIN)
	@$^

# lint：只检查 verilog 语法，不编译
lint: $(VSRCS_ABS)
	@verilator --lint-only -Wall -Wno-DECLFILENAME --top-module $(MODULE) $(VSRCS_ABS)

# clean：删除当前模块的构建目录
clean:
	rm -rf $(BUILD_DIR)

# clean_all：删除根目录下的整个 build 目录
clean_all:
	rm -rf build *.vcd *.fst

# help：显示帮助信息
help:
	@echo "Usage: make DIR=<module_dir> [VSRC=...] [MODULE=...] <target>"
	@echo ""
	@echo "Targets:"
	@echo "  run       Compile and run nvboard version"
	@echo "  sim       Compile and run simulation only"
	@echo "  lint      Run verilator lint check"
	@echo "  clean     Remove build outputs for this module"
	@echo "  clean_all Remove entire build directory"
	@echo "  help      Show this help message"
	@echo ""
	@echo "Run 'make' without arguments to show this help."

# 声明这些是伪目标，避免和同名文件冲突
.PHONY: run sim lint clean clean_all help

# 引用父Makefile，为了调用git_commit
include $(NPC_HOME)/../Makefile