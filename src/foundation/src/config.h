// config.h —— 全局编译期配置与固件标识（三层共享）
//
// 只放"常量"，不放业务逻辑。每次正式烧录建议更新 FW_VERSION。

#pragma once

#include <Arduino.h>

#define FW_NAME      "ESP32 Foundation Firmware"
#define FW_VERSION   "1.0"
#define FW_FRAMEWORK "Arduino-ESP32"

namespace cfg {

// ── 串口 ──
constexpr uint32_t SERIAL_BAUD     = 115200;
constexpr size_t   CMD_BUFFER_SIZE = 128;   // 127 字符 + '\0'

// ── Wi-Fi ──
constexpr uint32_t WIFI_RETRY_MS = 10000;

// ── 凭据存储（NVS）──
constexpr const char* NVS_NAMESPACE = "wifi";
constexpr const char* NVS_KEY_SSID  = "ssid";
constexpr const char* NVS_KEY_PASS  = "pass";

}  // namespace cfg
