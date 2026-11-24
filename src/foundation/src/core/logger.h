// log.h —— core 层：统一串口输出
//
// 为什么要有这一层：
//   之前每个模块各自 Serial.printf("...\n")，一旦写成 LF 就会让串口监视器
//   输出堆成阶梯状（光标不回行首）。线尾格式集中到这里，避免重复踩坑。
//
// 约定：调用 logger::line() 时，格式字符串里不要再写 \r\n。

#pragma once

namespace logger {

// 格式化的行，自动补 "\r\n"
void line(const char* fmt, ...);

// 空行
void blank();

// 原始输出，不追加换行（提示符、回显等场景）
void raw(const char* text);

// 原始输出一个字符（串口回显用）
void putc(char c);

}  // namespace logger
