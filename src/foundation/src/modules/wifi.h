// wifi.h —— modules 层：Wi-Fi 模块对外的唯一入口
//
// 其它层只通过 begin()/poll() 使用它；
// 需要查询网络状态时用 modules/network.h 的接口，而不是直接依赖本文件。

#pragma once

namespace modules {
namespace wifi {

// 初始化 STA、加载 NVS 凭据、注册 wifi 命令、发起连接（不阻塞）
void begin();

// 周期调用：状态变化打印、断线重连（不阻塞）
void poll();

}  // namespace wifi
}  // namespace modules
