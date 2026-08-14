# 配置部分
VERILATOR 	 = verilator
MODULE 		?= top 		# 默认顶层模块名
WAVES 		?= 0    	# 默认不生成波形 (0: 关闭, 1: VCD, 2: FST)

# 目录结构
VSRC_DIR  = vsrc
CSRC_DIR  = csrc
BUILD_DIR = build

# Verilator 编译选项
VERILATOR_FLAGS      = -Wall --cc --exe --build -j $(nproc)
ifeq ($(WAVES),1)
    VERILATOR_FLAGS += --trace
else ifeq ($(WAVES),2)
    VERILATOR_FLAGS += --trace-fst
endif

# 自动查找 Verilog 和 C++ 文件。但排除 example 文件
VSRCS = $(filter-out $(VSRC_DIR)/example.v,   $(wildcard $(VSRC_DIR)/*.v))
CSRCS = $(filter-out $(CSRC_DIR)/example.cpp, $(wildcard $(CSRC_DIR)/*.cpp))

# =========== 默认目标：编译并运行仿真 ============
all: sim run

# 编译 Verilator 仿真
sim:
	@mkdir -p $(BUILD_DIR)
	$(VERILATOR) $(VERILATOR_FLAGS) --top-module $(MODULE) $(VSRCS) $(CSRCS) --Mdir $(BUILD_DIR)	
	$(call git_commit, "sim RTL") # DO NOT REMOVE THIS LINE!!!

run:
	$(BUILD_DIR)/V$(MODULE)

clean:
	rm -rf $(BUILD_DIR) *.vcd *.fst

.PHONY: all sim run clean

include ../Makefile




#npc/
#├── vsrc/
#│   └── top.v          # Verilog 代码
#├── csrc/
#│   └── sim_top.cpp    # C++ 测试代码
#└── Makefile           # 通用 Makefile


