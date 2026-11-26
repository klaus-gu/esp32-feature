// wifi.cpp —— modules 层：Wi-Fi 模块实现
//
// 模块自治：连接、重连、扫描、凭据存储、命令注册都在本文件内完成，
// 外部（core / foundation.ino / 其它 modules）不需要了解实现细节。
//
// 对外提供两样东西：
//   1. modules::wifi::begin() / poll()        生命周期
//   2. net::Provider 接口（modules/network.h）网络状态查询

#include "wifi.h"

#include <Arduino.h>
#include <WiFi.h>
#include <string.h>

#include "../config.h"
#include "../core/command.h"
#include "../core/config_manager.h"
#include "../core/logger.h"
#include "network.h"

namespace {

String      gSsid;
String      gPass;
bool        gHasCreds     = false;
bool        gWasConnected = false;
uint8_t     gRetryCount   = 0;
bool        gGaveUp       = false;
uint32_t    gLastAttempt  = 0;

const char* statusText(wl_status_t s) {
  switch (s) {
    case WL_IDLE_STATUS:     return "IDLE";
    case WL_NO_SSID_AVAIL:   return "NO_SSID_AVAIL";
    case WL_SCAN_COMPLETED:  return "SCAN_COMPLETED";
    case WL_CONNECTED:       return "CONNECTED";
    case WL_CONNECT_FAILED:  return "CONNECT_FAILED";
    case WL_CONNECTION_LOST: return "CONNECTION_LOST";
    case WL_DISCONNECTED:    return "DISCONNECTED";
    default:                 return "UNKNOWN";
  }
}

void loadCredentials() {
  gSsid = core::config.getString(cfg::NVS_NAMESPACE, cfg::NVS_KEY_SSID, "");
  gPass = core::config.getString(cfg::NVS_NAMESPACE, cfg::NVS_KEY_PASS, "");
  gHasCreds = gSsid.length() > 0;
}

bool saveCredentials(const char* ssid, const char* pass) {
  const String password = (pass == nullptr) ? "" : pass;

  const bool okSsid = core::config.setString(cfg::NVS_NAMESPACE, cfg::NVS_KEY_SSID, ssid);
  const bool okPass = core::config.setString(cfg::NVS_NAMESPACE, cfg::NVS_KEY_PASS, password.c_str());
  if (!okSsid || !okPass) {
    // 避免留下一半新一半旧的凭据；内存状态也同步清空，保持与 NVS 一致
    core::config.remove(cfg::NVS_NAMESPACE, cfg::NVS_KEY_SSID);
    core::config.remove(cfg::NVS_NAMESPACE, cfg::NVS_KEY_PASS);
    gSsid = "";
    gPass = "";
    gHasCreds = false;
    logger::line("[ERROR] Failed to save WiFi credentials to NVS.");
    return false;
  }

  gSsid = ssid;
  gPass = password;
  gHasCreds = true;
  return true;
}

void clearCredentials() {
  core::config.remove(cfg::NVS_NAMESPACE, cfg::NVS_KEY_SSID);
  core::config.remove(cfg::NVS_NAMESPACE, cfg::NVS_KEY_PASS);

  gSsid = "";
  gPass = "";
  gHasCreds = false;
  gWasConnected = false;
  gRetryCount = 0;
  gGaveUp = false;
}

// 发起一次连接。
// 必须先 disconnect() 取消上一个仍在进行的请求：否则 ESP-IDF 会报
// "sta is connecting, cannot set config"，新参数不生效。
void startConnect(bool announce = true, bool isRetry = false) {
  if (isRetry && gRetryCount < 255) {
    gRetryCount++;
  }
  if (isRetry) {
    logger::line("[WiFi] Retry %u/%u: connecting to \"%s\" ...", gRetryCount,
                 cfg::WIFI_MAX_RETRIES, gSsid.c_str());
  } else if (announce) {
    logger::line("[WiFi] Connecting to \"%s\" ...", gSsid.c_str());
  } else {
    logger::line("[WiFi] Resuming connection attempt...");
  }
  Serial.flush();               // 提示先发完再切射频，避免波特率抖动打乱输出

  WiFi.disconnect(true, false);
  delay(100);
  WiFi.begin(gSsid.c_str(), gPass.c_str());
  delay(60);                    // 等时钟切换稳定，后续输出才不会串码
  gLastAttempt = millis();
}

// ── 实现 net::Provider，供其它模块查询状态 ──
class WifiStatusProvider : public net::Provider {
 public:
  net::Status status() const override {
    net::Status st;
    st.connected = (WiFi.status() == WL_CONNECTED);
    if (st.connected) {
      st.ssid = WiFi.SSID();
      st.ip   = WiFi.localIP();
      st.rssi = WiFi.RSSI();
    } else if (gHasCreds) {
      st.ssid = gSsid;
    }
    return st;
  }
};

WifiStatusProvider gProvider;

// ── 命令处理 ──
void printStatusCommand(const char* /*args*/) {
  logger::blank();
  const wl_status_t st = WiFi.status();

  if (st == WL_CONNECTED) {
    logger::line("[WiFi] CONNECTED");
    logger::line("SSID: %s", WiFi.SSID().c_str());
    logger::line("IP: %s", WiFi.localIP().toString().c_str());
    logger::line("RSSI: %d dBm", WiFi.RSSI());
    logger::line("MAC: %s", WiFi.macAddress().c_str());
  } else {
    logger::line("[WiFi] DISCONNECTED");
    logger::line("Status Code: %d (%s)", static_cast<int>(st), statusText(st));
    logger::line("Configured SSID: %s", gHasCreds ? gSsid.c_str() : "(none)");
    if (gGaveUp) {
      logger::line("Auto-retry:     stopped (failed after %u retries)",
                   cfg::WIFI_MAX_RETRIES);
    } else if (gHasCreds) {
      logger::line("Retries:        %u/%u", gRetryCount, cfg::WIFI_MAX_RETRIES);
    }
  }
  logger::blank();
}

void scanCommand(const char* /*args*/) {
  logger::blank();
  logger::line("[WiFi] Scanning...");
  Serial.flush();               // 同上：先发完再启动射频
  delay(10);

  // STA 正在连接时调用 scanNetworks() 会直接返回 -2。
  // 凭据失效或热点不在范围时重连会一直占着射频，导致扫描永远失败，
  // 所以这里先取消本次尝试，扫完再恢复。
  const bool wasConnecting = gHasCreds && (WiFi.status() != WL_CONNECTED);
  if (wasConnecting) {
    WiFi.disconnect(false, false);
    delay(100);
  }

  const int count = WiFi.scanNetworks();
  if (count < 0) {
    logger::line("[WiFi] Scan failed, code: %d", count);
  } else if (count == 0) {
    logger::line("[WiFi] No networks found.");
  } else {
    logger::line("[WiFi] Found %d networks:", count);
    for (int i = 0; i < count; i++) {
      logger::line("%2d. SSID: %-24s RSSI: %4d dBm Channel: %2d Encryption: %d",
                i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i),
                static_cast<int>(WiFi.encryptionType(i)));
    }
  }

  WiFi.scanDelete();
  logger::blank();

  if (wasConnecting && !gGaveUp) {
    startConnect(false);
  }
}

// 从 args 里取第一个参数，支持双引号包裹带空格的 SSID。
// 返回参数个数；argv 指向 args 内部（会写入 '\0'）。
int parseArgs(char* args, char** argv, int maxArg) {
  int argc = 0;
  while (*args && argc < maxArg) {
    while (*args == ' ' || *args == '\t') {
      args++;
    }
    if (!*args) {
      break;
    }
    if (*args == '"') {
      args++;
      argv[argc++] = args;
      while (*args && *args != '"') {
        args++;
      }
      if (*args) {
        *args++ = '\0';
      }
    } else {
      argv[argc++] = args;
      while (*args && *args != ' ' && *args != '\t') {
        args++;
      }
      if (*args) {
        *args++ = '\0';
      }
    }
  }
  return argc;
}

void setCommand(const char* args) {
  char buf[160];
  strncpy(buf, args == nullptr ? "" : args, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';

  char* argv[4];
  const int argc = parseArgs(buf, argv, 4);
  if (argc < 1) {
    logger::line("[ERROR] Usage: wifi set <ssid> [password]");
    return;
  }

  const char* pass = (argc >= 2) ? argv[1] : "";
  if (!saveCredentials(argv[0], pass)) {
    return;
  }
  logger::line("[WiFi] Credentials saved to NVS. SSID=\"%s\" password=%s",
            gSsid.c_str(), gPass.length() ? "set" : "(empty/open)");
  gRetryCount = 0;
  gGaveUp = false;
  startConnect();
}

void forgetCommand(const char* /*args*/) {
  clearCredentials();
  // 只断开、不关 radio：否则 WiFi.status() 会变成 WL_NO_SHIELD
  WiFi.disconnect(false, false);
  WiFi.mode(WIFI_STA);
  logger::line("[WiFi] Credentials cleared.");
}

void connectCommand(const char* /*args*/) {
  if (!gHasCreds) {
    logger::line("[ERROR] No WiFi credentials. Use 'wifi set <ssid> <password>'.");
    return;
  }
  if (WiFi.status() == WL_CONNECTED) {
    logger::line("[WiFi] Already connected.");
    return;
  }
  gRetryCount = 0;
  gGaveUp = false;
  gLastAttempt = 0;
  startConnect();
}

}  // namespace

namespace modules {
namespace wifi {

void begin() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);      // 关闭省电：扫描/连接时串口更稳定
  WiFi.setAutoReconnect(false);  // 重连由本模块统一控制（上限/提示）
  WiFi.persistent(false);    // 不让 WiFi 库往 NVS 写副本，凭据由本模块管理

  loadCredentials();

  // 注册本模块的命令（console 不需要认识 Wi-Fi）
  command::add("wifi status", printStatusCommand, "Show WiFi status");
  command::add("wifi scan", scanCommand, "Scan nearby WiFi networks");
  command::add("wifi set", setCommand, "Save credentials: wifi set <ssid> [password]");
  command::add("wifi connect", connectCommand, "Retry saved WiFi credentials");
  command::add("wifi forget", forgetCommand, "Erase saved credentials");

  // 向 modules 层暴露网络状态接口
  net::setProvider(&gProvider);

  if (!gHasCreds) {
    logger::line("[WiFi] No credentials stored. Use 'wifi set <ssid> <password>'.");
  }
}

void start() {
  if (!gHasCreds) {
    return;
  }
  logger::line("[WiFi] Connection started.");
  logger::line("[WiFi] Using saved SSID \"%s\" (password %s)",
            gSsid.c_str(), gPass.length() ? "set" : "empty/open");
  startConnect();
}

void poll() {
  const bool connected = (WiFi.status() == WL_CONNECTED);

  if (connected != gWasConnected) {
    gWasConnected = connected;
    if (connected) {
      gRetryCount = 0;
      gGaveUp = false;
      logger::line("[WiFi] Connected. SSID=\"%s\" IP=%s RSSI=%d dBm",
                WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(), WiFi.RSSI());
    } else if (gHasCreds) {
      logger::line("[WiFi] Connection lost (%s).", statusText(WiFi.status()));
    }
  }

  if (!gHasCreds || connected) {
    return;
  }
  if (gGaveUp) {
    return;
  }

  if (gRetryCount >= cfg::WIFI_MAX_RETRIES) {
    if (millis() - gLastAttempt >= cfg::WIFI_RETRY_MS) {
      gGaveUp = true;
      logger::line("[WiFi] Connection failed after %u retries. Auto-retry stopped.",
                   cfg::WIFI_MAX_RETRIES);
      WiFi.disconnect(false, false);
    }
    return;
  }

  if (millis() - gLastAttempt >= cfg::WIFI_RETRY_MS) {
    startConnect(true, true);
  }
}

bool hasCredentials() {
  return gHasCreds;
}

const char* configuredSsid() {
  return gSsid.c_str();
}

}  // namespace wifi
}  // namespace modules
