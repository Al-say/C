CC = gcc
CFLAGS = -Wall -Wextra -O2 -I./include
LDFLAGS = 

# Debug build flags
ifdef DEBUG
    CFLAGS += -DDEBUG -g
else
    CFLAGS += -O3 -DNDEBUG
endif

# Source files
SRC = src/main.c src/parser.c src/generator.c src/utils.c
OBJ = $(SRC:.c=.o)
BIN = blog-generator

# Targets
.PHONY: all clean install debug test help

all: $(BIN)

debug:
	$(MAKE) DEBUG=1

$(BIN): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(BIN)
	rm -rf _site public

test: $(BIN)
	@echo "Running tests..."
	./$(BIN) test_site
	@echo "Testing complete"

install: $(BIN)
	mkdir -p /usr/local/bin
	cp $(BIN) /usr/local/bin/

help:
	@echo "可用的目标："
	@echo "  all       - 构建项目（默认目标）"
	@echo "  debug     - 构建带调试信息的版本"
	@echo "  clean     - 清理构建文件"
	@echo "  install   - 安装到系统"
	@echo "  test      - 运行测试"
	@echo "  help      - 显示此帮助信息"
	@echo
	@echo "使用示例："
	@echo "  make              - 构建正常版本"
	@echo "  make debug        - 构建调试版本"
	@echo "  make clean        - 清理构建文件"
