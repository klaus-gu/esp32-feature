# turbo-esp32（ESP32 基础固件，Arduino 框架）

按《ESP32 开发基础固件 V1.0》实现的可复用基础固件：烧录后通过串口命令行查询系统信息、
查看 Wi-Fi 状态、扫描网络、重启设备，并提供基于 espMqttClient 的 MQTT 基础连接。
后续可在此基础上叠加 OLED / 传感器 / 舵机。

- 固件版本：`turbo-esp32.v1.2.0`
- 框架：Arduino-ESP32（core `esp32:esp32@3.3.12`）
- 实测硬件：ESP32-D0WD-V3（双核 240 MHz / 4 MB Flash / MAC `50:1F:86:A9:9B:20`）
- 串口：115200 8-N-1

## 目录结构

```
esp32-feature/
├── Makefile
├── AGENTS.md                # ★ 架构与协作约定（改固件前必读）
├── arduino-cli.yaml
├── tools/monitor.py         # 带时间戳 + 自动复位的串口日志工具
└── src/foundation/
    ├── foundation.ino       # 只做编排：把三层接进 setup/loop
    └── src/
        ├── config.h         # 版本号、串口参数、NVS 键名
        ├── core/            # 串口、命令行、配置、系统管理（最稳定）
        │   ├── config_manager.h/.cpp  NVS 配置管理（唯一入口）
        │   ├── logger.h/.cpp   统一输出，自动补 CRLF
        │   ├── command.h/.cpp  命令注册表（新命令自动进 help）
        │   ├── console.h/.cpp  输入、回显、退格、超长保护
        │   └── system.h/.cpp   system info / heap / flash / nvs / reboot
        ├── modules/         # 功能模块
        │   ├── network.h/.cpp  网络状态接口 net::Provider
        │   ├── wifi.h/.cpp     Wi-Fi 连接、扫描、凭据(经 ConfigManager)、命令
        │   └── mqtt.h/.cpp     MQTT 连接（espMqttClient + NVS 配置）
        └── drivers/         # 硬件驱动
            ├── driver.h         drivers::Driver 接口
            └── driver.cpp       驱动注册表（当前无具体驱动）
```

三层依赖是**单向**的：`core` 不依赖任何上层；`modules` / `drivers` 只依赖 `core`，
两者之间通过接口（`net::Provider`、`drivers::Driver`）交互，而不是互相 include 内部实现。
新增命令用 `command::add()` 注册，不需要改 console；新增驱动实现 `Driver` 并 `drivers::add()`，
不需要改主循环。详细规则与"新增模块/驱动"的步骤见 [AGENTS.md](AGENTS.md)。

## 配置存储

所有应用配置只存 **NVS**，并统一由 `core/config_manager.h` 的 `core::config`（ConfigManager）读写；
其它代码禁止直接 include `<Preferences.h>` / `<nvs.h>`。命名空间按模块划分
（`wifi`、`mqtt`、`oled`……），命名空间和键名都不得超过 15 字符（NVS 硬限制）。

## 快速开始

```bash
cd ~/Desktop/esp32-feature

make setup            # 首次：安装 ESP32 核心 + MQTT 依赖库
make build            # 编译
make upload           # 编译并烧录（自动探测串口）
make log              # 带时间戳看串口输出（Ctrl+C 退出）
```

`make setup`（或单独的 `make libs`）会把 **espMqttClient 1.7.3** 和它声明的 **AsyncTCP 3.4.9**
下载到项目内的 `.arduino15/user/libraries/`，不影响系统。espMqttClient 没有收录在 Arduino
Library Manager，用 Arduino IDE 的用户需要手动下载这两个库的 ZIP
（[espMqttClient v1.7.3](https://github.com/bertmelis/espMqttClient/archive/refs/tags/v1.7.3.zip)、
[AsyncTCP v3.4.9](https://github.com/ESP32Async/AsyncTCP/archive/refs/tags/v3.4.9.zip)）
并「Sketch → Include Library → Add .ZIP Library」；随后打开 `src/foundation/foundation.ino`，
选板子 `ESP32 Dev Module`、串口、115200，行结束符选 `Newline` 或 `Both NL & CR`。

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

扩展命令（凭据存储与诊断）：

| 命令 | 作用 |
|---|---|
| `wifi set <ssid> [密码]` | 通过 ConfigManager 写入 NVS 并立即连接（SSID 含空格用双引号） |
| `wifi forget` | 删除已保存凭据 |
| `system flash` | Flash 总量、运行分区/Sketch 占用、OTA 槽位、完整分区表 |
| `system nvs` | NVS 分区、条目使用率、固件使用的命名空间 |

凭据存在 NVS（命名空间 `wifi`），**断电、重启、重新烧录都不会丢**，统一由 `core::config` 管理；
日志只显示密码是否已设置，不打印内容。

## MQTT

MQTT 客户端使用 [espMqttClient](https://github.com/bertmelis/espMqttClient) 1.7.3（MQTT 3.1.1，
当前只开 **plain TCP**，默认端口 1883；暂不支持 TLS）。客户端内部使用 FreeRTOS 任务，
主循环里的 `poll()` 只做网络检查和重连调度，不会被 TCP 连接阻塞。

> espMqttClient 的 async 源码也会参与 Arduino 库编译，所以还必须安装它声明的
> AsyncTCP 3.4.9（本固件只用同步客户端，没有用到 async 实现）。

配置存 NVS 命名空间 `mqtt`（经 ConfigManager）：

| 命令 | 作用 |
|---|---|
| `mqtt status` | broker、client id、keepalive、网络、连接状态（不打印密码） |
| `mqtt set host <host>` | broker 主机名或 IP |
| `mqtt set port <1-65535>` | 端口，默认 1883 |
| `mqtt set client <id>` | client id；不设置则自动生成 `turbo-<MAC尾6位>` |
| `mqtt set user <username>` | 用户名；`mqtt set user`（不带值）清除 |
| `mqtt set pass <password>` | 密码；`mqtt set pass`（不带值）清除 |
| `mqtt set keepalive <seconds>` | keepalive，默认 60 秒 |
| `mqtt connect` / `mqtt disconnect` | 开/关自动连接（设置持久化到 NVS） |
| `mqtt clear` | 清空整个 `mqtt` 命名空间 |

示例（把地址换成你自己的 broker）：

```
mqtt set host 192.168.1.10
mqtt set port 1883
mqtt set client turbo-test
mqtt connect
mqtt status
```

配好 host 且 `enabled=yes` 时，Wi-Fi 一拿到 IP 就会连接；断线后每 5 秒重试。
当前只实现连接层，还没有 subscribe / publish 命令。

## 启动输出

```
================================
 turbo-esp32.v1.2.0
 Framework: Arduino-ESP32
 Type 'help' for commands.
================================
[WiFi] Connection started.
[WiFi] Using saved SSID "MyWiFi" (password set)
[WiFi] Connecting to "MyWiFi" ...
[MQTT] Not configured. Use 'mqtt set host <host>'.
[INFO] Use 'wifi status' to check connection.
```

未配置凭据时提示：`[WiFi] No credentials stored. Use 'wifi set <ssid> <password>'.`
MQTT 已配置时启动栏会显示：`[MQTT] Broker <host>:<port>, client "...", auto-connect on.`

## 验收测试（文档第 8 节）

| 测试项 | 操作 | 结果 |
|---|---|---|
| 启动日志 | 复位开发板 | ✅ 输出固件版本与框架信息 |
| 帮助命令 | `help` | ✅ 列出全部 15 条命令 |
| 芯片信息 | `system info` | ✅ `ESP32-D0WD-V3 rev 301 / 240 MHz / 4 MB / SDK v5.5.5` |
| 内存查询 | `system heap` | ✅ 例：`Free Heap: 235532 bytes` |
| Flash 查询 | `system flash` | ✅ `4096 KB / app0 1280 KB / Sketch 70% / 6 个分区` |
| NVS 查询 | `system nvs` | ✅ 实测 `130 used / 630 total (20%)`，5 个命名空间 |
| MQTT 未配置 | `mqtt status` | ✅ 提示 `Not configured`，自动生成 client id |
| MQTT 配置 | `mqtt set host/port/client/user/pass/keepalive` | ✅ 全部写入 NVS；`mqtt status` 不显示密码 |
| MQTT 配置持久化 | `mqtt set ...` → `reboot` → `mqtt status` | ✅ 重启后 broker/client/keepalive 保持不变 |
| MQTT 清除配置 | `mqtt clear` → `mqtt status` | ✅ 恢复 `NOT CONFIGURED` |
| MQTT 真实连接 | 配置 broker + 已连 Wi-Fi | ⬜ 板上无 Wi-Fi 凭据、无可用 broker，未测 |
| 配置持久化 | `wifi set __cm_test__` → `reboot` → `wifi status` | ✅ 重启后仍显示 `Configured SSID: __cm_test__` |
| 清除配置 | `wifi forget` → `wifi status` | ✅ 恢复为 `Configured SSID: (none)` |
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
9. **`Preferences::putString` 对空字符串返回 0**：不能用返回值判断写入成败；ConfigManager 改为
   写入后回读校验，否则 `wifi set <ssid>`（无密码）会被误判为失败。
10. **espMqttClient 保存的是指针**：`setServer()` / `setCredentials()` / `setClientId()` 不会复制字符串，
    配置必须放在生命周期长的固定缓冲区，不能用临时 `String` 或局部变量。
11. **espMqttClient 的 async 源码也会被 Arduino 编译**：库构建会编译所有 `.cpp`，因此即使只用同步
    客户端，也必须安装它声明的 AsyncTCP，否则报 `AsyncTCP.h: No such file or directory`。

## 已知限制

- `wifi scan` 是**同步扫描**，会阻塞 3~5 秒（文档 6 节已说明，后续可改异步）。
- 经典 ESP32 只支持 **2.4 GHz**，连 5 GHz 热点必然失败。
- 串口同一时刻只能被一个程序打开；`make log` 开着时不能烧录。
- NVS 分区只有 **20 KB**：配置项/命名空间要节制，写满时 ConfigManager 会返回失败并打 `[ERROR]`。
- MQTT 当前只支持 plain TCP，不支持 TLS/8883；也还没有 subscribe / publish 命令。
- MQTT 密码以明文形式保存在 NVS（未启用 NVS 加密），不要把生产环境密钥放进来。
- GPIO / 舵机 / 外设命令未实现：需要先确认板型、引脚用途和供电，不能随意驱动。

## 后续扩展路线（文档第 10 节）

| 模块 | 目录约定 | 建议命令 |
|---|---|---|
| OLED | `drivers/oled_ssd1306.h/.cpp` | `oled test` / `oled show wifi` |
| 传感器 | `drivers/sensor_xxx.h/.cpp` | `sensor read` / `sensor list` |
| 舵机 | `drivers/servo.h/.cpp` | `servo test` / `servo move <id> <angle>` |

新增模块只需在 `foundation.ino` 里调 `begin()` / `poll()`；命令用 `command::add()` 注册；
配置用 `core::config` 存 NVS（规则见 AGENTS.md 2.4）。
舵机必须独立供电，机械臂还需要本地急停与看门狗，网络指令不承担实时安全控制。
