#pragma once

#include <Arduino.h>
#include <time.h>

constexpr uint8_t kMaxNearbyAprsStations = 6;

struct NearbyAprsStation {
  String callsign;
  String comment;
  uint16_t distanceKm = 0;
  bool node = false;
};

struct NearbyAprsWeather {
  String callsign;
  uint16_t distanceKm = 0;
  time_t receivedAt = 0;
  bool hasTemperature = false;
  bool hasHumidity = false;
  bool hasPressure = false;
  bool hasWind = false;
  bool hasGust = false;
  bool hasRain = false;
  float temperatureC = 0;
  uint8_t humidity = 0;
  float pressureHpa = 0;
  uint16_t windDirection = 0;
  float windKph = 0;
  float gustKph = 0;
  float rain24hMm = 0;
};

// Stations plotted on the APRS map page. Larger than the six-row list because
// the map shows everything heard inside the radius, not just the closest few.
constexpr uint8_t kMaxAprsMapStations = 24;
// A station not heard for this long drops off the map.
constexpr uint32_t kAprsMapMaxAgeMs = 60UL * 60UL * 1000UL;

struct AprsMapStation {
  String callsign;
  float latitude = 0;
  float longitude = 0;
  uint16_t distanceKm = 0;
  // Bearing from the QTH, degrees true (0 = north).
  uint16_t bearingDeg = 0;
  // millis() of the last position packet, so ageing works before NTP sync.
  uint32_t heardMs = 0;
  char symbolTable = '/';
  char symbol = ' ';
  bool node = false;
  bool weather = false;
};

struct NearbyAprsData {
  String status = "Waiting";
  String weatherSource = "APRS-IS";
  String weatherApiStatus = "Not configured";
  NearbyAprsStation stations[kMaxNearbyAprsStations];
  uint8_t stationCount = 0;
  NearbyAprsWeather weather;
  // Every station with a position inside the radius (most recent kept when
  // full), unsorted. Feeds the APRS map page.
  AprsMapStation mapStations[kMaxAprsMapStations];
  uint8_t mapStationCount = 0;
  // Bumped whenever a map station is added, moved or expired, so the display
  // can tell cheaply whether the map needs redrawing.
  uint32_t mapRevision = 0;
};

void aprsNearbyBegin();
bool serviceAprsNearby(bool wifiConnected, bool pageActive, bool weatherPageActive);
const NearbyAprsData& getNearbyAprsData();