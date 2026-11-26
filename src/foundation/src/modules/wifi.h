// wifi.h —— modules 层：Wi-Fi 模块对外的唯一入口
//
// 其它层只通过 begin()/poll() 使用它；
// 需要查询网络状态时用 modules/network.h 的接口，而不是直接依赖本文件。

#pragma once

namespace modules {
namespace wifi {

// 初始化 STA、加载 NVS 凭据、注册 wifi 命令（不发起连接）
void begin();

// 发起首次连接；在开机摘要打印之后由 foundation 调用
void start();

// 周期调用：状态变化打印、断线重连（不阻塞）
void poll();

// 是否已保存 Wi-Fi 凭据（不代表当前已连接）
bool hasCredentials();

// 已保存的 SSID（未配置时返回空字符串）
const char* configuredSsid();


}  // namespace wifi
}  // namespace modules
