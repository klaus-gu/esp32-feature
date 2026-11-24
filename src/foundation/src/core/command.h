// command.h —— core 层：命令注册表
//
// 设计目的（对应"模块之间通过接口交互"）：
//   console 只负责收字符，不需要知道有哪些命令；
//   各层在初始化时自己把命令注册进来。新增模块只改自己的文件，
//   不用回头修改 console 的分发代码，help 列表也会自动更新。

#pragma once

namespace command {

// 参数 args 为命令名之后的剩余文本（可能为空字符串，不会是 nullptr）
using Handler = void (*)(const char* args);

// 注册命令。name 可以含空格（例如 "system info"）。
// help 为 help 列表里显示的说明；传 nullptr 则不在列表中出现。
void add(const char* name, Handler handler, const char* help);

// 按"最长命令名"匹配并调用；返回 false 表示没有匹配到
bool dispatch(const char* line);

// 打印已注册的命令列表
void printHelp();

}  // namespace command
