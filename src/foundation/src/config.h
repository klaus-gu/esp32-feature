// config.h —— 全局编译期配置与固件标识（三层共享）
//
// 只放"常量"，不放业务逻辑。每次正式烧录建议更新 FW_VERSION。

#pragma once

#include <Arduino.h>

// 固件标识：统一名称 turbo-esp32，版本遵循 SemVer 2.0.0（见 AGENTS.md 4.1）。
// 完整标识形如 turbo-esp32.v1.1.0；FW_VERSION 只存数字，不要再带 'v'。
#define FW_NAME      "turbo-esp32"
#define FW_VERSION   "1.1.0"
#define FW_ID        FW_NAME ".v" FW_VERSION
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
