// log.cpp —— core 层：统一串口输出实现

#include "logger.h"

#include <Arduino.h>
#include <stdarg.h>
#include <stdio.h>

namespace {

// 单行上限；超长会截断，避免把栈撑爆
constexpr size_t LOG_LINE_MAX = 256;

}  // namespace

namespace logger {

void line(const char* fmt, ...) {
  char buf[LOG_LINE_MAX];

  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);

  Serial.write(reinterpret_cast<const uint8_t*>(buf), strlen(buf));
  Serial.print("\r\n");
}

void blank() {
  Serial.print("\r\n");
}

void raw(const char* text) {
  Serial.print(text);
}

void putc(char c) {
  Serial.write(static_cast<uint8_t>(c));
}

}  // namespace logger
