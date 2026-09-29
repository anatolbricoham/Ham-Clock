#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()

def rep(rel, old, new):
    f = ROOT / rel
    s = f.read_text()
    if old not in s:
        raise SystemExit(f"[ERROR] No encuentro el bloque esperado en {rel}. Usa la rama clean-main actual.")
    f.write_text(s.replace(old, new, 1))
    print("[OK]", rel)

src = Path(__file__).resolve().parent.parent / "src"
for n in ("dmr_panel.h", "dmr_panel.cpp"):
    (ROOT / "src" / n).write_text((src / n).read_text())
    print("[ADD] src/" + n)

rep("src/settings.h", "constexpr uint8_t kAutoPageMaskAll = 0xFF;", "constexpr uint16_t kAutoPageMaskAll = 0x01FF;")
rep("src/settings.h", "  String n2yoApiKey;\n  uint16_t propagationRefreshMinutes;", "  String n2yoApiKey;\n  String openWebRxUrl;\n  String dmrHotspotUrl;\n  uint16_t dmrRefreshSeconds;\n  uint16_t propagationRefreshMinutes;")
rep("src/settings.h", "  uint8_t autoPageMask;", "  uint16_t autoPageMask;")

rep("src/settings.cpp", "constexpr uint16_t kDefaultPotaRefreshMinutes = 5;", "constexpr uint16_t kDefaultPotaRefreshMinutes = 5;\nconstexpr uint16_t kDefaultDmrRefreshSeconds = 30;")
rep("src/settings.cpp", "  settings.n2yoApiKey = limitedString(settings.n2yoApiKey, 64);", "  settings.n2yoApiKey = limitedString(settings.n2yoApiKey, 64);\n  settings.openWebRxUrl = limitedString(settings.openWebRxUrl, 180);\n  settings.dmrHotspotUrl = limitedString(settings.dmrHotspotUrl, 180);\n  settings.dmrRefreshSeconds = constrain(settings.dmrRefreshSeconds, static_cast<uint16_t>(15), static_cast<uint16_t>(300));")
rep("src/settings.cpp", "  currentSettings.n2yoApiKey = preferences.getString(\"n2yokey\", \"\");\n  currentSettings.autoPageChange = preferences.getBool(\"autopage\", false);", "  currentSettings.n2yoApiKey = preferences.getString(\"n2yokey\", \"\");\n  currentSettings.openWebRxUrl = preferences.getString(\"owrxurl\", \"\");\n  currentSettings.dmrHotspotUrl = preferences.getString(\"dmrurl\", \"\");\n  currentSettings.dmrRefreshSeconds = preferences.getUShort(\"dmrsecs\", kDefaultDmrRefreshSeconds);\n  currentSettings.autoPageChange = preferences.getBool(\"autopage\", false);")
rep("src/settings.cpp", "  currentSettings.autoPageMask = preferences.getUChar(\"autopages\", kAutoPageMaskAll);", "  currentSettings.autoPageMask = preferences.getUShort(\"autopages\", kAutoPageMaskAll);")
rep("src/settings.cpp", "  preferences.putString(\"n2yokey\", currentSettings.n2yoApiKey);", "  preferences.putString(\"n2yokey\", currentSettings.n2yoApiKey);\n  preferences.putString(\"owrxurl\", currentSettings.openWebRxUrl);\n  preferences.putString(\"dmrurl\", currentSettings.dmrHotspotUrl);\n  preferences.putUShort(\"dmrsecs\", currentSettings.dmrRefreshSeconds);")
rep("src/settings.cpp", "  preferences.putUChar(\"autopages\", currentSettings.autoPageMask);", "  preferences.putUShort(\"autopages\", currentSettings.autoPageMask);")

rep("src/dashboard_display.h", "constexpr uint8_t kDashboardPageCount = 8;", "constexpr uint8_t kDashboardPageCount = 9;")
rep("src/dashboard_display.cpp", '#include "dx_spots.h"\n#include "greyline.h"', '#include "dx_spots.h"\n#include "dmr_panel.h"\n#include "greyline.h"')
rep("src/dashboard_display.cpp", "  kPageDx,\n  kPagePota,\n  kPageCount", "  kPageDx,\n  kPagePota,\n  kPageDmr,\n  kPageCount")
rep("src/dashboard_display.cpp", "void advanceToNextIncludedPage(uint8_t mask) {", "void advanceToNextIncludedPage(uint16_t mask) {")
renderer = r'''void drawDmrPage(const ClockSnapshot& snapshot) {
  const DmrPanelData& dmr = getDmrPanelData();
  if (g_pageDirty) {
    tft.fillScreen(kBg);
    drawCentered("DMR Monitor", 4, 4, kAccent);
    drawLeft("OpenWebRX", 8, 30, 2, kMuted);
    drawLeft("Hotspot calls", 8, 70, 2, kMuted);
  }
  String owrx = dmr.openWebRxStatus;
  if (dmr.openWebRxOnline) {
    if (dmr.openWebRxName.length()) owrx += "  " + dmr.openWebRxName;
    if (dmr.openWebRxVersion.length()) owrx += "  " + dmr.openWebRxVersion;
  }
  tft.fillRect(8, 46, tft.width() - 16, 18, kBg);
  drawLeft(owrx, 8, 46, 2, dmr.openWebRxOnline ? kAccent : kWarn);
  tft.fillRect(8, 88, tft.width() - 16, kFooterTop - 92, kBg);
  if (!dmr.callCount) {
    drawLeft("Status: " + dmr.hotspotStatus, 8, 88, 2, dmr.hotspotStatus.startsWith("Online") ? kAccent : kWarn);
  } else {
    const int16_t pitch = DISPLAY_H >= 300 ? 28 : 23;
    const uint8_t font = DISPLAY_H >= 300 ? 2 : 1;
    const uint8_t rows = min<uint8_t>(dmr.callCount, DISPLAY_H >= 300 ? kMaxDmrCalls : 5);
    for (uint8_t i = 0; i < rows; ++i) {
      const DmrCall& c = dmr.calls[i];
      String line = c.timeUtc + " " + c.callsign + " " + c.slot + " " + c.target;
      if (DISPLAY_H >= 300) {
        if (c.source.length()) line += " " + c.source;
        if (c.duration.length()) line += " " + c.duration + "s";
        if (c.ber.length()) line += " BER " + c.ber;
      }
      drawLeft(line, 8, 88 + i * pitch, font, i == 0 ? kAccent : kText);
    }
  }
  drawLeft("Updated " + dmr.updated + " UTC", 8, kFooterTop - 16, 1, kMuted);
  drawFooter(snapshot);
}

'''
rep("src/dashboard_display.cpp", "void drawCurrentPage(const ClockSnapshot& snapshot) {", renderer + "void drawCurrentPage(const ClockSnapshot& snapshot) {")
rep("src/dashboard_display.cpp", "    case kPagePota:\n      drawPotaPage(snapshot);\n      break;\n    default:", "    case kPagePota:\n      drawPotaPage(snapshot);\n      break;\n    case kPageDmr:\n      drawDmrPage(snapshot);\n      break;\n    default:")
rep("src/dashboard_display.cpp", "  potaSpotsBegin();\n  issTrackerBegin();", "  potaSpotsBegin();\n  issTrackerBegin();\n  dmrPanelBegin();")
rep("src/dashboard_display.cpp", "                            refreshPotaSpotsIfNeeded(snapshot.wifiConnected) |\n                            refreshIssTrackerIfNeeded(snapshot.wifiConnected, snapshot.epoch,", "                            refreshPotaSpotsIfNeeded(snapshot.wifiConnected) |\n                            refreshDmrPanelIfNeeded(snapshot.wifiConnected) |\n                            refreshIssTrackerIfNeeded(snapshot.wifiConnected, snapshot.epoch,")
rep("src/dashboard_display.cpp", '    case kPagePota: return "POTA Spots";\n    default: return "";', '    case kPagePota: return "POTA Spots";\n    case kPageDmr: return "DMR Monitor";\n    default: return "";')
rep("src/dashboard_display.cpp", "    } else if (g_currentPage == kPageIss &&\n               x >= tft.width() / 3 && x <= (tft.width() * 2) / 3) {\n      requestIssTrackerRefresh();\n    } else {", "    } else if (g_currentPage == kPageIss &&\n               x >= tft.width() / 3 && x <= (tft.width() * 2) / 3) {\n      requestIssTrackerRefresh();\n    } else if (g_currentPage == kPageDmr &&\n               x >= tft.width() / 3 && x <= (tft.width() * 2) / 3) {\n      requestDmrPanelRefresh();\n    } else {")

rep("src/setup_portal.cpp", '#include "dx_spots.h"', '#include "dx_spots.h"\n#include "dmr_panel.h"')
dmr_html = '''  html += F("</div><div class='card'><h2>DMR Monitor</h2>");
  html += F("<small>OpenWebRX status and recent DMR hotspot calls.</small>");
  html += F("<label for='owrxurl'>OpenWebRX base URL</label><input id='owrxurl' name='owrxurl' maxlength='180' placeholder='http://openwebrx.local:8073' value='");
  html += htmlEscape(settings.openWebRxUrl);
  html += F("'><small>Reads <code>/status.json</code>.</small>");
  html += F("<label for='dmrurl'>Pi-Star hotspot base URL</label><input id='dmrurl' name='dmrurl' maxlength='180' placeholder='http://pi-star.local' value='");
  html += htmlEscape(settings.dmrHotspotUrl);
  html += F("'><small>Reads <code>/api/last_heard.php</code> and filters DMR.</small>");
  html += F("<label for='dmrsecs'>Refresh seconds</label><input id='dmrsecs' name='dmrsecs' type='number' min='15' max='300' value='");
  html += String(settings.dmrRefreshSeconds);
  html += F("'>");
'''
rep("src/setup_portal.cpp", "  html += F(\"</div><div class='card'><h2>Automatic Page Change</h2>\");", dmr_html + "  html += F(\"</div><div class='card'><h2>Automatic Page Change</h2>\");")
rep("src/setup_portal.cpp", "  settings.n2yoApiKey = limitedArg(\"n2yokey\", 64);\n  settings.autoPageChange = server.hasArg(\"autopage\");", "  settings.n2yoApiKey = limitedArg(\"n2yokey\", 64);\n  settings.openWebRxUrl = limitedArg(\"owrxurl\", 180);\n  settings.dmrHotspotUrl = limitedArg(\"dmrurl\", 180);\n  settings.dmrRefreshSeconds = static_cast<uint16_t>(constrain(server.arg(\"dmrsecs\").toInt(), 15L, 300L));\n  settings.autoPageChange = server.hasArg(\"autopage\");")
rep("src/setup_portal.cpp", "  uint8_t autoPageMask = 0;", "  uint16_t autoPageMask = 0;")
rep("src/setup_portal.cpp", "      autoPageMask |= static_cast<uint8_t>(1u << i);", "      autoPageMask |= static_cast<uint16_t>(1u << i);")
rep("src/setup_portal.cpp", "  requestPotaSpotsRefresh();\n  bool wifiChanged = false;", "  requestPotaSpotsRefresh();\n  requestDmrPanelRefresh();\n  bool wifiChanged = false;")
print("DMR patch aplicado.")
