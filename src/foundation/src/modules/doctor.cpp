// doctor.cpp —— modules 层：功能自检
//
// 检查项（按顺序）：
//   1. Wi-Fi 是否已保存凭据
//   2. Wi-Fi 是否已连接（有 IP）
//   3. MQTT 是否已配置 broker
//   4. MQTT 是否已连接（收到 CONNACK）
//
// 只读取状态，不修改配置、不打印密码。Wi-Fi 连接状态走 net::provider()，
// 不直接 include <WiFi.h>。

#include "doctor.h"

#include <Arduino.h>
#include <stdio.h>

#include "../core/command.h"
#include "../core/logger.h"
#include "mqtt.h"
#include "network.h"
#include "wifi.h"

namespace {

void printCheck(const char* label, bool ok, const char* detail = nullptr) {
  const bool hasDetail = (detail != nullptr && detail[0] != '\0');
  if (hasDetail) {
    logger::line("%-19s %-3s %s", label, ok ? "YES" : "NO", detail);
  } else {
    logger::line("%-19s %s", label, ok ? "YES" : "NO");
  }
}

void doctorCommand(const char* /*args*/) {
  net::Status netStatus;
  net::Provider* provider = net::provider();
  if (provider != nullptr) {
    netStatus = provider->status();
  }

  const bool wifiConfigured = modules::wifi::hasCredentials();
  const bool wifiConnected  = netStatus.connected;
  const bool mqttConfigured = modules::mqtt::configured();
  const bool mqttConnected  = modules::mqtt::connected();

  logger::blank();
  logger::line("[DOCTOR]");

  char detail[96];
  detail[0] = '\0';
  if (wifiConfigured && netStatus.ssid.length() > 0) {
    snprintf(detail, sizeof(detail), "SSID \"%s\"", netStatus.ssid.c_str());
  }
  printCheck("WiFi configured:", wifiConfigured, detail);

  detail[0] = '\0';
  if (wifiConnected) {
    snprintf(detail, sizeof(detail), "IP %s, RSSI %d dBm",
             netStatus.ip.toString().c_str(), static_cast<int>(netStatus.rssi));
  }
  printCheck("WiFi connected:", wifiConnected, detail);

  printCheck("MQTT configured:", mqttConfigured);
  printCheck("MQTT connected:", mqttConnected);

  const int passed = static_cast<int>(wifiConfigured) + static_cast<int>(wifiConnected) +
                     static_cast<int>(mqttConfigured) + static_cast<int>(mqttConnected);
  logger::line("Result:            %d/4 checks passed", passed);
  logger::blank();

  if (passed == 4) {
    logger::line("[DOCTOR] All checks passed.");
  } else {
    if (!wifiConfigured) {
      logger::line("[DOCTOR] Set WiFi: wifi set <ssid> <password>");
    } else if (!wifiConnected) {
      logger::line("[DOCTOR] WiFi not connected: check 'wifi status'.");
    }

    if (!mqttConfigured) {
      logger::line("[DOCTOR] Set MQTT broker: mqtt set host <host>");
    } else if (!mqttConnected) {
      if (!wifiConnected) {
        logger::line("[DOCTOR] MQTT waits for Wi-Fi; check 'wifi status' first.");
      } else {
        logger::line("[DOCTOR] MQTT not connected: check 'mqtt status'.");
      }
    }
  }
  logger::blank();
}

}  // namespace

namespace modules {
namespace doctor {

void begin() {
  command::add("doctor", doctorCommand, "Run WiFi/MQTT self-check");
}

}  // namespace doctor
}  // namespace modules
