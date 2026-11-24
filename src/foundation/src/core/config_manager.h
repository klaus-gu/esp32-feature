// config_manager.h —— core 层：NVS 配置管理（唯一入口）
//
// 强制规则（AGENTS.md §2.4）：
//   固件的应用配置必须存放在 NVS，并且只能通过 core::config（ConfigManager）读写。
//   其它文件禁止直接 include <Preferences.h> / <nvs.h>。
//
// 用法：
//   String ssid = core::config.getString("wifi", "ssid", "");
//   core::config.setString("wifi", "ssid", ssid.c_str());
//
// 命名空间（namespace）和键名（key）都必须 ≤15 字符，这是 NVS 的硬限制。

#pragma once

#include <Arduino.h>

namespace core {

// NVS 使用情况快照（供 system nvs / system flash 诊断）
struct ConfigUsage {
  bool     valid        = false;   // nvs_get_stats 是否成功
  uint32_t address      = 0;       // nvs 分区起始地址
  uint32_t size         = 0;       // nvs 分区大小（字节）
  size_t   totalEntries = 0;       // 总条目数
  size_t   usedEntries  = 0;       // 已用条目数
  size_t   freeEntries  = 0;       // 空闲条目数
  size_t   namespaces   = 0;       // NVS 里的命名空间总数
};

class ConfigManager {
 public:
  // 初始化 NVS（幂等）。失败返回 false；调用方应降级运行，不得重启风暴。
  bool begin();
  bool ready() const { return ready_; }

  bool has(const char* ns, const char* key) const;

  // 字符串
  String getString(const char* ns, const char* key, const char* fallback = "") const;
  bool   setString(const char* ns, const char* key, const char* value);

  // 有符号 / 无符号整数
  int32_t  getInt(const char* ns, const char* key, int32_t fallback = 0) const;
  bool     setInt(const char* ns, const char* key, int32_t value);
  uint32_t getUInt(const char* ns, const char* key, uint32_t fallback = 0) const;
  bool     setUInt(const char* ns, const char* key, uint32_t value);

  // 布尔 / 浮点
  bool  getBool(const char* ns, const char* key, bool fallback = false) const;
  bool  setBool(const char* ns, const char* key, bool value);
  float getFloat(const char* ns, const char* key, float fallback = 0.0f) const;
  bool  setFloat(const char* ns, const char* key, float value);

  // 删除单个键 / 清空整个命名空间
  bool remove(const char* ns, const char* key);
  bool clear(const char* ns);

  // 诊断
  ConfigUsage usage() const;
  void        printUsage() const;

 private:
  bool ready_ = false;
};

// 全局唯一实例，所有模块共用
extern ConfigManager config;

}  // namespace core
