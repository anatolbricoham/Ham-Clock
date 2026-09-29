#include "aprs_nearby.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <math.h>
#include <time.h>

#include "connectivity.h"
#include "greyline.h"
#include "settings.h"

namespace {
constexpr char kAprsIsHost[] = "rotate.aprs2.net";
constexpr uint16_t kAprsIsPort = 14580;
constexpr uint32_t kReconnectDelayMs = 30000;
constexpr uint32_t kLoginTimeoutMs = 10000;
constexpr uint32_t kAprsFiMinIntervalMs = 15UL * 60UL * 1000UL;
constexpr size_t kMaxAprsLineLength = 255;
constexpr size_t kMaxBytesPerLoop = 768;
constexpr double kEarthRadiusKm = 6371.0;

WiFiClient g_client;
NearbyAprsData g_data;
String g_line;
String g_activeCallsign;
String g_activeLocator;
uint16_t g_activeRadiusKm = 0;
uint32_t g_lastConnectAttemptMs = 0;
uint32_t g_connectedAtMs = 0;
bool g_logonSent = false;
bool g_lineOverflow = false;
bool g_feedActivated = false;
bool g_connectAttempted = false;
bool g_aprsFiAttempted = false;
uint32_t g_lastAprsFiAttemptMs = 0;
uint32_t g_lastMapExpiryMs = 0;
constexpr uint32_t kMapExpiryCheckMs = 30000;
String g_lastAprsFiKey;
String g_lastAprsFiCallsign;

double degreesToRadians(double degrees) {
  return degrees * 0.017453292519943295;
}

uint16_t distanceKm(double lat1, double lon1, double lat2, double lon2) {
    const double dLat = degreesToRadians(lat2 - lat1);
    const double dLon = degreesToRadians(lon2 - lon1);
  const double a = sin(dLat / 2.0) * sin(dLat / 2.0) +
      cos(degreesToRadians(lat1)) * cos(degreesToRadians(lat2)) *
        sin(dLon / 2.0) * sin(dLon / 2.0);
  const double clamped = a < 0.0 ? 0.0 : a > 1.0 ? 1.0 : a;
  return static_cast<uint16_t>(lround(kEarthRadiusKm * 2.0 * atan2(sqrt(clamped),
                                                                  sqrt(1.0 - clamped))));
}

bool isDigits(const String& text, size_t start, size_t count);

bool parseAprsCoordinate(const String& packet, uint8_t positionStart,
                         double& latitude, double& longitude,
                         uint8_t& commentStart, char& symbolTable, char& symbol) {
  if (positionStart + 19 <= packet.length() &&
      (packet.charAt(positionStart + 7) == 'N' ||
       packet.charAt(positionStart + 7) == 'S') &&
      isDigits(packet, positionStart, 4) && isDigits(packet, positionStart + 5, 2) &&
      isDigits(packet, positionStart + 9, 5) && isDigits(packet, positionStart + 15, 2)) {
    const String latDegreesText = packet.substring(positionStart, positionStart + 2);
    const String latMinutesText = packet.substring(positionStart + 2, positionStart + 7);
    const String lonDegreesText = packet.substring(positionStart + 9, positionStart + 12);
    const String lonMinutesText = packet.substring(positionStart + 12, positionStart + 17);
    const char latHemisphere = packet.charAt(positionStart + 7);
    const char lonHemisphere = packet.charAt(positionStart + 17);
    if (packet.charAt(positionStart + 8) != '/' && packet.charAt(positionStart + 8) != '\\') {
      return false;
    }
    if ((latHemisphere != 'N' && latHemisphere != 'S') ||
        (lonHemisphere != 'E' && lonHemisphere != 'W')) {
      return false;
    }
    latitude = latDegreesText.toInt() + latMinutesText.toFloat() / 60.0;
    longitude = lonDegreesText.toInt() + lonMinutesText.toFloat() / 60.0;
    if (latHemisphere == 'S') latitude = -latitude;
    if (lonHemisphere == 'W') longitude = -longitude;
    symbolTable = packet.charAt(positionStart + 8);
    symbol = packet.charAt(positionStart + 18);
    commentStart = positionStart + 19;
    return true;
  }

  if (positionStart + 10 >= packet.length()) return false;
  const char table = packet.charAt(positionStart);
  const char symbolCode = packet.charAt(positionStart + 9);
  if (table < '!' || table > '{' || symbolCode < '!' || symbolCode > '{') return false;
  uint32_t compressedLat = 0;
  uint32_t compressedLon = 0;
  for (uint8_t i = 1; i <= 4; ++i) {
    const char latDigit = packet.charAt(positionStart + i);
    const char lonDigit = packet.charAt(positionStart + i + 4);
    if (latDigit < '!' || latDigit > '{' || lonDigit < '!' || lonDigit > '{') return false;
    compressedLat = compressedLat * 91U + static_cast<uint8_t>(latDigit - '!');
    compressedLon = compressedLon * 91U + static_cast<uint8_t>(lonDigit - '!');
  }
  latitude = 90.0 - static_cast<double>(compressedLat) / 380926.0;
  longitude = -180.0 + static_cast<double>(compressedLon) / 190463.0;
  symbolTable = table;
  symbol = symbolCode;
  commentStart = positionStart + 13;
  return latitude >= -90.0 && latitude <= 90.0 && longitude >= -180.0 && longitude <= 180.0;
}

bool isDigits(const String& text, size_t start, size_t count) {
  if (start + count > text.length()) return false;
  for (size_t i = start; i < start + count; ++i) {
    if (text.charAt(i) < '0' || text.charAt(i) > '9') return false;
  }
  return true;
}

bool parseWeather(const String& comment, NearbyAprsWeather& weather) {
  bool hasWeatherValue = false;
  for (size_t i = 0; i + 4 < comment.length(); ++i) {
    if (comment.charAt(i) == 't') {
      size_t valueStart = i + 1;
      bool negative = false;
      if (comment.charAt(valueStart) == '-') {
        negative = true;
        ++valueStart;
      }
      const uint8_t digitCount = isDigits(comment, valueStart, 3) ? 3 :
                                 isDigits(comment, valueStart, 2) ? 2 : 0;
      if (digitCount > 0) {
        float fahrenheit = comment.substring(valueStart, valueStart + digitCount).toFloat();
        if (negative) fahrenheit = -fahrenheit;
        weather.temperatureC = (fahrenheit - 32.0f) * (5.0f / 9.0f);
        weather.hasTemperature = true;
        hasWeatherValue = true;
        break;
      }
    }
  }

  for (size_t i = 3; i + 4 < comment.length(); ++i) {
    if (comment.charAt(i) == '/' && isDigits(comment, i - 3, 3) &&
        isDigits(comment, i + 1, 3)) {
      weather.windDirection = static_cast<uint16_t>(comment.substring(i - 3, i).toInt());
      weather.windKph = comment.substring(i + 1, i + 4).toFloat() * 1.609344f;
      weather.hasWind = true;
      hasWeatherValue = true;
      break;
    }
  }

  for (size_t i = 0; i + 2 < comment.length(); ++i) {
    if (comment.charAt(i) == 'h' && isDigits(comment, i + 1, 2)) {
      weather.humidity = static_cast<uint8_t>(comment.substring(i + 1, i + 3).toInt());
      if (weather.humidity == 0) weather.humidity = 100;
      weather.hasHumidity = true;
      hasWeatherValue = true;
    } else if (comment.charAt(i) == 'g' && isDigits(comment, i + 1, 3)) {
      weather.gustKph = comment.substring(i + 1, i + 4).toFloat() * 1.609344f;
      weather.hasGust = true;
      hasWeatherValue = true;
    } else if (comment.charAt(i) == 'b' && isDigits(comment, i + 1, 5)) {
      weather.pressureHpa = comment.substring(i + 1, i + 6).toFloat() / 10.0f;
      weather.hasPressure = true;
      hasWeatherValue = true;
    } else if (comment.charAt(i) == 'p' && isDigits(comment, i + 1, 3)) {
      weather.rain24hMm = comment.substring(i + 1, i + 4).toFloat() * 0.254f;
      weather.hasRain = true;
      hasWeatherValue = true;
    }
  }
  return hasWeatherValue;
}

String urlEncode(const String& value) {
  static const char hex[] = "0123456789ABCDEF";
  String encoded;
  encoded.reserve(value.length() * 3);
  for (size_t i = 0; i < value.length(); ++i) {
    const uint8_t c = static_cast<uint8_t>(value.charAt(i));
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.') {
      encoded += static_cast<char>(c);
    } else {
      encoded += '%';
      encoded += hex[c >> 4];
      encoded += hex[c & 0x0F];
    }
  }
  return encoded;
}

String weatherField(JsonVariantConst field) {
  if (field.is<const char*>()) return String(field.as<const char*>());
  if (field.is<double>() || field.is<long>()) return String(field.as<double>(), 2);
  return "";
}

bool fetchAprsFiWeather() {
  const AppSettings& settings = getSettings();
  const String apiKey = settings.aprsFiApiKey;
  const String callsign = g_data.weather.callsign;
  if (!apiKey.length() || !callsign.length()) return false;

  if (apiKey != g_lastAprsFiKey || callsign != g_lastAprsFiCallsign) {
    g_lastAprsFiKey = apiKey;
    g_lastAprsFiCallsign = callsign;
    g_aprsFiAttempted = false;
  }
  if (g_aprsFiAttempted && millis() - g_lastAprsFiAttemptMs < kAprsFiMinIntervalMs) {
    return false;
  }
  g_aprsFiAttempted = true;
  g_lastAprsFiAttemptMs = millis();

  const String url = "https://api.aprs.fi/api/get?name=" + urlEncode(callsign) +
      "&what=wx&apikey=" + urlEncode(apiKey) + "&format=json";
  WiFiClientSecure secureClient;
  configureSecureClient(secureClient);
  HTTPClient http;
  http.setConnectTimeout(5000);
  http.setTimeout(5000);
  http.setUserAgent("CYD-HamDashboard/1.0 (+https://github.com/bricohams/esp32-cyd-ham-dashboard)");
  if (!http.begin(secureClient, url)) {
    g_data.weatherApiStatus = "Connect failed";
    return true;
  }

  const int statusCode = http.GET();
  if (statusCode != HTTP_CODE_OK) {
    http.end();
    g_data.weatherApiStatus = statusCode > 0
        ? "HTTP " + String(statusCode) : "Connect failed";
    return true;
  }

  StaticJsonDocument<512> filter;
  filter["result"] = true;
  filter["description"] = true;
  filter["entries"][0]["temp"] = true;
  filter["entries"][0]["humidity"] = true;
  filter["entries"][0]["pressure"] = true;
  filter["entries"][0]["wind_direction"] = true;
  filter["entries"][0]["wind_speed"] = true;
  filter["entries"][0]["wind_gust"] = true;
  filter["entries"][0]["rain_24h"] = true;

  DynamicJsonDocument doc(1536);
  const DeserializationError jsonError = deserializeJson(
      doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (jsonError || String(doc["result"] | "") != "ok" ||
      !doc["entries"].is<JsonArray>() || doc["entries"].as<JsonArray>().size() == 0) {
    g_data.weatherApiStatus = jsonError ? "Invalid APRS.fi response" :
        String(doc["description"] | "No weather data");
    return true;
  }

  JsonObjectConst entry = doc["entries"][0].as<JsonObjectConst>();
  NearbyAprsWeather weather;
  weather.callsign = callsign;
  weather.distanceKm = g_data.weather.distanceKm;
  weather.receivedAt = time(nullptr);
  String value = weatherField(entry["temp"]);
  weather.hasTemperature = value.length() > 0;
  weather.temperatureC = value.toFloat();
  value = weatherField(entry["humidity"]);
  weather.hasHumidity = value.length() > 0;
  weather.humidity = static_cast<uint8_t>(value.toInt());
  value = weatherField(entry["pressure"]);
  weather.hasPressure = value.length() > 0;
  weather.pressureHpa = value.toFloat();
  value = weatherField(entry["wind_direction"]);
  weather.hasWind = value.length() > 0 && weatherField(entry["wind_speed"]).length() > 0;
  weather.windDirection = static_cast<uint16_t>(value.toInt());
  weather.windKph = weatherField(entry["wind_speed"]).toFloat() * 3.6f;
  value = weatherField(entry["wind_gust"]);
  weather.hasGust = value.length() > 0;
  weather.gustKph = value.toFloat() * 3.6f;
  value = weatherField(entry["rain_24h"]);
  weather.hasRain = value.length() > 0;
  weather.rain24hMm = value.toFloat();
  g_data.weather = weather;
  g_data.weatherSource = "APRS.fi";
  g_data.weatherApiStatus = "OK";
  return true;
}

bool isNodeComment(const String& comment) {
  String upper = comment;
  upper.toUpperCase();
  return upper.indexOf("MMDVM") >= 0 || upper.indexOf("DIGI") >= 0 ||
         upper.indexOf("NODE") >= 0 || upper.indexOf("REPEATER") >= 0 ||
         upper.indexOf("DSTAR") >= 0 || upper.indexOf("DMR") >= 0 ||
         upper.indexOf("LORA") >= 0 ||
         upper.indexOf("ECHOLINK") >= 0;
}

void storeStation(const String& callsign, const String& comment, uint16_t distance) {
  for (uint8_t i = 0; i < g_data.stationCount; ++i) {
    if (g_data.stations[i].callsign == callsign) {
      g_data.stations[i].comment = comment;
      g_data.stations[i].distanceKm = distance;
      g_data.stations[i].node = isNodeComment(comment);
      while (i > 0 && g_data.stations[i].distanceKm < g_data.stations[i - 1].distanceKm) {
        NearbyAprsStation previous = g_data.stations[i - 1];
        g_data.stations[i - 1] = g_data.stations[i];
        g_data.stations[i] = previous;
        --i;
      }
      while (i + 1 < g_data.stationCount &&
             g_data.stations[i].distanceKm > g_data.stations[i + 1].distanceKm) {
        NearbyAprsStation next = g_data.stations[i + 1];
        g_data.stations[i + 1] = g_data.stations[i];
        g_data.stations[i] = next;
        ++i;
      }
      return;
    }
  }

  uint8_t insertAt = g_data.stationCount;
  if (insertAt == kMaxNearbyAprsStations) {
    if (distance >= g_data.stations[insertAt - 1].distanceKm) return;
    --insertAt;
  } else {
    ++g_data.stationCount;
  }
  while (insertAt > 0 && distance < g_data.stations[insertAt - 1].distanceKm) {
    g_data.stations[insertAt] = g_data.stations[insertAt - 1];
    --insertAt;
  }
  NearbyAprsStation& station = g_data.stations[insertAt];
  station.callsign = callsign;
  station.comment = comment;
  station.distanceKm = distance;
  station.node = isNodeComment(comment);
}

uint16_t bearingDeg(double lat1, double lon1, double lat2, double lon2) {
  const double phi1 = degreesToRadians(lat1);
  const double phi2 = degreesToRadians(lat2);
  const double dLon = degreesToRadians(lon2 - lon1);
  const double y = sin(dLon) * cos(phi2);
  const double x = cos(phi1) * sin(phi2) - sin(phi1) * cos(phi2) * cos(dLon);
  double bearing = atan2(y, x) * 57.29577951308232;
  if (bearing < 0.0) bearing += 360.0;
  return static_cast<uint16_t>(lround(bearing)) % 360;
}

// Keeps one entry per callsign with its latest position. When the table is
// full the station heard longest ago makes room, so the map always shows what
// is currently active rather than whatever arrived first.
void storeMapStation(const String& callsign, double qthLatitude, double qthLongitude,
                     double latitude, double longitude, uint16_t distance,
                     char symbolTable, char symbol, const String& comment) {
  uint8_t index = g_data.mapStationCount;
  for (uint8_t i = 0; i < g_data.mapStationCount; ++i) {
    if (g_data.mapStations[i].callsign == callsign) {
      index = i;
      break;
    }
  }
  if (index == g_data.mapStationCount) {
    if (g_data.mapStationCount < kMaxAprsMapStations) {
      ++g_data.mapStationCount;
    } else {
      index = 0;
      for (uint8_t i = 1; i < g_data.mapStationCount; ++i) {
        if (g_data.mapStations[i].heardMs < g_data.mapStations[index].heardMs) {
          index = i;
        }
      }
    }
  }

  AprsMapStation& station = g_data.mapStations[index];
  station.callsign = callsign;
  station.latitude = static_cast<float>(latitude);
  station.longitude = static_cast<float>(longitude);
  station.distanceKm = distance;
  station.bearingDeg = bearingDeg(qthLatitude, qthLongitude, latitude, longitude);
  station.heardMs = millis();
  station.symbolTable = symbolTable;
  station.symbol = symbol;
  station.weather = symbol == '_';
  station.node = !station.weather && isNodeComment(comment);
  ++g_data.mapRevision;
}

// Drops stations that have gone quiet. Returns true when any were removed.
bool expireMapStations() {
  const uint32_t nowMs = millis();
  bool removed = false;
  uint8_t kept = 0;
  for (uint8_t i = 0; i < g_data.mapStationCount; ++i) {
    if (nowMs - g_data.mapStations[i].heardMs >= kAprsMapMaxAgeMs) {
      removed = true;
      continue;
    }
    if (kept != i) g_data.mapStations[kept] = g_data.mapStations[i];
    ++kept;
  }
  for (uint8_t i = kept; i < g_data.mapStationCount; ++i) {
    g_data.mapStations[i] = AprsMapStation();
  }
  g_data.mapStationCount = kept;
  if (removed) ++g_data.mapRevision;
  return removed;
}

void clearMapStations() {
  for (uint8_t i = 0; i < g_data.mapStationCount; ++i) {
    g_data.mapStations[i] = AprsMapStation();
  }
  g_data.mapStationCount = 0;
  ++g_data.mapRevision;
}

bool parsePacket(String line) {
  const int separator = line.indexOf('>');
  const int colon = line.indexOf(':', separator + 1);
  if (separator < 1 || colon < 0) return false;
  const String callsign = line.substring(0, separator);
  const String header = line.substring(separator + 1, colon);
  String payload = line.substring(colon + 1);
  if (!payload.length()) return false;

  uint8_t positionStart = 0;
  const char packetType = payload.charAt(0);
  if (packetType == '!' || packetType == '=') {
    positionStart = 1;
  } else if (packetType == '@' || packetType == '/') {
    positionStart = 8;
  } else {
    return false;
  }

  double latitude;
  double longitude;
  uint8_t commentStart;
  char symbolTable;
  char symbol;
  if (!parseAprsCoordinate(payload, positionStart, latitude, longitude,
                           commentStart, symbolTable, symbol)) {
    return false;
  }

  double qthLatitude;
  double qthLongitude;
  if (!getConfiguredLatitude(qthLatitude) || !getConfiguredLongitude(qthLongitude)) {
    return false;
  }
  const uint16_t distance = distanceKm(qthLatitude, qthLongitude, latitude, longitude);
  if (distance > getSettings().aprsRadiusKm) return false;

  const String comment = payload.substring(min(static_cast<size_t>(commentStart), payload.length()));
  storeStation(callsign, comment, distance);
  storeMapStation(callsign, qthLatitude, qthLongitude, latitude, longitude, distance,
                  symbolTable, symbol, comment);

  NearbyAprsWeather weather;
  if (symbol == '_' && parseWeather(comment, weather) &&
      (!g_data.weather.callsign.length() || distance <= g_data.weather.distanceKm)) {
    weather.callsign = callsign;
    weather.distanceKm = distance;
    weather.receivedAt = time(nullptr);
    g_data.weather = weather;
    g_data.weatherSource = "APRS-IS";
  }
  (void)header;
  return true;
}

void sendLogin() {
  double latitude;
  double longitude;
  if (!getConfiguredLatitude(latitude) || !getConfiguredLongitude(longitude)) return;
  char filter[80];
  snprintf(filter, sizeof(filter), "filter r/%.4f/%.4f/%u",
           latitude, longitude, getSettings().aprsRadiusKm);
  String login = "user " + g_activeCallsign + " pass -1 vers CYD-HamDashboard 1.0 ";
  login += filter;
  g_client.print(login);
  g_client.print("\r\n");
  g_logonSent = true;
  g_connectedAtMs = millis();
  g_data.status = "Logging in";
}

bool processLine(String line) {
  line.trim();
  if (!line.length()) return false;
  if (line.startsWith("#")) {
    if (!g_logonSent && line.startsWith("# aprsc")) sendLogin();
    if (line.indexOf("logresp") >= 0) {
      g_data.status = line.indexOf("unverified") >= 0 || line.indexOf("verified") >= 0
                          ? "Receiving (read only)"
                          : "Login rejected";
      return true;
    }
    return false;
  }
  return parsePacket(line);
}
}

void aprsNearbyBegin() {
  g_client.stop();
  g_data = NearbyAprsData();
  g_line = "";
  g_logonSent = false;
  g_lineOverflow = false;
  g_lastConnectAttemptMs = 0;
  g_connectAttempted = false;
  g_aprsFiAttempted = false;
  g_lastAprsFiAttemptMs = 0;
  g_lastAprsFiKey = "";
  g_lastAprsFiCallsign = "";
  g_activeCallsign = "";
  g_activeLocator = "";
  g_activeRadiusKm = 0;
  g_feedActivated = false;
}

bool serviceAprsNearbyImpl(bool wifiConnected, bool pageActive, bool weatherPageActive) {
  if (pageActive) g_feedActivated = true;
  if (!g_feedActivated) {
    g_data.status = "Paused";
    return false;
  }
  if (!wifiConnected) {
    if (g_client.connected()) g_client.stop();
    g_logonSent = false;
    g_connectAttempted = false;
    g_data.status = "No Wi-Fi";
    return false;
  }

  const AppSettings& settings = getSettings();
  const String callsign = settings.callsign;
  const String locator = settings.locator;
  if (!callsign.length() || callsign == "NOCALL" || locator.length() < 4) {
    g_data.status = "Set callsign and locator";
    return false;
  }

  if (callsign != g_activeCallsign || locator != g_activeLocator ||
      settings.aprsRadiusKm != g_activeRadiusKm) {
    if (g_client.connected()) g_client.stop();
    g_activeCallsign = callsign;
    g_activeLocator = locator;
    g_activeRadiusKm = settings.aprsRadiusKm;
    g_logonSent = false;
    g_data.stationCount = 0;
    g_data.weather = NearbyAprsWeather();
    clearMapStations();
    g_data.status = "Filter changed";
    g_lastConnectAttemptMs = 0;
    g_connectAttempted = false;
  }

  if (!g_client.connected()) {
    if (g_connectAttempted && millis() - g_lastConnectAttemptMs < kReconnectDelayMs) return false;
    g_lastConnectAttemptMs = millis();
    g_connectAttempted = true;
    g_data.status = "Connecting to APRS-IS";
    g_client.stop();
    if (!g_client.connect(kAprsIsHost, kAprsIsPort)) {
      g_data.status = "APRS-IS unavailable";
      return true;
    }
    g_client.setNoDelay(true);
    g_client.setTimeout(10);
    g_logonSent = false;
    g_lineOverflow = false;
    g_line = "";
    g_connectedAtMs = millis();
    g_data.status = "Connected";
  }

  bool changed = false;
  size_t bytesRead = 0;
  while (g_client.available() && bytesRead < kMaxBytesPerLoop) {
    const int value = g_client.read();
    if (value < 0) break;
    ++bytesRead;
    const char character = static_cast<char>(value);
    if (character == '\n') {
      if (!g_lineOverflow) changed |= processLine(g_line);
      g_line = "";
      g_lineOverflow = false;
    } else if (character != '\r') {
      if (g_line.length() < kMaxAprsLineLength) {
        g_line += character;
      } else {
        g_lineOverflow = true;
      }
    }
  }

  if (!g_logonSent && millis() - g_connectedAtMs > kLoginTimeoutMs) {
    g_client.stop();
    g_data.status = "APRS-IS login timeout";
    return true;
  }
  if (!g_client.connected() && g_data.status == "Receiving (read only)") {
    g_data.status = "Feed disconnected";
    return true;
  }
  if (millis() - g_lastMapExpiryMs >= kMapExpiryCheckMs) {
    g_lastMapExpiryMs = millis();
    if (expireMapStations()) changed = true;
  }
  if (weatherPageActive && fetchAprsFiWeather()) changed = true;
  return changed;
}

bool serviceAprsNearby(bool wifiConnected, bool pageActive, bool weatherPageActive) {
  // Report a change only when something visible moved: new data, or a status
  // transition. Steady states (no Wi-Fi, missing callsign) would otherwise force
  // a full redraw on every loop.
  const String previousStatus = g_data.status;
  const bool changed = serviceAprsNearbyImpl(wifiConnected, pageActive, weatherPageActive);
  return changed || g_data.status != previousStatus;
}

const NearbyAprsData& getNearbyAprsData() {
  return g_data;
}
