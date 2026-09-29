#include "dmr_panel.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <WebSocketsClient.h>
#include <time.h>
#include "settings.h"

namespace {
constexpr uint32_t kMinRefreshMs = 15000UL;
DmrPanelData g_data;
WebSocketsClient g_openWebRxSocket;
String g_openWebRxSocketBase;
bool g_openWebRxSocketStarted = false;
uint32_t g_lastRefreshMs = 0;
bool g_refreshRequested = true;

String cleanBase(String s) {
  s.trim();
  while (s.endsWith("/")) s.remove(s.length() - 1);
  return s;
}
String nowUtc() {
  time_t now = time(nullptr);
  if (now < 1704067200) return String(millis() / 1000) + "s";
  tm utc; gmtime_r(&now, &utc);
  char b[16]; strftime(b, sizeof(b), "%H:%M:%S", &utc);
  return String(b);
}
template <typename Handler>
bool getResponse(const String& url, Handler handler, String& error) {
  if (!url.length()) { error = "Not configured"; return false; }
  HTTPClient http;
  http.setConnectTimeout(3500);
  http.setTimeout(5000);
  http.setUserAgent("CYD-Ham-DMR/1.0");
  bool begun = false;
  int code = -1;
  if (url.startsWith("https://")) {
    WiFiClientSecure client; client.setInsecure();
    begun = http.begin(client, url);
    if (begun) code = http.GET();
    if (code == HTTP_CODE_OK) {
      bool ok = handler(http.getStream());
      http.end();
      if (!ok) error = "Bad JSON";
      return ok;
    }
  } else {
    WiFiClient client;
    begun = http.begin(client, url);
    if (begun) code = http.GET();
    if (code == HTTP_CODE_OK) {
      bool ok = handler(http.getStream());
      http.end();
      if (!ok) error = "Bad JSON";
      return ok;
    }
  }
  if (begun) http.end();
  error = code > 0 ? "HTTP " + String(code) : "Connect failed";
  return false;
}
bool fetchOpenWebRx(const AppSettings& s) {
  String base = cleanBase(s.openWebRxUrl);
  if (!base.length()) {
    g_data.openWebRxOnline = false;
    g_data.openWebRxStatus = "Not configured";
    return false;
  }
  String error;
  bool ok = getResponse(base + "/status.json", [&](Stream& stream) {
    StaticJsonDocument<320> filter;
    filter["receiver"]["name"] = true;
    filter["receiver"]["location"] = true;
    filter["version"] = true;
    filter["max_clients"] = true;
    filter["sdrs"] = true;
    DynamicJsonDocument doc(4096);
    if (deserializeJson(doc, stream, DeserializationOption::Filter(filter))) return false;
    g_data.openWebRxName = String(doc["receiver"]["name"] | "OpenWebRX");
    g_data.openWebRxLocation = String(doc["receiver"]["location"] | "");
    g_data.openWebRxVersion = String(doc["version"] | "");
    g_data.openWebRxMaxClients = static_cast<uint8_t>(doc["max_clients"] | 0);
    g_data.openWebRxSdrCount = static_cast<uint8_t>(doc["sdrs"].size());
    return true;
  }, error);
  g_data.openWebRxOnline = ok;
  g_data.openWebRxStatus = ok ? "Online" : error;
  return ok;
}

void appendOpenWebRxChat(const String& name, const String& text) {
  if (!text.length()) return;
  if (g_data.openWebRxChatCount == kMaxOpenWebRxChatMessages) {
    for (uint8_t i = 1; i < kMaxOpenWebRxChatMessages; ++i) {
      g_data.openWebRxChat[i - 1] = g_data.openWebRxChat[i];
    }
    --g_data.openWebRxChatCount;
  }
  OpenWebRxChatMessage& message = g_data.openWebRxChat[g_data.openWebRxChatCount++];
  message.name = name.substring(0, 16);
  message.text = text.substring(0, 100);
}

void handleOpenWebRxText(uint8_t* payload, size_t length) {
  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, payload, length)) return;
  const char* type = doc["type"] | "";
  if (strcmp(type, "clients") == 0) {
    g_data.openWebRxActiveClients = static_cast<uint8_t>(doc["value"] | 0);
  } else if (strcmp(type, "receiver_details") == 0) {
    JsonObjectConst receiver = doc["value"].as<JsonObjectConst>();
    g_data.openWebRxName = String(receiver["receiver_name"] | g_data.openWebRxName.c_str());
    g_data.openWebRxLocation = String(receiver["receiver_location"] | g_data.openWebRxLocation.c_str());
  } else if (strcmp(type, "chat_message") == 0) {
    appendOpenWebRxChat(String(doc["name"] | ""), String(doc["text"] | ""));
  }
}

void openWebRxSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_CONNECTED) {
    g_data.openWebRxChatStatus = "Connected";
    g_openWebRxSocket.sendTXT("SERVER DE CLIENT client=openwebrx.js type=receiver");
  } else if (type == WStype_DISCONNECTED) {
    g_data.openWebRxChatStatus = "Disconnected";
  } else if (type == WStype_ERROR) {
    g_data.openWebRxChatStatus = "Socket error";
  } else if (type == WStype_TEXT && length > 0) {
    handleOpenWebRxText(payload, length);
  }
}

bool parseOpenWebRxUrl(const String& base, String& host, uint16_t& port,
                       String& path, bool& secure) {
  const int schemeEnd = base.indexOf("://");
  if (schemeEnd < 0) return false;
  const String scheme = base.substring(0, schemeEnd);
  secure = scheme == "https";
  if (!secure && scheme != "http") return false;

  const int authorityStart = schemeEnd + 3;
  const int pathStart = base.indexOf('/', authorityStart);
  const String authority = pathStart < 0 ? base.substring(authorityStart)
                                         : base.substring(authorityStart, pathStart);
  if (!authority.length()) return false;

  const int portSeparator = authority.lastIndexOf(':');
  if (portSeparator > 0) {
    host = authority.substring(0, portSeparator);
    port = static_cast<uint16_t>(authority.substring(portSeparator + 1).toInt());
  } else {
    host = authority;
    port = secure ? 443 : 80;
  }
  if (!host.length() || port == 0) return false;

  path = pathStart < 0 ? String("") : base.substring(pathStart);
  if (!path.endsWith("/")) path += "/";
  path += "ws/";
  return true;
}

void serviceOpenWebRxChatImpl(bool wifiConnected, bool pageActive) {
  if (!pageActive) {
    if (g_openWebRxSocketStarted) {
      g_openWebRxSocket.disconnect();
      g_openWebRxSocketStarted = false;
    }
    g_data.openWebRxChatStatus = "Paused";
    return;
  }
  if (!wifiConnected) {
    if (g_openWebRxSocketStarted) {
      g_openWebRxSocket.disconnect();
      g_openWebRxSocketStarted = false;
    }
    g_data.openWebRxChatStatus = "No Wi-Fi";
    return;
  }

  const String base = cleanBase(getSettings().openWebRxUrl);
  if (base != g_openWebRxSocketBase) {
    if (g_openWebRxSocketStarted) g_openWebRxSocket.disconnect();
    g_openWebRxSocketStarted = false;
    g_openWebRxSocketBase = base;
  }
  if (!base.length()) {
    g_data.openWebRxChatStatus = "Not configured";
    return;
  }

  if (!g_openWebRxSocketStarted) {
    String host;
    String path;
    uint16_t port;
    bool secure;
    if (!parseOpenWebRxUrl(base, host, port, path, secure)) {
      g_data.openWebRxChatStatus = "Invalid server URL";
      return;
    }
    g_openWebRxSocket.onEvent(openWebRxSocketEvent);
    g_openWebRxSocket.setReconnectInterval(10000);
    if (secure) {
      g_openWebRxSocket.beginSSL(host.c_str(), port, path.c_str(), "", "");
    } else {
      g_openWebRxSocket.begin(host.c_str(), port, path.c_str(), "");
    }
    g_openWebRxSocketStarted = true;
    g_data.openWebRxChatStatus = "Connecting";
  }
  g_openWebRxSocket.loop();
}

String slotFromMode(const String& mode) {
  if (mode.indexOf("Slot 1") >= 0 || mode.indexOf("TS1") >= 0) return "TS1";
  if (mode.indexOf("Slot 2") >= 0 || mode.indexOf("TS2") >= 0) return "TS2";
  return "";
}

String htmlCellText(const String& html) {
  String text;
  text.reserve(html.length());
  bool inTag = false;
  for (size_t i = 0; i < html.length(); ++i) {
    const char c = html[i];
    if (c == '<') {
      inTag = true;
    } else if (c == '>') {
      inTag = false;
    } else if (!inTag) {
      text += c;
    }
  }
  text.replace("&nbsp;", " ");
  text.replace("&amp;", "&");
  text.replace("&quot;", "\"");
  text.replace("&#39;", "'");
  text.replace("&lt;", "<");
  text.replace("&gt;", ">");
  text.trim();
  return text;
}

bool parseWpsdLastHeard(Stream& stream) {
  stream.setTimeout(250);
  const String html = stream.readString();
  if (html.indexOf("table-header-bar") < 0) return false;

  g_data.callCount = 0;
  int rowStart = html.indexOf("<tr");
  while (rowStart >= 0 && g_data.callCount < kMaxDmrCalls) {
    const int rowEnd = html.indexOf("</tr>", rowStart);
    if (rowEnd < 0) break;
    const String row = html.substring(rowStart, rowEnd);
    String cells[8];
    uint8_t cellCount = 0;
    int cellStart = row.indexOf("<td");
    while (cellStart >= 0 && cellCount < 8) {
      const int contentStart = row.indexOf('>', cellStart);
      const int cellEnd = row.indexOf("</td>", contentStart);
      if (contentStart < 0 || cellEnd < 0) break;
      cells[cellCount++] = htmlCellText(row.substring(contentStart + 1, cellEnd));
      cellStart = row.indexOf("<td", cellEnd + 5);
    }

    if (cellCount >= 6 && cells[3].startsWith("DMR") && cells[1].length() > 0) {
      DmrCall& call = g_data.calls[g_data.callCount++];
      call.timeUtc = cells[0];
      if (call.timeUtc.length() > 11) call.timeUtc = call.timeUtc.substring(0, 11);
      call.callsign = cells[1];
      call.country = cells[2];
      call.target = cells[4];
      if (call.target.startsWith("TG ")) call.target.remove(0, 3);
      call.slot = slotFromMode(cells[3]);
      call.source = cells[5];
      call.duration = cellCount > 6 ? cells[6] : "";
      call.ber = cellCount > 7 ? cells[7] : "";
      call.packetLoss = call.ber.endsWith("%");
    }
    rowStart = html.indexOf("<tr", rowEnd + 5);
  }
  return true;
}

bool fetchHotspotJson(const String& base, String& error) {
  return getResponse(base + "/api/last_heard.php?num_transmissions=12", [&](Stream& stream) {
    DynamicJsonDocument doc(12288);
    if (deserializeJson(doc, stream) || !doc.is<JsonArray>()) return false;
    g_data.callCount = 0;
    for (JsonObject row : doc.as<JsonArray>()) {
      if (g_data.callCount >= kMaxDmrCalls) break;
      String mode = String(row["mode"] | "");
      if (!mode.startsWith("DMR")) continue;
      DmrCall& c = g_data.calls[g_data.callCount++];
      c.timeUtc = String(row["time_utc"] | "");
      if (c.timeUtc.length() >= 19) c.timeUtc = c.timeUtc.substring(11, 19);
      c.callsign = String(row["callsign"] | "");
      c.country = String(row["country"] | "");
      c.target = String(row["target"] | "");
      c.slot = slotFromMode(mode);
      c.source = String(row["src"] | "");
      c.duration = String(row["duration"] | "");
      c.ber = String(row["bit_error_rate"] | "");
      c.packetLoss = false;
    }
    return true;
  }, error);
}

bool fetchHotspotWpsd(const String& base, String& error) {
  return getResponse(base + "/mmdvmhost/last_heard_table.php", parseWpsdLastHeard, error);
}

bool fetchHotspot(const AppSettings& s) {
  String base = cleanBase(s.dmrHotspotUrl);
  if (!base.length()) {
    g_data.hotspotStatus = "Not configured";
    g_data.callCount = 0;
    return false;
  }
  String error;
  bool ok = fetchHotspotJson(base, error);
  if (!ok) {
    String wpsdError;
    ok = fetchHotspotWpsd(base, wpsdError);
    if (!ok) error = wpsdError;
  }
  g_data.hotspotStatus = ok ? (g_data.callCount ? "Online" : "Online / no DMR") : error;
  if (!ok) g_data.callCount = 0;
  return ok;
}
}

void serviceOpenWebRxChat(bool wifiConnected, bool pageActive) {
  serviceOpenWebRxChatImpl(wifiConnected, pageActive);
}

void dmrPanelBegin() { g_refreshRequested = true; g_lastRefreshMs = 0; }

bool refreshDmrPanelIfNeeded(bool wifiConnected) {
  const AppSettings& s = getSettings();
  const uint32_t now = millis();
  const uint32_t refreshMs = max(
      static_cast<uint32_t>(kMinRefreshMs),
      static_cast<uint32_t>(static_cast<uint32_t>(s.dmrRefreshSeconds) * 1000U));
  if (!wifiConnected) {
    bool changed = g_data.openWebRxStatus != "No Wi-Fi" || g_data.hotspotStatus != "No Wi-Fi";
    g_data.openWebRxOnline = false;
    g_data.openWebRxStatus = "No Wi-Fi";
    g_data.hotspotStatus = "No Wi-Fi";
    g_data.callCount = 0;
    return changed;
  }
  if (!g_refreshRequested && g_lastRefreshMs && now - g_lastRefreshMs < refreshMs) return false;
  g_refreshRequested = false;
  g_lastRefreshMs = now;
  fetchOpenWebRx(s);
  fetchHotspot(s);
  g_data.updated = nowUtc();
  return true;
}
void requestDmrPanelRefresh() { g_refreshRequested = true; }
const DmrPanelData& getDmrPanelData() { return g_data; }
