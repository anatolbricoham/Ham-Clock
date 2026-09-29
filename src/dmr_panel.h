#pragma once
#include <Arduino.h>

constexpr uint8_t kMaxDmrCalls = 6;
constexpr uint8_t kMaxOpenWebRxChatMessages = 4;

struct OpenWebRxChatMessage {
  String name;
  String text;
};

struct DmrCall {
  String timeUtc, callsign, country, target, slot, source, duration, ber;
  bool packetLoss = false;
};

struct DmrPanelData {
  bool openWebRxOnline = false;
  String openWebRxName, openWebRxVersion;
  String openWebRxLocation;
  String openWebRxStatus = "Not configured";
  String openWebRxChatStatus = "Paused";
  uint8_t openWebRxMaxClients = 0;
  uint8_t openWebRxActiveClients = 0;
  uint8_t openWebRxSdrCount = 0;
  OpenWebRxChatMessage openWebRxChat[kMaxOpenWebRxChatMessages];
  uint8_t openWebRxChatCount = 0;
  String hotspotStatus = "Not configured";
  DmrCall calls[kMaxDmrCalls];
  uint8_t callCount = 0;
  String updated = "--";
};

void dmrPanelBegin();
bool refreshDmrPanelIfNeeded(bool wifiConnected);
void serviceOpenWebRxChat(bool wifiConnected, bool pageActive);
void requestDmrPanelRefresh();
const DmrPanelData& getDmrPanelData();
