// system.cpp —— core 层：系统管理实现
//
// 全部基于 Arduino-ESP32 / ESP-IDF 已有接口，不重复实现底层能力。
// system flash / system nvs 只做只读诊断，不修改 flash 内容。

#include "system.h"

#include <Arduino.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <stdio.h>

#include "../config.h"
#include "command.h"
#include "config_manager.h"
#include "logger.h"

namespace {

const char* partitionTypeName(esp_partition_type_t type) {
  switch (type) {
    case ESP_PARTITION_TYPE_APP:  return "app";
    case ESP_PARTITION_TYPE_DATA: return "data";
    default:                      return "?";
  }
}

const char* partitionSubtypeName(esp_partition_type_t type, esp_partition_subtype_t subtype) {
  if (type == ESP_PARTITION_TYPE_APP) {
    switch (subtype) {
      case ESP_PARTITION_SUBTYPE_APP_FACTORY: return "factory";
      case ESP_PARTITION_SUBTYPE_APP_OTA_0:   return "ota_0";
      case ESP_PARTITION_SUBTYPE_APP_OTA_1:   return "ota_1";
      case ESP_PARTITION_SUBTYPE_APP_TEST:    return "test";
      default:                                return "app";
    }
  }
  if (type == ESP_PARTITION_TYPE_DATA) {
    switch (subtype) {
      case ESP_PARTITION_SUBTYPE_DATA_OTA:      return "ota";
      case ESP_PARTITION_SUBTYPE_DATA_PHY:      return "phy";
      case ESP_PARTITION_SUBTYPE_DATA_NVS:      return "nvs";
      case ESP_PARTITION_SUBTYPE_DATA_COREDUMP: return "coredump";
      case ESP_PARTITION_SUBTYPE_DATA_NVS_KEYS: return "nvs_keys";
      case ESP_PARTITION_SUBTYPE_DATA_SPIFFS:   return "spiffs";
      case ESP_PARTITION_SUBTYPE_DATA_FAT:      return "fat";
      case ESP_PARTITION_SUBTYPE_DATA_LITTLEFS: return "littlefs";
      default:                                  return "data";
    }
  }
  return "?";
}

void printInfo() {
  logger::blank();
  logger::line("[SYSTEM INFO]");
  logger::line("Chip Model:     %s rev %d", ESP.getChipModel(), ESP.getChipRevision());
  logger::line("CPU Frequency:  %u MHz", ESP.getCpuFreqMHz());
  logger::line("Flash Size:     %u bytes (%u KB)",
            ESP.getFlashChipSize(), ESP.getFlashChipSize() / 1024);
  logger::line("SDK Version:    %s", ESP.getSdkVersion());
  logger::line("Firmware:       %s", FW_ID);
  logger::blank();
}

void printHeap() {
  logger::blank();
  logger::line("[MEMORY]");
  logger::line("Free Heap:         %u bytes", ESP.getFreeHeap());
  logger::line("Minimum Free Heap: %u bytes", ESP.getMinFreeHeap());
  logger::blank();
}

// ── Flash 使用情况 ──
void printFlash() {
  logger::blank();
  logger::line("[FLASH]");

  const uint32_t chip = ESP.getFlashChipSize();
  logger::line("Chip Size:       %u bytes (%u KB)", chip, chip / 1024);

  const uint32_t sketch = ESP.getSketchSize();
  const esp_partition_t* running = esp_ota_get_running_partition();
  if (running != nullptr) {
    const unsigned pct =
        running->size == 0 ? 0 : static_cast<unsigned>((100ULL * sketch) / running->size);
    logger::line("Running App:     %s @ 0x%08X (%u KB)", running->label, running->address,
              running->size / 1024);
    logger::line("Sketch Size:     %u bytes (%u%% of app partition)", sketch, pct);
    const uint32_t freeBytes = running->size > sketch ? running->size - sketch : 0;
    logger::line("Free in App:     %u bytes (%u KB)", freeBytes, freeBytes / 1024);
  } else {
    logger::line("Sketch Size:     %u bytes", sketch);
  }

  const esp_partition_t* next = esp_ota_get_next_update_partition(nullptr);
  if (next != nullptr) {
    logger::line("Next OTA Slot:   %s @ 0x%08X (%u KB)", next->label, next->address,
              next->size / 1024);
  } else {
    logger::line("Next OTA Slot:   none");
  }

  logger::line("Partitions:");
  esp_partition_iterator_t it =
      esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, nullptr);
  while (it != nullptr) {
    const esp_partition_t* part = esp_partition_get(it);
    char kind[24];
    snprintf(kind, sizeof(kind), "%s/%s", partitionTypeName(part->type),
             partitionSubtypeName(part->type, part->subtype));
    logger::line("  %-9s %-13s @ 0x%08X %5u KB", part->label, kind, part->address,
              part->size / 1024);
    it = esp_partition_next(it);
  }
  esp_partition_iterator_release(it);   // 允许传 NULL
  logger::blank();
}

void printNvs() {
  core::config.printUsage();
}

void reboot() {
  logger::line("[SYSTEM] Restarting...");
  Serial.flush();          // 先发完再重启，否则提示会被截断
  delay(300);
  ESP.restart();
}

// ── 命令处理函数 ──
void infoHandler(const char* /*args*/)   { printInfo(); }
void heapHandler(const char* /*args*/)   { printHeap(); }
void flashHandler(const char* /*args*/)  { printFlash(); }
void nvsHandler(const char* /*args*/)    { printNvs(); }
void rebootHandler(const char* /*args*/) { reboot(); }

}  // namespace

namespace core {
namespace sys {

void begin() {
  command::add("system info", infoHandler, "Show chip information");
  command::add("system heap", heapHandler, "Show free heap memory");
  command::add("system flash", flashHandler, "Show flash usage and partitions");
  command::add("system nvs", nvsHandler, "Show NVS usage and namespaces");
  command::add("reboot", rebootHandler, "Restart ESP32");
}

}  // namespace sys
}  // namespace core
