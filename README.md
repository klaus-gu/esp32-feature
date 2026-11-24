# ESP32 Foundation Firmware V1.0（Arduino 框架）

按《ESP32 开发基础固件 V1.0》实现的可复用基础固件：烧录后通过串口命令行查询系统信息、
查看 Wi-Fi 状态、扫描网络、重启设备。后续可在此基础上叠加 MQTT / OLED / 传感器 / 舵机。

- 固件版本：`1.0`
- 框架：Arduino-ESP32（core `esp32:esp32@3.3.12`）
- 实测硬件：ESP32-D0WD-V3（双核 240 MHz / 4 MB Flash / MAC `50:1F:86:A9:9B:20`）
- 串口：115200 8-N-1

## 目录结构

```
esp32-dev/
├── Makefile
├── AGENTS.md                # ★ 架构与协作约定（改固件前必读）
├── arduino-cli.yaml
├── tools/monitor.py         # 带时间戳 + 自动复位的串口日志工具
├── archive/                 # 早期实验代码归档（不参与编译）
└── src/foundation/
    ├── foundation.ino       # 只做编排：把三层接进 setup/loop
    └── src/
        ├── config.h         # 版本号、串口参数、NVS 键名
        ├── core/            # 串口、命令行、系统管理（最稳定）
        │   ├── logger.h/.cpp   统一输出，自动补 CRLF
        │   ├── command.h/.cpp  命令注册表（新命令自动进 help）
        │   ├── console.h/.cpp  输入、回显、退格、超长保护
        │   └── system.h/.cpp   system info / system heap / reboot
        ├── modules/         # 功能模块
        │   ├── network.h/.cpp  网络状态接口 net::Provider
        │   └── wifi.h/.cpp     Wi-Fi 连接、扫描、凭据(NVS)、命令
        └── drivers/         # 硬件驱动
            ├── driver.h         drivers::Driver 接口
            └── driver.cpp       驱动注册表（当前无具体驱动）
```

三层依赖是**单向**的：`core` 不依赖任何上层；`modules` / `drivers` 只依赖 `core`，
两者之间通过接口（`net::Provider`、`drivers::Driver`）交互，而不是互相 include 内部实现。
新增命令用 `command::add()` 注册，不需要改 console；新增驱动实现 `Driver` 并 `drivers::add()`，
不需要改主循环。详细规则与"新增模块/驱动"的步骤见 [AGENTS.md](AGENTS.md)。

## 快速开始

```bash
cd ~/Desktop/esp32-dev

make build            # 编译
make upload           # 编译并烧录（自动探测串口）
make log              # 带时间戳看串口输出（Ctrl+C 退出）
```

用 Arduino IDE 的用户：打开 `src/foundation/foundation.ino`，选板子 `ESP32 Dev Module`、
选对串口、115200 打开监视器即可，行结束符选 `Newline` 或 `Both NL & CR`。

## 命令

文档 V1.0 规定：

| 命令 | 作用 |
|---|---|
| `help` | 列出命令 |
| `system info` | 芯片型号、CPU 频率、Flash 容量、SDK 版本 |
| `system heap` | 剩余堆内存 / 历史最小剩余堆 |
| `wifi status` | 连接状态、IP、RSSI、MAC |
| `wifi scan` | 扫描附近网络（SSID / RSSI / 信道 / 加密类型） |
| `reboot` | 重启设备 |

额外提供的两条（文档 6.1 建议「凭据存放在适当的配置存储中」，否则换 WiFi 就要重新烧录）：

| 命令 | 作用 |
|---|---|
| `wifi set <ssid> [密码]` | 写入 NVS 并立即连接（SSID 含空格用双引号） |
| `wifi forget` | 删除已保存凭据 |

凭据存在 NVS（命名空间 `wifi`），**断电、重启、重新烧录都不会丢**；日志只显示密码是否已设置，不打印内容。

## 启动输出

```
================================
 ESP32 Foundation Firmware v1.0
 Framework: Arduino-ESP32
 Type 'help' for commands.
================================
[WiFi] Connection started.
[WiFi] Using saved SSID "MyWiFi" (password set)
[WiFi] Connecting to "MyWiFi" ...
[INFO] Use 'wifi status' to check connection.
```

未配置凭据时提示：`[WiFi] No credentials stored. Use 'wifi set <ssid> <password>'.`

## 验收测试（文档第 8 节）

| 测试项 | 操作 | 结果 |
|---|---|---|
| 启动日志 | 复位开发板 | ✅ 输出固件版本与框架信息 |
| 帮助命令 | `help` | ✅ 列出 6 条命令 + 2 条扩展 |
| 芯片信息 | `system info` | ✅ `ESP32-D0WD-V3 rev 301 / 240 MHz / 4 MB / SDK v5.5.5` |
| 内存查询 | `system heap` | ✅ 例：`Free Heap: 235532 bytes` |
| Wi-Fi 状态 | `wifi status` | ✅ 已连接显示 IP/RSSI/MAC；未连接显示状态码 |
| Wi-Fi 扫描 | `wifi scan` | ✅ 实测扫到 28 个网络 |
| 网络断开 | 关闭路由器 | ⬜ 需真实热点，未测 |
| 网络恢复 | 恢复路由器 | ⬜ 需真实热点，未测 |
| 重启 | `reboot` | ✅ 重启并重新打印启动横幅 |
| 超长命令 | 输入 200 字符 | ✅ `[ERROR] Command too long. Input discarded.`，无溢出 |

## 实现中踩到的坑（都已在代码里处理）

1. **`\n` 不够，必须 `\r\n`**：只发 LF 时串口监视器光标不回行首，输出会堆成阶梯状。
2. **扫描前必须 `Serial.flush()`**：射频启动瞬间 APB 时钟跳变，还没发完的字节会被打乱。
3. **重连会占住射频**：STA 正在连接时 `scanNetworks()` 直接返回 `-2`。现在 `wifi scan` 会先取消
   本次连接尝试，扫完再恢复，保证凭据失效时扫描依然可用。
4. **重连前先 `disconnect()`**：否则 ESP-IDF 报 `sta is connecting, cannot set config`，新参数不生效。
5. **串口噪声字节**：复位/切射频时线上会出现 `0x00`/`0xFF`，会把命令污染成「未知命令」，已丢弃。
6. **不要把功能堆进 `.ino`**：按文档 11.1 拆成 core / modules / drivers 三层。
7. **`log` 不能作命名空间**：与 `math.h` 的 `log()` 撞名，已改名 `logger`。
8. **Arduino 只编译 `sketch/src/`**：源码放在其它子目录会不被编译；跨层包含必须用相对路径。

## 已知限制

- `wifi scan` 是**同步扫描**，会阻塞 3~5 秒（文档 6 节已说明，后续可改异步）。
- 经典 ESP32 只支持 **2.4 GHz**，连 5 GHz 热点必然失败。
- 串口同一时刻只能被一个程序打开；`make log` 开着时不能烧录。
- GPIO / 舵机 / 外设命令未实现：需要先确认板型、引脚用途和供电，不能随意驱动。

## 后续扩展路线（文档第 10 节）

| 模块 | 目录约定 | 建议命令 |
|---|---|---|
| MQTT | `mqtt/mqtt_manager.h/.cpp` | `mqtt status` / `mqtt publish` |
| OLED | `drivers/oled/` | `oled test` / `oled show wifi` |
| 传感器 | `drivers/sensor/` | `sensor read` / `sensor list` |
| 舵机 | `drivers/servo/` | `servo test` / `servo move <id> <angle>` |

新增模块只需在 `foundation.ino` 的 `loop()` 里调用其 `poll()`，命令解析仍放在 `console.cpp`。
舵机必须独立供电，机械臂还需要本地急停与看门狗，网络指令不承担实时安全控制。
