// command.cpp —— core 层：命令注册表实现

#include "command.h"

#include <string.h>

#include "logger.h"

namespace {

constexpr int MAX_COMMANDS = 24;

struct Entry {
  const char* name;
  command::Handler handler;
  const char* help;   // nullptr = 不在 help 列表显示
};

Entry gEntries[MAX_COMMANDS];
int   gCount = 0;

bool matches(const char* line, const char* name) {
  const size_t n = strlen(name);
  if (strncmp(line, name, n) != 0) {
    return false;
  }
  // 命令名之后必须是字符串结束或空格，避免 "wifi setx" 匹配到 "wifi set"
  return line[n] == '\0' || line[n] == ' ';
}

}  // namespace

namespace command {

void add(const char* name, Handler handler, const char* help) {
  if (name == nullptr || handler == nullptr) {
    return;
  }
  if (gCount >= MAX_COMMANDS) {
    logger::line("[ERROR] Command table full, '%s' not registered.", name);
    return;
  }
  gEntries[gCount++] = Entry{name, handler, help};
}

bool dispatch(const char* line) {
  if (line == nullptr) {
    return false;
  }
  while (*line == ' ' || *line == '\t') {
    line++;
  }
  if (*line == '\0') {
    return true;   // 空命令，什么都不做
  }

  const Entry* best = nullptr;
  size_t bestLen = 0;

  for (int i = 0; i < gCount; i++) {
    const size_t len = strlen(gEntries[i].name);
    if (len > bestLen && matches(line, gEntries[i].name)) {
      best = &gEntries[i];
      bestLen = len;
    }
  }

  if (best == nullptr) {
    return false;
  }

  const char* args = line + bestLen;
  while (*args == ' ' || *args == '\t') {
    args++;
  }
  best->handler(args);
  return true;
}

void printHelp() {
  logger::blank();
  logger::line("Available commands:");
  for (int i = 0; i < gCount; i++) {
    if (gEntries[i].help == nullptr) {
      continue;
    }
    logger::line("  %-26s - %s", gEntries[i].name, gEntries[i].help);
  }
  logger::blank();
}

}  // namespace command
