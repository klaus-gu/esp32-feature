// foundation.ino —— turbo-esp32.v1.3.0（Arduino 框架）
//
// 三层结构（详见 AGENTS.md）：
//   core    串口、命令行、系统管理      —— 尽量稳定，不依赖上层
//   modules Wi-Fi、MQTT 等功能模块      —— 按需求扩展，通过接口对外提供能力
//   drivers OLED、传感器、舵机等硬件    —— 按硬件扩展，实现 drivers::Driver
//
// 本文件只做"编排"：初始化各层、把它们的 poll() 串进主循环。
// 任何业务逻辑都不应写在这里。

#include <Arduino.h>

#include "src/config.h"
#include "src/core/config_manager.h"
#include "src/core/console.h"
#include "src/core/logger.h"
#include "src/core/system.h"
#include "src/drivers/driver.h"
#include "src/modules/boardinfo.h"
#include "src/modules/doctor.h"
#include "src/modules/mqtt.h"
#include "src/modules/wifi.h"

void setup() {
  Serial.begin(cfg::SERIAL_BAUD);
  delay(500);                    // 等 USB 串口就绪，避免开头几行丢失

  core::console::begin();        // 启动横幅 + 注册 help
  core::config.begin();          // 初始化 NVS（所有配置的唯一入口）
  core::sys::begin();            // 注册 system info / system heap / reboot
  modules::wifi::begin();        // 注册 wifi 命令 + 加载凭据（暂不连接）
  modules::mqtt::begin();        // 加载 MQTT 配置 + 注册 mqtt 命令
  modules::doctor::begin();      // 注册 doctor 自检命令
  drivers::beginAll();           // 初始化已注册的硬件驱动（当前为空）

  logger::line("[INFO] Use 'help' to list commands, 'wifi status' to check network.");
  modules::boardinfo::begin();   // 开机板卡摘要（只展示配置快照）

  modules::wifi::start();        // 摘要打印后才开始连接 Wi-Fi
  core::console::prompt();       // Wi-Fi 连接日志之后再出提示符
}

void loop() {
  core::console::poll();         // 串口命令（非阻塞）
  modules::wifi::poll();         // 网络维护（非阻塞）
  modules::mqtt::poll();         // MQTT 连接维护（非阻塞）
  drivers::pollAll();            // 硬件驱动（非阻塞）

  // 后续扩展在此接线，例如：
  // modules::sensor::poll();
}
