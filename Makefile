# ESP32 开发 Makefile —— 全部工具留在项目内，不污染系统
CLI    := ./.tools/arduino-cli
CONFIG := arduino-cli.yaml
CLI_CFG := $(CLI) --config-file $(CONFIG)

# ↓↓↓ 换板子只改这一行 ↓↓↓
# esp32:esp32:esp32      = ESP32 DevKitC / WROOM-32（默认）
# esp32:esp32:esp32s3    = ESP32-S3
# esp32:esp32:esp32c3    = ESP32-C3
# esp32:esp32:esp32c6    = ESP32-C6
FQBN   ?= esp32:esp32:esp32

SKETCH ?= src/foundation
BAUD   ?= 115200
# 每个 sketch 独立 build 目录，避免互相覆盖
BUILD  := .build/$(notdir $(SKETCH))

# 自动探测串口：端口在第一列；只认 USB 串口，排除蓝牙调试口
PORT ?= $(shell $(CLI_CFG) board list 2>/dev/null | awk '$$1 ~ /^\/dev\/cu\.(usbserial|wchusbserial|usbmodem|SLAB_USBtoUART|wch)/ {print $$1; exit}')

.DEFAULT_GOAL := help
.PHONY: help setup ports board build upload monitor log flash clean doctor

PY := ./.venv/bin/python

# 串口同一时刻只能被一个程序打开；被占用时给出明确提示而不是晦涩的 esptool 报错
define check_port
	@if [ -n "$(PORT)" ] && lsof -t $(PORT) >/dev/null 2>&1; then \
	  echo "❌ $(PORT) 正被占用，PID: $$(lsof -t $(PORT) | tr '\n' ' ')"; \
	  echo "   先关掉正在运行的 make log / Arduino 串口监视器 / PlatformIO monitor，再重试。"; \
	  exit 1; fi
endef

help: ## 显示可用命令
	@echo "ESP32 开发快捷命令"
	@echo "  make setup    安装/校验 ESP32 核心（首次必做）"
	@echo "  make ports    列出串口和已连接的开发板"
	@echo "  make build    编译 $(SKETCH) -> $(FQBN)"
	@echo "  make upload   编译并烧录到自动探测的串口"
	@echo "  make log      带时间戳的串口日志（自动复位，Ctrl+C 退出）"
	@echo "  make monitor  原生串口监视器（需要真实终端）"
	@echo "  make flash    编译 + 烧录 + 接着打日志（一条龙）"
	@echo "  make doctor   打印工具链/板卡/串口体检信息"
	@echo "  make clean    删除编译产物"
	@echo ""
	@echo "换板子: make upload FQBN=esp32:esp32:esp32s3"
	@echo "指定串口: make upload PORT=/dev/cu.usbserial-0001"

setup: ## 安装 ESP32 Arduino 核心
	$(CLI_CFG) core update-index
	$(CLI_CFG) core install esp32:esp32

ports: ## 列出串口/开发板
	@$(CLI_CFG) board list

board: ## 显示当前生效的配置
	@echo "FQBN   = $(FQBN)"
	@echo "SKETCH = $(SKETCH)"
	@echo "PORT   = $(if $(PORT),$(PORT),<未检测到，请插上 USB 或手动指定 PORT=>)"

build: ## 编译
	@mkdir -p $(BUILD)
	$(CLI_CFG) compile --fqbn $(FQBN) --build-path $(BUILD) --warnings default $(SKETCH)

upload: build ## 编译并烧录
	@if [ -z "$(PORT)" ]; then \
	  echo "❌ 没检测到串口。插好 USB 后运行 'make ports' 查看，或指定 PORT=/dev/cu.xxx"; exit 1; fi
	$(check_port)
	$(CLI_CFG) upload --fqbn $(FQBN) --port $(PORT) --input-dir $(BUILD) $(SKETCH)

log: ## 带时间戳串口日志（自动复位抓启动日志）
	@if [ -z "$(PORT)" ]; then \
	  echo "❌ 没检测到串口，或指定 PORT=/dev/cu.xxx"; exit 1; fi
	$(check_port)
	@mkdir -p logs
	$(PY) tools/monitor.py -p $(PORT) -b $(BAUD) --save logs/serial.log

monitor: ## 原生串口监视器（需要真实终端）
	@if [ -z "$(PORT)" ]; then \
	  echo "❌ 没检测到串口，或指定 PORT=/dev/cu.xxx"; exit 1; fi
	$(CLI_CFG) monitor --port $(PORT) --config baudrate=$(BAUD)

flash: upload ## 编译+烧录，然后提示看日志
	@echo ""
	@echo "✅ 烧录完成。看日志: make log   （或 make log SKETCH=... 前先 make upload）"

doctor: ## 工具链体检
	@echo "== arduino-cli =="; $(CLI) version
	@echo "== 已安装核心 =="; $(CLI_CFG) core list
	@echo "== 串口 =="; $(CLI_CFG) board list
	@echo "== 目录占用 =="; du -sh .arduino15 .tools 2>/dev/null
	@echo "== 目标板 =="; echo "FQBN=$(FQBN)"
	@echo "== 串口占用 =="; \
	  if [ -n "$(PORT)" ] && lsof -t $(PORT) >/dev/null 2>&1; then \
	    echo "$(PORT) 被占用: PID $$(lsof -t $(PORT) | tr '\n' ' ')"; \
	  else echo "$(PORT) 空闲（或未检测到）"; fi

clean: ## 清理编译产物
	rm -rf $(BUILD)
