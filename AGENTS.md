# AGENTS.md —— 固件架构与协作约定

本文件是本工作区的**开发契约**。修改固件前先读它；新增功能后如改变了分层规则，请同步更新本文件。

- 工程：turbo-esp32（Arduino-ESP32，core `esp32:esp32@3.3.12`），版本命名见 4.1
- 入口：`src/foundation/foundation.ino`
- 分层：`core` / `modules` / `drivers`
- 文档：功能范围见 `README.md`，原始需求见用户提供的 V1.0 文档

---

## 1. 三层职责

| 层 | 目录 | 负责 | 稳定性要求 |
|---|---|---|---|
| **core** | `src/foundation/src/core/` | 串口收发、命令行、命令注册表、NVS 配置管理、系统信息、重启 | 尽可能稳定，改动要保守 |
| **modules** | `src/foundation/src/modules/` | Wi-Fi、MQTT 等功能模块 | 按需求扩展，可替换 |
| **drivers** | `src/foundation/src/drivers/` | OLED、传感器、舵机等硬件 | 按硬件扩展，可裁剪 |

### 依赖方向（单向，禁止反向）

```
foundation.ino（只做编排）
       │  可以 include 全部三层
       ▼
   drivers ──► core
   modules ──► core
   core  ──► 谁都不依赖
```

- **core 不得 include modules 或 drivers**：核心层不认识 Wi-Fi，更不认识 OLED。
- **modules 不得 include drivers 的具体头文件**，反之亦然。两者要交互时，走下面的接口。
- 跨层只允许"上层依赖下层"，同层之间通过接口，不直接引用对方的实现文件。

---

## 2. 模块间接口（不要互相依赖内部实现）

### 2.1 网络状态：`modules/network.h`

任何模块想知道"有没有网 / IP / RSSI"，用这个接口，**不要 include `<WiFi.h>`**：

```cpp
#include "src/modules/network.h"

net::Provider* p = net::provider();      // 可能为 nullptr
if (p != nullptr) {
  net::Status st = p->status();
  if (st.connected) { /* st.ip, st.ssid, st.rssi */ }
}
```

- 接口定义在 `network.h`，实现类在 `modules/wifi.cpp` 内部，通过 `net::setProvider(&gProvider)` 注入。
- 未来换成 4G 模块时，只需另一份实现注入同一个接口，MQTT 等上层代码不用改。
- **新增模块（如 MQTT）禁止 `#include <WiFi.h>` 来查状态**，一律走 `net::provider()`。

### 2.2 硬件驱动：`drivers/driver.h`

```cpp
class Driver {
  virtual const char* name() const = 0;
  virtual bool begin() = 0;      // 失败返回 false，不得卡死系统
  virtual void poll() = 0;       // 必须非阻塞
  virtual void printStatus() {}  // 可选
};
```

驱动在自己文件里 `drivers::add(this)`；主循环只调 `drivers::pollAll()`。
新增驱动**不需要修改** `foundation.ino` 以外的地方，更不需要改 core。

### 2.3 命令：`core/command.h`

各层在 `begin()` 里自行注册命令，**不要在 console 里加 if/else**：

```cpp
command::add("wifi scan", scanCommand, "Scan nearby WiFi networks");
```

- `name` 可含空格；分发按"最长命令名"匹配（`wifi setx` 不会误匹配 `wifi set`）。
- `help` 列表由注册表自动生成——注册即出现在帮助里，说明文字是所有者的责任。
- 处理器签名 `void(const char* args)`，`args` 是命令名之后的剩余文本，永不为 `nullptr`。
- 单个命令表上限 24 条，驱动表上限 8 个；超限会在串口报 `[ERROR]`。

### 2.4 配置存储：`core/config_manager.h`（强制）

**固件的应用配置必须存 NVS，并且只能通过 ConfigManager（全局实例 `core::config`）读写。**

```cpp
#include "../core/config_manager.h"

String ssid = core::config.getString("wifi", "ssid", "");
core::config.setString("wifi", "ssid", ssid.c_str());
```

- **禁止**在 `config_manager.cpp` 之外 include `<Preferences.h>`、`<nvs.h>`、`<nvs_flash.h>`，
  禁止直接调用 `Preferences` / `nvs_*` API；`wifi.cpp` 已按此改造。
- 每个模块使用自己的 NVS 命名空间（`wifi`、`mqtt`、`oled`……），命名空间和键名都
  **≤15 字符**（NVS 硬限制），键名小写；不同模块不得共用键名。
- 配置只能存 NVS：不得写入 SPIFFS/文件，也不得硬编码在源码里（`config.h` 的编译期常量除外）。
- 密码、token 等敏感值同样存 NVS，但**不得**通过日志、`help` 或通用命令输出其内容。
- 写失败（NVS 满、分区损坏）必须返回 false 并打印 `[ERROR]`，不得阻塞主循环或重启。
- `core::config.begin()` 在 `setup()` 里调用一次；初始化失败时各模块用默认值降级运行。
- 新增模块不要在类里放 `Preferences` 成员，用 `core::config` 的 get/set/remove。
- **例外**：ESP-IDF 组件（Wi-Fi/PHY 校准等）自己维护的 NVS 记录不属于应用配置，
  不经过 ConfigManager，也不得被应用逻辑直接读取。

---

## 3. 目录与 Arduino 构建约束

```
src/foundation/
├── foundation.ino            # 只做编排：begin/poll 接线
└── src/                      # ← 必须是 sketch 下的 src/，否则不会被编译
    ├── config.h              # 编译期常量（版本号、串口、NVS 键名）
    ├── core/
    │   ├── config_manager.h/.cpp  # NVS 配置管理（唯一入口）
    │   ├── logger.h/.cpp     # 统一输出（自动 CRLF）
    │   ├── command.h/.cpp    # 命令注册表
    │   ├── console.h/.cpp    # 串口输入、回显、退格、超长保护
    │   └── system.h/.cpp     # system info / heap / flash / nvs / reboot
    ├── modules/
    │   ├── network.h/.cpp    # 网络状态接口（Provider）
    │   └── wifi.cpp/.h       # Wi-Fi 模块（连接/扫描/凭据/命令）
    └── drivers/
        ├── driver.h          # 驱动接口
        └── driver.cpp        # 驱动注册表
```

**Arduino 构建的两个硬约束（踩过坑）：**

1. 只有 sketch 目录下的 `src/` 子目录会被递归编译。把源码放到 `core/`、
   `modules/`（与 `src/` 平级）会**不被编译**，最后报 `undefined reference`。
2. `sketch/src` **不在默认 include 路径里**。跨层包含必须写相对路径：
   - `foundation.ino`：`#include "src/core/console.h"`
   - `modules/wifi.cpp`：`#include "../core/logger.h"`、`#include "../config.h"`、`#include "network.h"`
   - `core/console.cpp`：`#include "logger.h"`（同目录）

不要为了好看而加 `-I` 编译参数——那会让 Arduino IDE 里编译失败。

---

## 4. 编码约定

1. **行尾统一 `logger::line()`**：它自动补 `\r\n`。格式串里**不要**自己写 `\r\n`。
   只发 LF 会让串口监视器输出堆成阶梯状（历史 bug）。
2. **不要阻塞主循环**：禁止在 `loop()` 路径上 `delay()` 等网络；驱动 `poll()` 里禁止长延时。
   `wifi scan` 是文档允许的同步扫描（3~5 秒），除此之外一律非阻塞。
3. **射频操作前 `Serial.flush()`**：扫描/连接会切换 APB 时钟，没发完的字节会乱码。
4. **重连前先 `WiFi.disconnect()`**：否则 ESP-IDF 报 `sta is connecting, cannot set config`。
5. **正在连接时不能扫描**：`scanNetworks()` 会返回 `-2`。`wifi scan` 已实现"先取消、扫完恢复"。
6. **不打印凭据**：密码只输出"是否已设置"，不得输出内容（文档 11.4）。
7. **命令缓冲区有上限**（`cfg::CMD_BUFFER_SIZE`），新增输入路径必须复用同一套保护。
8. **不要用 `log` 作命名空间**：与 `math.h` 的 `log()` 冲突，固件里已改为 `logger`。
9. **命名空间**：`core::`、`modules::`、`drivers::`、`net::`、`command::`、`logger::`。
10. **版本号**：固件统一名称为 `turbo-esp32.v<MAJOR>.<MINOR>.<PATCH>`，详细规则见 4.1；
    正式烧录前必须更新 `config.h` 里的 `FW_VERSION`（文档 11.8）。

### 4.1 固件版本命名（SemVer，强制）

- **统一名称**：`turbo-esp32.v<MAJOR>.<MINOR>.<PATCH>`，例如 `turbo-esp32.v1.0.0`。
  `v` 是小写连接符，只出现在完整名称里；`FW_VERSION` 本身只存 `MAJOR.MINOR.PATCH`，
  不得写成 `v1.0.0`，否则完整名称会变成 `turbo-esp32.vv1.0.0`。
- **版本标准**：版本部分遵循语义化版本 SemVer 2.0.0——三段非负整数，不得省略
  （`1.0`、`v1` 都不合规）；不得有前导零（`01.2.3` 不合规）。正式版本不带后缀。
- **唯一来源**：版本只在 `src/foundation/src/config.h` 定义一次：

  ```cpp
  #define FW_NAME      "turbo-esp32"
  #define FW_VERSION   "1.0.0"                  // 只写数字，不带 v
  #define FW_ID        FW_NAME ".v" FW_VERSION   // turbo-esp32.v1.0.0
  ```

  串口横幅、`system info`、README 一律引用 `FW_ID`，**不要手写版本字符串**。
- **递增规则**（递增某一段时，低位段归零）：

  | 段 | 何时递增 | 示例 |
  |---|---|---|
  | MAJOR | 不兼容变更：命令改名/删除、串口协议或 NVS 布局不兼容 | `0.9.0` → `1.0.0` |
  | MINOR | 向后兼容的新功能：新增模块、命令、驱动 | `1.0.0` → `1.1.0` |
  | PATCH | 向后兼容的修复：bug、日志、文档、构建脚本 | `1.1.0` → `1.1.1` |

- **预发布 / 构建元数据**（可选，仅用于测试）：`turbo-esp32.v1.2.0-rc.1`、
  `turbo-esp32.v1.2.0+build.5`。正式烧录必须是纯 `X.Y.Z`。
- **发布**：Git tag 与完整名称一致，如 `git tag turbo-esp32.v1.2.0`；烧录前核对
  串口横幅与 `system info` 显示的版本都等于本次发布版本。

---

## 5. 新增模块的步骤（以 MQTT 为例）

1. 建文件 `src/foundation/src/modules/mqtt.h/.cpp`，命名空间 `modules::mqtt`。
2. 只暴露生命周期：`void begin(); void poll();`，实现细节留在 `.cpp` 的内部匿名 namespace。
3. 在 `begin()` 里注册命令：`command::add("mqtt status", statusHandler, "...")`。
4. 需要网络状态时用 `net::provider()`，**禁止** `#include <WiFi.h>`。
5. 在 `foundation.ino` 里加两行：`modules::mqtt::begin();` 和 `modules::mqtt::poll();`。
6. 配置（broker 地址、端口、凭据）用 `core::config` 存 NVS，命名空间用 `mqtt`；
   禁止直接 include `<Preferences.h>`，规则见 2.4。
7. 编译、烧录，跑一遍 `README.md` 的验收表 + 新命令。

## 6. 新增驱动的步骤（以 OLED 为例）

1. 在 `src/foundation/src/drivers/` 下**平铺**建文件，用文件名区分硬件，
   例如 `oled_ssd1306.h/.cpp`，并实现 `drivers::Driver`。
   不要建 `drivers/oled/` 这类子目录——Arduino 只递归编译 `sketch/src/` 这一层，
   `src/drivers/oled/` 里的 `.cpp` 不会被编译，最终报 `undefined reference`。
2. `begin()` 返回 false 表示硬件不存在或 I2C 无应答——**不得让系统卡死或重启风暴**。
3. `poll()` 里做非阻塞刷新（例如每 500ms 由 `millis()` 控制，不用 `delay`）。
4. 驱动自己注册命令（如 `oled test`），或在 `printStatus()` 里输出诊断信息。
5. 在 `foundation.ino` 里 `drivers::add(&gOled);`（放在 `drivers::beginAll()` 之前）。
6. 接线引脚写进该驱动的头文件注释，并核实不是启动绑带脚 / Flash 专用脚。

---

## 7. 构建、烧录、验证

```bash
make build                 # 编译（默认 src/foundation）
make upload                # 烧录（自动探测 /dev/cu.usbserial-*）
make log                   # 带时间戳的串口日志，Ctrl+C 退出
make doctor                # 工具链/串口/占用体检
```

- 工具链全部在项目内（`.tools/`、`.arduino15/`），不污染系统。
- **串口独占**：`make log` 开着时不能烧录，反之亦然。被占用时 `make upload` 会打印占用 PID。
- 打开串口会让开发板复位一次（DTR/RTS 接 EN/GPIO0），这是硬件行为；凭据在 NVS，复位后会自动重连。

### 提交前的最小验证

| 项 | 期望 |
|---|---|
| `make build` | 无 error、无 warning |
| 启动 | 打印版本横幅（`turbo-esp32.vX.Y.Z`）；有凭据则显示 `Connection started.` |
| `help` | 列出全部已注册命令（新增模块的命令自动出现） |
| `system info` / `system heap` | 数值合理，不重启 |
| `system flash` / `system nvs` | 分区、Sketch、NVS 条目数值合理，不重启 |
| `wifi scan` | 能列出网络；凭据失效时也能扫（不应卡在 -2） |
| `wifi status` | 未连接显示状态码，已连接显示 IP/RSSI/MAC |
| 超长命令（>127 字符） | `[ERROR] Command too long. Input discarded.` |
| `reboot` | 重启并重新打印横幅 |

---

## 8. 不要做的事

- ❌ 把业务逻辑写进 `foundation.ino`（它只做编排）。
- ❌ 在 `console.cpp` 里加功能命令判断（用 `command::add`）。
- ❌ 让 core 依赖具体模块；让 modules 依赖具体驱动。
- ❌ 绕过 ConfigManager 直接操作 Preferences/NVS（见 2.4）。
- ❌ 把配置写进 SPIFFS/文件，或硬编码在源码里（编译期常量除外）。
- ❌ 用 `delay()` 等待网络或硬件就绪。
- ❌ 打印 Wi-Fi / MQTT 密码。
- ❌ 未经核实就驱动未知 GPIO（启动绑带脚 0/2/5/12/15、Flash 脚 6-11、只能输入的 34-39）。
- ❌ 为了目录好看而调整 `src/` 结构——会直接导致代码不被编译。
