// doctor.h —— modules 层：功能自检
//
// doctor 只注册一条命令，不参与主循环；它通过各模块暴露的公开接口
// 读取配置/连接状态，不直接访问模块内部或 NVS。

#pragma once

namespace modules {
namespace doctor {

// 注册 doctor 命令
void begin();

}  // namespace doctor
}  // namespace modules
