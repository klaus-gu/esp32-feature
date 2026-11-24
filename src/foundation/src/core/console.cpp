// console.cpp —— core 层：串口输入处理
//
// 输入特性：
//   · 缓冲区有上限，超长立即报错并丢弃整行
//   · 回车兼容 \r\n / \n / \r，\r\n 不会重复执行
//   · 本地回显 + 退格，普通串口监视器里也看得见自己打的字
//   · 丢弃 0x00/0xFF 噪声字节，行首只接受可见 ASCII

#include "console.h"

#include <Arduino.h>
#include <string.h>

#include "../config.h"
#include "command.h"
#include "logger.h"

namespace {

char   gBuf[cfg::CMD_BUFFER_SIZE];
size_t gLen         = 0;
bool   gLastWasCR   = false;
bool   gDiscardLine = false;   // 本行已因超长报错，余下字符全部丢弃

void helpHandler(const char* /*args*/) {
  command::printHelp();
}

}  // namespace

namespace core {
namespace console {

void begin() {
  // 横幅
  logger::blank();
  logger::line("================================");
  logger::line(" %s", FW_ID);
  logger::line(" Framework: %s", FW_FRAMEWORK);
  logger::line(" Type 'help' for commands.");
  logger::line("================================");

  // 注册 console 自己的命令
  command::add("help", helpHandler, "Show command list");
}

void poll() {
  while (Serial.available() > 0) {
    const int rawByte = Serial.read();
    if (rawByte < 0) {
      break;
    }
    const char c = static_cast<char>(rawByte);

    // ── 回车 ──
    if (c == '\r' || c == '\n') {
      if (c == '\n' && gLastWasCR) {
        gLastWasCR = false;          // \r\n 里的 \n 已经处理过
        continue;
      }
      gLastWasCR = (c == '\r');

      logger::blank();
      if (!gDiscardLine) {
        gBuf[gLen] = '\0';
        if (!command::dispatch(gBuf)) {
          logger::line("[ERROR] Unknown command. Type 'help'.");
        }
      }
      gLen = 0;
      gDiscardLine = false;
      continue;
    }
    gLastWasCR = false;

    if (gDiscardLine) {
      continue;
    }

    // ── 退格 ──
    if (c == 0x08 || c == 0x7F) {
      if (gLen > 0) {
        gLen--;
        logger::raw("\b \b");
      }
      continue;
    }

    // ── 噪声字节（复位/切射频时串口线上会出现）──
    const unsigned char uc = static_cast<unsigned char>(c);
    if (uc == 0x00 || uc == 0xFF || uc < 32) {
      continue;
    }
    if (gLen == 0 && (uc < 33 || uc > 126)) {
      continue;                      // 行首必须是可见 ASCII
    }

    // ── 超长保护 ──
    if (gLen >= cfg::CMD_BUFFER_SIZE - 1) {
      logger::blank();
      logger::line("[ERROR] Command too long. Input discarded.");
      gLen = 0;
      gDiscardLine = true;
      continue;
    }

    gBuf[gLen++] = c;
    logger::putc(c);                    // 回显
  }
}

}  // namespace console
}  // namespace core
