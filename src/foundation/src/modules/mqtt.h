// mqtt.h —— modules 层：MQTT 模块对外的唯一入口
//
// 与 Wi-Fi 模块保持一致：其它层只通过 begin()/poll() 使用本模块。
// 网络状态一律走 modules/network.h 的 net::Provider，禁止 include <WiFi.h>。
// MQTT 配置存在 NVS 命名空间 "mqtt"，读写统一经过 core::config（AGENTS.md 2.4）。

#pragma once

#include <stdint.h>

namespace modules {
namespace mqtt {

// 加载 NVS 配置、注册 mqtt 命令、初始化客户端（不阻塞）
void begin();

// 周期调用：网络状态、断线重连、配置热更新（不阻塞）
void poll();

// 是否已配置 broker（host 非空）
bool configured();

// 是否已连接 broker（收到 CONNACK）
bool connected();

// 已配置的 broker 主机名/端口（未配置时 host 为空）
const char* configuredHost();
uint16_t configuredPort();

}  // namespace mqtt
}  // namespace modules
