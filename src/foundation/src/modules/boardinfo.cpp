// boardinfo.cpp —— modules 层：开机板卡摘要
//
// 串口监视器打开时开发板会复位，因此这份摘要是“接通串口后第一屏”的
// 主要信息。网络状态在打印时读取；Wi-Fi 后续连上时由 wifi 模块继续打印
// SSID/IP/RSSI。

#include "boardinfo.h"

#include <Arduino.h>
#include <esp_mac.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

#include "../config.h"
#include "../core/config_manager.h"
#include "../core/logger.h"
#include "mqtt.h"
#include "wifi.h"

namespace {

void printMacAddress() {
  uint8_t mac[6] = {0};
  if (esp_read_mac(mac, ESP_MAC_WIFI_STA) != ESP_OK) {
    const uint64_t efuse = ESP.getEfuseMac();
    for (int i = 0; i < 6; i++) {
      mac[i] = static_cast<uint8_t>((efuse >> (8 * (5 - i))) & 0xFF);
    }
  }
  logger::line("MAC (WiFi):    %02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2],
               mac[3], mac[4], mac[5]);
}

void printConfig() {
  const char* ssid = modules::wifi::configuredSsid();
  if (ssid != nullptr && ssid[0] != '\0') {
    logger::line("WiFi:          configured \"%s\"", ssid);
  } else {
    logger::line("WiFi:          not configured");
  }

  if (modules::mqtt::configured()) {
    logger::line("MQTT:          configured \"%s:%u\"", modules::mqtt::configuredHost(),
                 static_cast<unsigned>(modules::mqtt::configuredPort()));
  } else {
    logger::line("MQTT:          not configured");
  }
}

void printSummary() {
  logger::blank();
  logger::line("[BOARD]");

  logger::line("Firmware:      %s", FW_ID);
  logger::line("Chip:          %s rev %d", ESP.getChipModel(), ESP.getChipRevision());
  logger::line("CPU:           %u MHz", ESP.getCpuFreqMHz());
  logger::line("SDK:           %s", ESP.getSdkVersion());

  const uint32_t flashSize = ESP.getFlashChipSize();
  logger::line("Flash:         %u B (%u KB)", flashSize, flashSize / 1024);

  const uint32_t sketchSize = ESP.getSketchSize();
  const esp_partition_t* running = esp_ota_get_running_partition();
  if (running != nullptr) {
    const uint32_t freeBytes = running->size > sketchSize ? running->size - sketchSize : 0;
    const unsigned pct = running->size == 0
                             ? 0
                             : static_cast<unsigned>((100ULL * sketchSize) / running->size);
    logger::line("App:           %s %u KB, sketch %u B (%u%%), free %u KB", running->label,
                 static_cast<unsigned>(running->size / 1024), sketchSize, pct,
                 static_cast<unsigned>(freeBytes / 1024));
  } else {
    logger::line("App:           sketch %u B", sketchSize);
  }

  logger::line("Heap:          free %u / %u B, min %u", ESP.getFreeHeap(), ESP.getHeapSize(),
               ESP.getMinFreeHeap());

  const core::ConfigUsage nvs = core::config.usage();
  if (nvs.valid) {
    const unsigned pct = nvs.totalEntries == 0
                             ? 0
                             : static_cast<unsigned>((100ULL * nvs.usedEntries) / nvs.totalEntries);
    logger::line("NVS:           %u/%u entries (%u%%), %u KB", static_cast<unsigned>(nvs.usedEntries),
                 static_cast<unsigned>(nvs.totalEntries), pct,
                 static_cast<unsigned>(nvs.size / 1024));
  } else {
    logger::line("NVS:           unavailable");
  }

  printMacAddress();
  printConfig();

  logger::blank();
}

}  // namespace

namespace modules {
namespace boardinfo {

void begin() {
  // 只打印配置快照：Wi-Fi 的已保存 SSID、MQTT 的 broker 配置，
  // 不读取连接状态/IP。打印完成后由 foundation 再启动 Wi-Fi 连接。
  printSummary();
}

}  // namespace boardinfo
}  // namespace modules
