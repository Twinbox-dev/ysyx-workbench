TOPNAME = top
VSRCS = $(abspath vsrc/top.v)
CSRCS = $(abspath csrc/main.cpp)
NVBOARD_SRCS = $(wildcard $(NVBOARD_HOME)/src/*.cpp)

SDL_CFLAGS = $(shell sdl2-config --cflags)
SDL_LIBS   = $(shell sdl2-config --libs) -lSDL2_image -lSDL2_ttf

# 依赖里任何一个源文件更新都会重新执行下面三条命令
build/top: $(VSRCS) $(CSRCS) $(NVBOARD_SRCS)
	mkdir -p build/nvboard
	cd build/nvboard && g++ -I$(NVBOARD_HOME)/include $(SDL_CFLAGS) -c $(NVBOARD_SRCS)
	ar rcs build/nvboard.a build/nvboard/*.o
	verilator --cc --exe --build --top-module $(TOPNAME) $(VSRCS) $(CSRCS) $(abspath build/nvboard.a) \
		--Mdir build/obj_dir -o $(abspath $@) \
		-CFLAGS "-I$(NVBOARD_HOME)/usr/include $(SDL_CFLAGS)" \
		-LDFLAGS "$(SDL_LIBS)"

run: build/top
	./build/top

clean:
	rm -rf build

.PHONY: run clean
