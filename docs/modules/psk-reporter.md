# PSKReporter

Files: `src/psk_reporter.h`, `src/psk_reporter.cpp`

## Public API

```cpp
constexpr uint8_t kMaxPskReports = 48;   // map markers (unique 4-char grids)
void pskReporterBegin();
bool refreshPskReporterIfNeeded(bool wifi);
void requestPskReporterRefresh();
const PskReporterData& getPskReporterData();
uint8_t pskBandCount();
const char* pskBandLabel(uint8_t bandIndex);
uint16_t pskBandColor(uint8_t bandIndex);   // RGB565 marker colour
```

`PskReport`: lat/lon, `callsign`, `locator`, `bandIndex`, `distanceKm`, `flowStartSeconds`.
`PskReporterData`: `hasData`, `status`, `updated`, `callsign`, `reportCount`, `reports[]`, `totalReports`, `bandMask` (bit per band), `bestCallsign`, `bestLocator`, `bestDistanceKm`.

## Query

```
https://retrieve.pskreporter.info/query
  ?senderCallsign=<call>     (Who is hearing me – default)
  | receiverCallsign=<call>  (Who I am hearing)
  &flowStartSeconds=-<window seconds>
  &rronly=1&noactive=1&rptlimit=150
  [&appcontact=<e-mail>]
```

The XML response is streamed one `<receptionReport …/>` element at a time (≤ 512 bytes each, 6 s idle timeout), so memory use does not depend on how many reports come back.

For each report: the far-end locator is converted to lat/lon, distance from the QTH is computed, the band is found from the frequency, and the report is stored one per 4-character grid (the oldest is displaced when 48 grids are full). The furthest report across *all* returned reports is kept as "best DX".

## Bands and colours

160m, 80m, 60m, 40m, 30m, 20m, 17m, 15m, 12m, 10m, 6m, 4m, 2m, 70cm – each with a fixed RGB565 colour used for the markers and the legend (only bands present are listed).

## Rate limiting

PSKReporter asks for at most one query every five minutes. `kMinFetchIntervalMs` (5 min) is enforced before any refresh – including manual taps and saving settings, which are queued until the floor has elapsed. HTTP 503 is shown as `Rate limited`.

## Requirements

Needs a callsign (status `No callsign`, no requests) and a valid locator (`Locator invalid`).
