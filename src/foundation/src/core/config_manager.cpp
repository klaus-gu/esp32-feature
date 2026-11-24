// config_manager.cpp —— core 层：NVS 配置管理实现
//
// 这是固件里唯一允许直接操作 Preferences / NVS 的文件。
// 其它模块通过 core::config 的 get/set/remove 访问配置。

#include "config_manager.h"

#include <Preferences.h>
#include <esp_err.h>
#include <esp_partition.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <string.h>

#include "logger.h"

namespace {

constexpr size_t NVS_NAME_MAX       = 15;   // NVS 命名空间/键名的硬限制
constexpr int    MAX_TRACKED_NS     = 8;

char gNamespaces[MAX_TRACKED_NS][NVS_NAME_MAX + 1];
int  gNamespaceCount = 0;

bool validName(const char* name) {
  if (name == nullptr) {
    return false;
  }
  const size_t len = strlen(name);
  return len > 0 && len <= NVS_NAME_MAX;
}

void trackNamespace(const char* ns) {
  for (int i = 0; i < gNamespaceCount; i++) {
    if (strcmp(gNamespaces[i], ns) == 0) {
      return;
    }
  }
  if (gNamespaceCount >= MAX_TRACKED_NS) {
    return;
  }
  strncpy(gNamespaces[gNamespaceCount], ns, NVS_NAME_MAX);
  gNamespaces[gNamespaceCount][NVS_NAME_MAX] = '\0';
  gNamespaceCount++;
}

// 打开一个 NVS 命名空间。读取失败是正常情况（配置还没写过），因此不打印日志。
bool openPrefs(const char* ns, bool readOnly, Preferences& prefs) {
  if (!validName(ns)) {
    return false;
  }
  if (!prefs.begin(ns, readOnly)) {
    return false;
  }
  trackNamespace(ns);
  return true;
}

void reportWriteFailure(const char* ns, const char* key) {
  logger::line("[ERROR] NVS write failed: %s/%s", ns == nullptr ? "?" : ns,
               key == nullptr ? "?" : key);
}

}  // namespace

namespace core {

bool ConfigManager::begin() {
  if (ready_) {
    return true;
  }

  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    // 标准恢复流程：NVS 页耗尽或版本不兼容时只能整区擦除。
    // 会丢失已有配置，必须明确打印，避免静默清空。
    logger::line("[NVS] Reinitializing, stored config will be erased: %s", esp_err_to_name(err));
    const esp_err_t eraseErr = nvs_flash_erase();
    if (eraseErr != ESP_OK) {
      logger::line("[ERROR] NVS erase failed: %s", esp_err_to_name(eraseErr));
      return false;
    }
    err = nvs_flash_init();
  }

  if (err != ESP_OK) {
    logger::line("[ERROR] NVS init failed: %s", esp_err_to_name(err));
    return false;
  }

  ready_ = true;
  return true;
}

bool ConfigManager::has(const char* ns, const char* key) const {
  if (!validName(key)) {
    return false;
  }
  Preferences prefs;
  if (!openPrefs(ns, true, prefs)) {
    return false;
  }
  const bool found = prefs.isKey(key);
  prefs.end();
  return found;
}

String ConfigManager::getString(const char* ns, const char* key, const char* fallback) const {
  const String def = (fallback == nullptr) ? String() : String(fallback);
  if (!validName(key)) {
    return def;
  }
  Preferences prefs;
  if (!openPrefs(ns, true, prefs)) {
    return def;
  }
  const String value = prefs.isKey(key) ? prefs.getString(key, def) : def;
  prefs.end();
  return value;
}

bool ConfigManager::setString(const char* ns, const char* key, const char* value) {
  if (!validName(key) || value == nullptr) {
    return false;
  }
  Preferences prefs;
  if (!openPrefs(ns, false, prefs)) {
    reportWriteFailure(ns, key);
    return false;
  }
  // Preferences::putString 成功时返回 strlen(value)，空字符串会返回 0，
  // 无法用返回值判断成败，所以写入后回读校验。
  prefs.putString(key, value);
  const bool ok = prefs.isKey(key) && prefs.getString(key, "") == value;
  prefs.end();
  if (!ok) {
    reportWriteFailure(ns, key);
  }
  return ok;
}

int32_t ConfigManager::getInt(const char* ns, const char* key, int32_t fallback) const {
  if (!validName(key)) {
    return fallback;
  }
  Preferences prefs;
  if (!openPrefs(ns, true, prefs)) {
    return fallback;
  }
  const int32_t value = prefs.getInt(key, fallback);
  prefs.end();
  return value;
}

bool ConfigManager::setInt(const char* ns, const char* key, int32_t value) {
  if (!validName(key)) {
    return false;
  }
  Preferences prefs;
  if (!openPrefs(ns, false, prefs)) {
    reportWriteFailure(ns, key);
    return false;
  }
  const bool ok = prefs.putInt(key, value) > 0;
  prefs.end();
  if (!ok) {
    reportWriteFailure(ns, key);
  }
  return ok;
}

uint32_t ConfigManager::getUInt(const char* ns, const char* key, uint32_t fallback) const {
  if (!validName(key)) {
    return fallback;
  }
  Preferences prefs;
  if (!openPrefs(ns, true, prefs)) {
    return fallback;
  }
  const uint32_t value = prefs.getUInt(key, fallback);
  prefs.end();
  return value;
}

bool ConfigManager::setUInt(const char* ns, const char* key, uint32_t value) {
  if (!validName(key)) {
    return false;
  }
  Preferences prefs;
  if (!openPrefs(ns, false, prefs)) {
    reportWriteFailure(ns, key);
    return false;
  }
  const bool ok = prefs.putUInt(key, value) > 0;
  prefs.end();
  if (!ok) {
    reportWriteFailure(ns, key);
  }
  return ok;
}

bool ConfigManager::getBool(const char* ns, const char* key, bool fallback) const {
  if (!validName(key)) {
    return fallback;
  }
  Preferences prefs;
  if (!openPrefs(ns, true, prefs)) {
    return fallback;
  }
  const bool value = prefs.getBool(key, fallback);
  prefs.end();
  return value;
}

bool ConfigManager::setBool(const char* ns, const char* key, bool value) {
  if (!validName(key)) {
    return false;
  }
  Preferences prefs;
  if (!openPrefs(ns, false, prefs)) {
    reportWriteFailure(ns, key);
    return false;
  }
  const bool ok = prefs.putBool(key, value) > 0;
  prefs.end();
  if (!ok) {
    reportWriteFailure(ns, key);
  }
  return ok;
}

float ConfigManager::getFloat(const char* ns, const char* key, float fallback) const {
  if (!validName(key)) {
    return fallback;
  }
  Preferences prefs;
  if (!openPrefs(ns, true, prefs)) {
    return fallback;
  }
  const float value = prefs.getFloat(key, fallback);
  prefs.end();
  return value;
}

bool ConfigManager::setFloat(const char* ns, const char* key, float value) {
  if (!validName(key)) {
    return false;
  }
  Preferences prefs;
  if (!openPrefs(ns, false, prefs)) {
    reportWriteFailure(ns, key);
    return false;
  }
  const bool ok = prefs.putFloat(key, value) > 0;
  prefs.end();
  if (!ok) {
    reportWriteFailure(ns, key);
  }
  return ok;
}

bool ConfigManager::remove(const char* ns, const char* key) {
  if (!validName(key)) {
    return false;
  }
  Preferences prefs;
  if (!openPrefs(ns, false, prefs)) {
    reportWriteFailure(ns, key);
    return false;
  }
  // 键不存在时 Preferences::remove 也返回 false，这不算错误，不打印日志
  const bool ok = prefs.remove(key);
  prefs.end();
  return ok;
}

bool ConfigManager::clear(const char* ns) {
  Preferences prefs;
  if (!openPrefs(ns, false, prefs)) {
    reportWriteFailure(ns, "<all>");
    return false;
  }
  const bool ok = prefs.clear();
  prefs.end();
  if (!ok) {
    reportWriteFailure(ns, "<all>");
  }
  return ok;
}

ConfigUsage ConfigManager::usage() const {
  ConfigUsage out;

  const esp_partition_t* part =
      esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, nullptr);
  if (part != nullptr) {
    out.address = part->address;
    out.size    = part->size;
  }

  nvs_stats_t stats = {};
  if (nvs_get_stats("nvs", &stats) == ESP_OK) {
    out.valid        = true;
    out.totalEntries = stats.total_entries;
    out.usedEntries  = stats.used_entries;
    out.freeEntries  = stats.free_entries;
    out.namespaces   = stats.namespace_count;
  }

  return out;
}

void ConfigManager::printUsage() const {
  const ConfigUsage stats = usage();

  logger::blank();
  logger::line("[NVS]");
  if (stats.size > 0) {
    logger::line("Partition:      nvs @ 0x%08X (%u KB)", stats.address,
                 static_cast<unsigned>(stats.size / 1024));
  }

  if (!stats.valid) {
    logger::line("Status:         unavailable (NVS not initialized or stats failed)");
    logger::blank();
    return;
  }

  const unsigned pct = stats.totalEntries == 0
                           ? 0
                           : static_cast<unsigned>((100ULL * stats.usedEntries) / stats.totalEntries);
  logger::line("Entries:        %u used / %u total (%u%%), %u free",
               static_cast<unsigned>(stats.usedEntries),
               static_cast<unsigned>(stats.totalEntries), pct,
               static_cast<unsigned>(stats.freeEntries));
  logger::line("Namespaces:     %u", static_cast<unsigned>(stats.namespaces));
  logger::line("Firmware uses:  %d namespace(s)", gNamespaceCount);
  for (int i = 0; i < gNamespaceCount; i++) {
    logger::line("  - %s", gNamespaces[i]);
  }
  logger::blank();
}

}  // namespace core

// 全局唯一实例
core::ConfigManager core::config;
