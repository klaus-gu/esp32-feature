// console.h —— core 层：串口命令控制台
//
// 职责边界：只负责"字符 → 一行命令 → 交给 command 分发"。
// 不认识 WiFi，也不认识任何具体功能。

#pragma once

namespace core {
namespace console {

// 打印启动横幅，并注册 help 命令
void begin();

// 处理串口输入（非阻塞，主循环里反复调用）
void poll();

}  // namespace console
}  // namespace core
