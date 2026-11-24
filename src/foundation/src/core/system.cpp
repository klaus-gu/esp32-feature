// system.cpp —— core 层：系统管理实现
//
// 全部基于 Arduino-ESP32 已有的 ESP.* 接口，不重复实现底层能力。

#include "system.h"

#include <Arduino.h>

#include "../config.h"
#include "command.h"
#include "logger.h"

namespace {

void printInfo() {
  logger::blank();
  logger::line("[SYSTEM INFO]");
  logger::line("Chip Model:     %s rev %d", ESP.getChipModel(), ESP.getChipRevision());
  logger::line("CPU Frequency:  %u MHz", ESP.getCpuFreqMHz());
  logger::line("Flash Size:     %u bytes (%u KB)",
            ESP.getFlashChipSize(), ESP.getFlashChipSize() / 1024);
  logger::line("SDK Version:    %s", ESP.getSdkVersion());
  logger::line("Firmware:       %s v%s", FW_NAME, FW_VERSION);
  logger::blank();
}

void printHeap() {
  logger::blank();
  logger::line("[MEMORY]");
  logger::line("Free Heap:         %u bytes", ESP.getFreeHeap());
  logger::line("Minimum Free Heap: %u bytes", ESP.getMinFreeHeap());
  logger::blank();
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
void rebootHandler(const char* /*args*/) { reboot(); }

}  // namespace

namespace core {
namespace sys {

void begin() {
  command::add("system info", infoHandler, "Show chip information");
  command::add("system heap", heapHandler, "Show free heap memory");
  command::add("reboot", rebootHandler, "Restart ESP32");
}

}  // namespace sys
}  // namespace core
