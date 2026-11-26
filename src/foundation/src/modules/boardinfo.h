// boardinfo.h —— modules 层：开机板卡摘要
//
// 在 setup() 末尾打印一次：固件、芯片、CPU、Flash/App 占用、堆、NVS、
// MAC 地址，以及当前的 Wi-Fi / MQTT 状态。只读，不修改任何配置。

#pragma once

namespace modules {
namespace boardinfo {

// 立即打印一次板卡摘要（当前状态快照）
void begin();

}  // namespace boardinfo
}  // namespace modules
