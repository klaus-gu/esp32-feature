// system.h —— core 层：系统管理
//
// 对外只暴露 begin()：内部自行注册 system info / system heap / reboot。
// 其它层不需要知道这些命令是怎么实现的。

#pragma once

namespace core {
namespace sys {

// 注册系统类命令
void begin();

}  // namespace sys
}  // namespace core
