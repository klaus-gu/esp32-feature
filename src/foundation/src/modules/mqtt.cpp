// mqtt.cpp —— modules 层：MQTT 模块实现
//
// 设计要点：
//   · 客户端用 espMqttClient 的内部 FreeRTOS 任务模式，主循环里 connect() 只负责
//     入队/唤醒，不对 loop() 产生阻塞；
//   · 配置全部存 NVS 命名空间 "mqtt"，读写统一走 core::config；
//   · 网络状态一律通过 net::provider() 获取，不 include <WiFi.h>；
//   · 断线由 poll() 按 cfg::MQTT_RETRY_MS 重连，配置变更后重新下发参数；
//   · 密码只写 NVS / 交给客户端，任何日志和状态输出都不打印内容。

#include "mqtt.h"

#include <Arduino.h>
#include <espMqttClient.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../config.h"
#include "../core/command.h"
#include "../core/config_manager.h"
#include "../core/logger.h"
#include "network.h"

namespace {

constexpr const char* NVS_NS = "mqtt";

constexpr size_t HOST_MAX   = 64;
constexpr size_t CLIENT_MAX = 32;
constexpr size_t USER_MAX   = 64;
constexpr size_t PASS_MAX   = 64;
constexpr size_t VALUE_MAX  = 80;

constexpr uint32_t NET_CHECK_MS = 1000;

// espMqttClient 会保存这些指针，所以用固定缓冲区，避免 String 重新分配后悬空
char gHost[HOST_MAX]     = {0};
char gClientId[CLIENT_MAX] = {0};
char gUser[USER_MAX]     = {0};
char gPass[PASS_MAX]     = {0};

uint16_t gPort      = cfg::MQTT_PORT_DEFAULT;
uint16_t gKeepAlive = cfg::MQTT_KEEPALIVE_DEFAULT;

bool gConfigured = false;
bool gEnabled    = true;

bool     gApplyPending = false;
bool     gWasConnected = false;
bool     gNetReady     = false;
uint32_t gLastNetCheck = 0;
uint32_t gLastAttempt  = 0;

// 内部任务模式：priority 1，固定到 core 0，避免占用 Arduino loop 所在的 core 1
espMqttClient gClient(1, 0);

void copyString(char* dst, size_t size, const char* src) {
  if (size == 0) {
    return;
  }
  if (src == nullptr) {
    dst[0] = '\0';
    return;
  }
  strncpy(dst, src, size - 1);
  dst[size - 1] = '\0';
}

void buildDefaultClientId() {
  const uint32_t mac = static_cast<uint32_t>(ESP.getEfuseMac() & 0xFFFFFFULL);
  snprintf(gClientId, sizeof(gClientId), "turbo-%06X", mac);
}

void loadConfig() {
  const String host = core::config.getString(NVS_NS, "host", "");
  copyString(gHost, sizeof(gHost), host.c_str());

  gPort = static_cast<uint16_t>(core::config.getUInt(NVS_NS, "port", cfg::MQTT_PORT_DEFAULT));
  if (gPort == 0) {
    gPort = cfg::MQTT_PORT_DEFAULT;
  }

  const String client = core::config.getString(NVS_NS, "client", "");
  if (client.length() > 0) {
    copyString(gClientId, sizeof(gClientId), client.c_str());
  } else {
    buildDefaultClientId();
  }

  const String user = core::config.getString(NVS_NS, "user", "");
  copyString(gUser, sizeof(gUser), user.c_str());

  const String pass = core::config.getString(NVS_NS, "pass", "");
  copyString(gPass, sizeof(gPass), pass.c_str());

  gKeepAlive =
      static_cast<uint16_t>(core::config.getUInt(NVS_NS, "keepalive", cfg::MQTT_KEEPALIVE_DEFAULT));
  if (gKeepAlive == 0) {
    gKeepAlive = cfg::MQTT_KEEPALIVE_DEFAULT;
  }

  gEnabled    = core::config.getBool(NVS_NS, "enabled", true);
  gConfigured = (gHost[0] != '\0');
}

void configureClient() {
  if (!gConfigured) {
    return;
  }
  gClient.setServer(gHost, gPort);
  gClient.setClientId(gClientId);
  gClient.setKeepAlive(gKeepAlive);
  if (gUser[0] != '\0') {
    gClient.setCredentials(gUser, gPass[0] != '\0' ? gPass : nullptr);
  } else {
    gClient.setCredentials(nullptr, nullptr);
  }
}

// 配置变更后统一走这里：先断开，等 poll() 确认客户端已停止后再重新加载并下发参数
void applyConfig() {
  gApplyPending = true;
  gLastAttempt  = 0;
  if (!gClient.disconnected()) {
    gClient.disconnect(true);
  }
}

void updateNetwork(uint32_t now) {
  if (now - gLastNetCheck < NET_CHECK_MS) {
    return;
  }
  gLastNetCheck = now;
  net::Provider* provider = net::provider();
  gNetReady = (provider != nullptr) && provider->status().connected;
}

// ── 命令处理 ────────────────────────────────────────────────────────────────

void printUsage() {
  logger::line("[MQTT] Usage:");
  logger::line("  mqtt set host <host>");
  logger::line("  mqtt set port <1-65535>");
  logger::line("  mqtt set client <client-id>");
  logger::line("  mqtt set user <username>");
  logger::line("  mqtt set pass <password>");
  logger::line("  mqtt set keepalive <seconds>");
}

void statusCommand(const char* /*args*/) {
  logger::blank();
  logger::line("[MQTT]");
  logger::line("Enabled:        %s", gEnabled ? "yes" : "no");
  if (gConfigured) {
    logger::line("Broker:         %s:%u", gHost, static_cast<unsigned>(gPort));
  } else {
    logger::line("Broker:         (not configured)");
  }
  logger::line("Client ID:      %s", gClientId[0] != '\0' ? gClientId : "(none)");
  if (gUser[0] == '\0') {
    logger::line("Auth:           none");
  } else {
    logger::line("Auth:           username set, password %s", gPass[0] != '\0' ? "set" : "empty");
  }
  logger::line("Keepalive:      %u s", static_cast<unsigned>(gKeepAlive));
  logger::line("Network:        %s", gNetReady ? "connected" : "disconnected");

  const char* state = "DISCONNECTED";
  if (!gConfigured) {
    state = "NOT CONFIGURED";
  } else if (gClient.connected()) {
    state = "CONNECTED";
  }
  logger::line("State:          %s", state);

  if (gConfigured && gEnabled && gNetReady && gClient.disconnected()) {
    const uint32_t now = millis();
    uint32_t left = 0;
    if (gLastAttempt != 0 && (now - gLastAttempt) < cfg::MQTT_RETRY_MS) {
      left = cfg::MQTT_RETRY_MS - (now - gLastAttempt);
    }
    logger::line("Next retry:     %u s", static_cast<unsigned>((left + 999) / 1000));
  }
  logger::blank();
}

bool parseSetArgs(const char* args, char* key, size_t keySize, char* value, size_t valueSize) {
  char buf[192];
  strncpy(buf, args == nullptr ? "" : args, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';

  char* p = buf;
  while (*p == ' ' || *p == '\t') {
    p++;
  }
  if (*p == '\0') {
    return false;
  }

  char* k = p;
  while (*p != '\0' && *p != ' ' && *p != '\t') {
    p++;
  }
  if (*p != '\0') {
    *p++ = '\0';
  }
  while (*p == ' ' || *p == '\t') {
    p++;
  }

  char* v = p;
  if (*v == '"') {
    v++;
    char* end = strchr(v, '"');
    if (end != nullptr) {
      *end = '\0';
    }
  } else {
    size_t len = strlen(v);
    while (len > 0 && (v[len - 1] == ' ' || v[len - 1] == '\t')) {
      v[--len] = '\0';
    }
  }

  if (strlen(k) >= keySize || strlen(v) >= valueSize) {
    return false;
  }
  strcpy(key, k);
  strcpy(value, v);
  return true;
}

bool parsePort(const char* text, uint16_t& out) {
  if (text == nullptr || text[0] == '\0') {
    return false;
  }
  char* end = nullptr;
  const unsigned long value = strtoul(text, &end, 10);
  if (end == nullptr || *end != '\0' || value == 0 || value > 65535UL) {
    return false;
  }
  out = static_cast<uint16_t>(value);
  return true;
}

void setCommand(const char* args) {
  char key[16];
  char value[VALUE_MAX];
  if (!parseSetArgs(args, key, sizeof(key), value, sizeof(value))) {
    printUsage();
    return;
  }

  if (strcmp(key, "host") == 0) {
    if (value[0] == '\0' || strpbrk(value, " \t") != nullptr) {
      logger::line("[ERROR] Usage: mqtt set host <host>");
      return;
    }
    if (strlen(value) >= sizeof(gHost)) {
      logger::line("[ERROR] MQTT host too long (max %u).", static_cast<unsigned>(sizeof(gHost) - 1));
      return;
    }
    if (!core::config.setString(NVS_NS, "host", value)) {
      return;
    }
  } else if (strcmp(key, "port") == 0) {
    uint16_t port = 0;
    if (!parsePort(value, port)) {
      logger::line("[ERROR] Usage: mqtt set port <1-65535>");
      return;
    }
    if (!core::config.setUInt(NVS_NS, "port", port)) {
      return;
    }
  } else if (strcmp(key, "client") == 0) {
    if (value[0] != '\0') {
      if (strlen(value) >= sizeof(gClientId)) {
        logger::line("[ERROR] MQTT client id too long (max %u).",
                     static_cast<unsigned>(sizeof(gClientId) - 1));
        return;
      }
      if (!core::config.setString(NVS_NS, "client", value)) {
        return;
      }
    } else {
      core::config.remove(NVS_NS, "client");
    }
  } else if (strcmp(key, "user") == 0) {
    if (value[0] != '\0') {
      if (strlen(value) >= sizeof(gUser)) {
        logger::line("[ERROR] MQTT username too long (max %u).",
                     static_cast<unsigned>(sizeof(gUser) - 1));
        return;
      }
      if (!core::config.setString(NVS_NS, "user", value)) {
        return;
      }
    } else {
      core::config.remove(NVS_NS, "user");
    }
  } else if (strcmp(key, "pass") == 0) {
    if (value[0] != '\0') {
      if (strlen(value) >= sizeof(gPass)) {
        logger::line("[ERROR] MQTT password too long (max %u).",
                     static_cast<unsigned>(sizeof(gPass) - 1));
        return;
      }
      if (!core::config.setString(NVS_NS, "pass", value)) {
        return;
      }
    } else {
      core::config.remove(NVS_NS, "pass");
    }
  } else if (strcmp(key, "keepalive") == 0) {
    uint16_t keepAlive = 0;
    if (!parsePort(value, keepAlive)) {
      logger::line("[ERROR] Usage: mqtt set keepalive <1-65535>");
      return;
    }
    if (!core::config.setUInt(NVS_NS, "keepalive", keepAlive)) {
      return;
    }
  } else {
    logger::line("[ERROR] Unknown mqtt key '%s'.", key);
    printUsage();
    return;
  }

  applyConfig();

  if (strcmp(key, "pass") == 0) {
    logger::line("[MQTT] pass updated (value not printed).");
  } else {
    logger::line("[MQTT] %s saved.", key);
  }
}

void connectCommand(const char* /*args*/) {
  if (!core::config.setBool(NVS_NS, "enabled", true)) {
    return;
  }
  gEnabled    = true;
  gLastAttempt = 0;
  logger::line("[MQTT] Auto-connect enabled.");
  if (!gConfigured) {
    logger::line("[MQTT] Set broker first: mqtt set host <host>");
  }
}

void disconnectCommand(const char* /*args*/) {
  if (!core::config.setBool(NVS_NS, "enabled", false)) {
    return;
  }
  gEnabled = false;
  gClient.disconnect(true);
  logger::line("[MQTT] Auto-connect disabled.");
}

void clearCommand(const char* /*args*/) {
  if (!core::config.clear(NVS_NS)) {
    logger::line("[ERROR] Failed to clear MQTT configuration.");
    return;
  }
  gClient.disconnect(true);
  gApplyPending = true;
  gLastAttempt  = 0;
  logger::line("[MQTT] Configuration cleared.");
}

}  // namespace

namespace modules {
namespace mqtt {

void begin() {
  loadConfig();
  if (gConfigured) {
    configureClient();
  }

  command::add("mqtt status", statusCommand, "Show MQTT connection status");
  command::add("mqtt set", setCommand, "Set MQTT config (host/port/client/user/pass/keepalive)");
  command::add("mqtt connect", connectCommand, "Enable MQTT auto-connect");
  command::add("mqtt disconnect", disconnectCommand, "Disable MQTT auto-connect");
  command::add("mqtt clear", clearCommand, "Erase MQTT configuration");

  net::Provider* provider = net::provider();
  gNetReady     = (provider != nullptr) && provider->status().connected;
  gLastNetCheck = millis();

  if (!gConfigured) {
    logger::line("[MQTT] Not configured. Use 'mqtt set host <host>'.");
    return;
  }

  logger::line("[MQTT] Broker %s:%u, client \"%s\", auto-connect %s.", gHost,
               static_cast<unsigned>(gPort), gClientId, gEnabled ? "on" : "off");
  if (!gEnabled) {
    logger::line("[MQTT] Auto-connect disabled. Use 'mqtt connect'.");
  }
}

void poll() {
  const uint32_t now = millis();
  updateNetwork(now);

  const bool connected = gClient.connected();
  if (connected != gWasConnected) {
    gWasConnected = connected;
    if (connected) {
      logger::line("[MQTT] Connected to %s:%u.", gHost, static_cast<unsigned>(gPort));
    } else {
      logger::line("[MQTT] Disconnected.");
      gLastAttempt = now;
    }
  }

  // Wi-Fi 掉线或配置变更时主动结束 MQTT 连接，避免停在“假连接”状态。
  // 连接过程中库可能把状态推回 connecting，所以这里在 pending 期间持续纠正。
  if ((!gNetReady || gApplyPending) && !gClient.disconnected()) {
    gClient.disconnect(true);
  }

  // 配置变更后，等客户端完全停下再重新加载并下发参数
  if (gApplyPending && gClient.disconnected()) {
    loadConfig();
    configureClient();
    gApplyPending = false;
    gLastAttempt  = 0;
  }

  if (!gEnabled || !gConfigured || !gNetReady) {
    return;
  }
  if (!gClient.disconnected()) {
    return;
  }
  if (gLastAttempt != 0 && (now - gLastAttempt) < cfg::MQTT_RETRY_MS) {
    return;
  }

  gLastAttempt = now;
  if (gClient.connect()) {
    logger::line("[MQTT] Connecting to %s:%u ...", gHost, static_cast<unsigned>(gPort));
  } else {
    logger::line("[MQTT] Connect request rejected (client busy).");
  }
}

bool configured() {
  return gConfigured;
}

bool connected() {
  return gClient.connected();
}

const char* configuredHost() {
  return gHost;
}

uint16_t configuredPort() {
  return gPort;
}

}  // namespace mqtt
}  // namespace modules
