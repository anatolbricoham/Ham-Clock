# Dashboard display

Files: `src/dashboard_display.h`, `src/dashboard_display.cpp` (≈3200 lines), `include/User_Setup*.h`, `include/greyline_map.h`, `include/greyline_map_460x230.h`

This module owns the TFT, the touch controller, page navigation, the backlight and every page renderer. It is also the scheduler that calls all data modules (see [architecture](../architecture.md#main-loop)).

## Public API

```cpp
constexpr uint8_t kDashboardPageCount = 14;
const char* dashboardPageName(uint8_t pageIndex);   // "" past the last page
void displayBegin();                                // TFT init, sprites, module Begin()s, touch bus
void displayUpdate(const ClockSnapshot& snapshot);  // one frame: touch, data refresh, render
void applyDisplaySettings();                        // MADCTL, invert, backlight; forces full redraw
uint8_t getCurrentDashboardPageNumber();            // 1-based
uint8_t getAppliedBrightnessPercent();              // actual PWM level (mid-fade aware)
void displayShowMessage(const String& title, const String& subtitle);
void displayShowWifiSearching(uint8_t spinnerFrame);
void requestDisplayRedraw();                        // clear field caches + mark page dirty
```

## Pages

`enum DashboardPage` defines the order. `static_assert`s keep `kPageCount`, `kDashboardPageCount` and `kAutoPageMaskAll` (one bit per page) in step – adding a page requires updating all three.

| Enum | Renderer | Content |
| --- | --- | --- |
| `kPageClock` | `drawClockPage` | Large UTC (font 7), local time (12/24 h), date, callsign + locator, IP, uptime. `swapUtcLocal` swaps the big and small readouts. |
| `kPagePropagation` | `drawPropagationPage` | SFI/X-ray, A/K, sunspots, SW/Bz, geomag, noise, aurora; HF band groups day/night coloured Good/Fair/Poor. |
| `kPageVhf` | `drawVhfPage` | Same top rows; VHF aurora (+ latitude), E-skip 6 m/4 m/2 m Europe, 2 m North America. |
| `kPageGreyline` | `drawGreylinePage` | World map, night shading, terminator, QTH and sun markers; QTH, sun position, sunrise/sunset (UTC + local), daylight/greyline status. |
| `kPagePsk` | `drawPskPage` | Same map with one marker per grid, coloured by band; counts, furthest report, band legend. |
| `kPageIss` | `drawIssPage` | Same map with ISS marker and ±45 min ground track; lat/lon, altitude, az/el, next passes. Hidden unless `issTrackerActive()`. |
| `kPageDx` | `drawDxPage` | Freq / Call / Mode / UTC rows (8 or 12), updated time, source provider, status. |
| `kPagePota` | `drawPotaPage` | Freq / Call / Mode / Park rows, same scroll machinery as DX. |
| `kPageOpenWebRx` | `drawOpenWebRxPage` | Server status, receiver name, location, version, clients/max, SDR count, chat status, last 4 chat lines. |
| `kPageDmr` | `drawDmrPage` | Hotspot status, last callsign (large), country, last-heard time, source, slot, talkgroup, duration, BER / packet loss. |
| `kPageWorldClock` | `drawWorldClockPage` | UTC date/time and ten cities in two columns (Anchorage … Sydney). |
| `kPageAprsWeather` | `drawAprsWeatherPage` | APRS-IS status, nearest weather station, distance/age/source, temperature, humidity, pressure, wind, gust, rain 24 h. |
| `kPageAprsStations` | `drawAprsStationsPage` | APRS-IS status + radius, up to six stations sorted by distance, nodes highlighted. |
| `kPageAprsMap` | `drawAprsMapPage` | Live range-ring map centred on the QTH with every station heard inside the radius, plus a nearest-first list with distance and compass direction. See [APRS map page](#aprs-map-page). |

Every page ends with `drawFooter()`: Wi-Fi state, NTP state, page indicator `n/14` and (except on the Clock page) UTC or local time.

### Drawing helpers

- `drawCentered`, `drawLeft`, `drawCenteredAt` – stateless text drawing.
- `drawCenteredField`, `drawLeftField`, `drawSplitField`, `drawPropStatusField`, `drawDmrDetail` – take a `String& last` cache and redraw only when the text changed.
- `clearPageState()` – empties every `g_last…` cache (called on page change and by `requestDisplayRedraw()`).
- `g_pageDirty` – when true the renderer paints the page background and static labels.

### World maps

- The map bitmap is `kGreylineMapRgb565[]` in PROGMEM: 300×150 on 320-wide panels, 460×230 on the 4.0" panel (`#if defined(ST7796_DRIVER)`).
- `drawGreylineMap`, `drawPskMap`, `drawIssMap` compose the map band by band (`MAP_BAND_ROWS`, default and per-board) into `mapSprite`, adding night shading (`drawNightShading`, darkens pixels on the night side), the terminator line, markers and overlays, then push each band.
- A *signature* string per map page decides when the map needs recomposing (e.g. QTH + sun position for Greyline).
- `latLonToMapXY()` is an equirectangular projection.

### DX/POTA scrolling

`updateDxRows()` / `updatePotaRows()` convert spots to `DxRowText` rows. When the list changes by a push at the top (`dxRowShiftCount`), a queue animates each new row sliding in (`startDxQueue`, `serviceDxQueue`, `stepDxScroll`) through `dxScrollSprite` (3 px per 10 ms frame, 200 ms between queued rows). If the sprite cannot be allocated, rows are applied instantly. Auto page change waits up to 3 s for a running scroll to finish.

### APRS map page

`drawAprsMapPage()` splits the screen into a square plot on the left and a list column on the right; all geometry derives from `DISPLAY_W`/`DISPLAY_H` (`kAprsMap*`, `kAprsList*`), so it lays out on both 320×240 and 480×320.

- **Plot** (`drawAprsMapPlot`) – QTH at the centre (white ring), north up, three range rings labelled in km, cross-hair axes. Each station from `NearbyAprsData::mapStations` is projected with a local flat-earth projection (`aprsMapProject`) and drawn as a cyan square (weather, symbol `_`), yellow triangle (node) or green dot (other), with its callsign. Stations not heard for 30 min turn grey; after 60 min they are dropped by the APRS module. Drawing is clipped to the square with `setViewport`, and the most recently heard stations are drawn last (on top).
- **Redraw policy** – the plot is repainted when the page opens, when the zoom or radius changes, when `mapRevision` changes (at most every 2 s, `kAprsMapMinRedrawMs`), and once a minute so ageing colours update.
- **List** – stations inside the shown radius, nearest first: callsign and `distance + compass point` (plus age on the 4.0" panel), in the same colours; a summary line (`n stn  r <km> km  x2`) and a legend.
- **Zoom** – `g_aprsMapZoom` 0/1/2 shows the full configured radius, half or a quarter; a centre tap cycles it.

## Navigation

- `nextPage()` / `previousPage()` wrap around and skip inactive pages (`pageIsActive()` – only the ISS page can be inactive).
- `serviceAutoPageChange()` – when `autoPageChange` is on and the mask is non-zero, moves to the next page whose bit is set in `autoPageMask` every `autoPageSeconds`. Any tap restarts the dwell.
- `advanceToNextIncludedPage(uint16_t mask)` – stops short of a full lap so a single-page mask does not repaint needlessly.

## Touch input

- XPT2046 read over SPI at 2.5 MHz, gated by `TOUCH_IRQ` (LOW = touched), 4 samples averaged per axis.
- On boards where the touch shares the display SPI pins (4.0"), the code uses `TFT_eSPI::getSPIinstance()` instead of starting a second bus (`TOUCH_SHARES_DISPLAY_BUS`).
- Calibration macros (per setup header, with 2.8" defaults): `TOUCH_RAW_MIN` 120, `TOUCH_RAW_MAX` 3975, `TOUCH_SWAP_XY` 1, `TOUCH_INVERT_X` 0, `TOUCH_INVERT_Y` 1. The 4.0" header uses 200/3980 and inverts both axes.
- The point is then mirrored to match `flip180` and `mirror`, because the digitiser does not know about MADCTL changes.
- Debounce 300 ms; one action per press.

`handleTouch()` actions:

| Where | Page | Action |
| --- | --- | --- |
| Centre third (x) | HF / VHF | `requestPropagationRefresh()`, status shows "Refreshing" |
| Centre third | DX | `requestDxSpotsRefresh()` |
| Centre third | POTA | `requestPotaSpotsRefresh()` |
| Centre third | PSKReporter | `requestPskReporterRefresh()` (runs when the 5-min floor allows) |
| Centre third | ISS | `requestIssTrackerRefresh()` |
| Centre third | APRS Map | Cycle zoom: full radius → ½ → ¼ |
| Left half (anything else) | any | previous page (next page if `swapTouchNav`) |
| Right half | any | next page (previous if `swapTouchNav`) |

> The project README says "tap left: next page"; the code does the opposite by default (left = previous). Use *Swap left/right page navigation* if your unit behaves differently.

## Panel orientation and colour (`applyDisplaySettings`)

TFT_eSPI fixes orientation at compile time; this function writes the MADCTL register (0x36) directly so it can be changed from the web page:

```
madctl = rotate90 ? MV : MX
if flip180: madctl ^= MX | MY
if mirror:  madctl ^= rotate90 ? MY : MX
if swapRedBlue: madctl |= BGR
```

then `tft.invertDisplay(invertColours)`. The same bit layout is valid for ILI9341, ST7789 and ST7796. The function also re-applies the backlight, restarts the DMR/OpenWebRX refresh timer (`dmrPanelBegin()`), and forces a full redraw.

## Backlight and night dimming

- `applyBacklight(percent)` drives `TFT_BL` with LEDC PWM (respecting `TFT_BACKLIGHT_ON`).
- `targetBrightnessPercent(epoch)` = day level when dimming is off; otherwise it interpolates between `brightnessPercent` and `nightBrightnessPercent` using `greylineNightFraction(epoch, nightFadeMinutes)` (0 = day, 1 = night, linear ramp centred on sunrise/sunset).
- `serviceNightDimming()` runs every loop and only touches PWM when the whole-percent value changes.

## Status screens

- `displayShowWifiSearching(frame)` – "Searching for Wi-Fi" with a `/ - \ |` spinner.
- `displayShowMessage(title, subtitle)` – full-screen message, used by the factory-reset countdown.

## Adding a page

1. Add an enum value before `kPageCount`.
2. Bump `kDashboardPageCount` (`dashboard_display.h`) and widen `kAutoPageMaskAll` (`settings.h`).
3. Write `drawXxxPage(const ClockSnapshot&)` using the field-cache helpers and end with `drawFooter()`.
4. Add it to `drawCurrentPage()` and `dashboardPageName()`.
5. Add any `g_last…` caches to `clearPageState()`.
6. If it needs data, call the module's refresh function from `displayUpdate()`.
7. Use `tft.width()` / `DISPLAY_W` for layout so the 4.0" panel is covered.
