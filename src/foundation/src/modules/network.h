// network.h —— modules 层：网络状态接口（不含任何 Wi-Fi 实现细节）
//
// 这就是"模块之间靠接口交互"的落点：
//   MQTT 等模块想知道"现在有没有网、IP 是多少、信号如何"，
//   只需要包含本文件并调用 net::provider()->status()，
//   完全不需要 include <WiFi.h>，也不需要知道 Wi-Fi 模块的内部分实现。
//
// 具体实现由某个网络模块（当前是 wifi）在初始化时通过 setProvider() 注入。

#pragma once

#include <Arduino.h>
#include <IPAddress.h>

namespace net {

struct Status {
  bool      connected = false;
  String    ssid;
  IPAddress ip;
  int32_t   rssi = 0;        // dBm
};

class Provider {
 public:
  virtual ~Provider() = default;
  virtual Status status() const = 0;
};

// 由具体网络模块注册（传 nullptr 表示撤销）
void setProvider(Provider* provider);

// 可能返回 nullptr（尚未注册任何网络模块）
Provider* provider();

}  // namespace net
