// foundation.ino —— ESP32 Foundation Firmware v1.0（Arduino 框架）
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
#include "src/core/console.h"
#include "src/core/logger.h"
#include "src/core/system.h"
#include "src/drivers/driver.h"
#include "src/modules/wifi.h"

void setup() {
  Serial.begin(cfg::SERIAL_BAUD);
  delay(500);                    // 等 USB 串口就绪，避免开头几行丢失

  core::console::begin();        // 启动横幅 + 注册 help
  core::sys::begin();            // 注册 system info / system heap / reboot
  modules::wifi::begin();        // 注册 wifi 命令 + 加载凭据 + 发起连接
  drivers::beginAll();           // 初始化已注册的硬件驱动（当前为空）

  logger::line("[INFO] Use 'help' to list commands, 'wifi status' to check network.");
}

void loop() {
  core::console::poll();         // 串口命令（非阻塞）
  modules::wifi::poll();         // 网络维护（非阻塞）
  drivers::pollAll();            // 硬件驱动（非阻塞）

  // 后续扩展在此接线，例如：
  // modules::mqtt::poll();
}
