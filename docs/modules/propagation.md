# Propagation

Files: `src/propagation.h`, `src/propagation.cpp`

Supplies the HF Propagation and VHF Conditions pages.

## Public API

```cpp
void propagationBegin();                       // all fields "--", status "Waiting"
bool refreshPropagationIfNeeded(bool wifi);    // fetch when due or requested
void requestPropagationRefresh();
const PropagationData& getPropagationData();
```

`PropagationData` holds every value as a display-ready `String` (`"--"` when missing): `sfi`, `aIndex`, `kIndex`, `sunspots`, `xray`, `solarWind`, `bz`, `geomag`, `signalNoise`, `aurora`, `fof2`, `mufFactor`, the four HF band groups × day/night (`band8040Day` … `band1210Night`), per-band values (`band80m` … `band10m`), VHF phenomena (`vhfAurora`, `vhfAuroraLat`, `vhfEsEurope`, `vhfEsNorthAmerica`, `vhfEsEurope6m`, `vhfEsEurope4m`), `updatedUtc`, `status`, `hasData`.

## Sources

| Mode | URL | Parser |
| --- | --- | --- |
| Direct (default) | `https://www.hamqsl.com/solarxml.php` | `parseHamQslXml()` – simple tag extraction (`getXmlTagValue`, `getXmlBandValue`, `getXmlPhenomenonValue`) |
| JSON proxy | `propagationJsonUrl` (when *Propagation source mode* = JSON and the URL is non-empty) | `parsePropagationJson()` |

HTTP timeout: 5 s. Plain `http://` URLs are allowed for the proxy.

### JSON proxy format

A JSON object with top-level fields plus a flat `conditions` object. The parser is a lightweight string scanner, not a full JSON parser: keep `conditions` flat (no nested objects) and avoid duplicate key names. Recognised keys (first match wins where alternatives are listed):

| Field | Keys |
| --- | --- |
| Solar | `sfi`, `a_index`, `k_index`, `sunspots`, `xray`, `solar_wind` / `solar_wind_speed` / `sw`, `bz` / `bz_gsm`, `geomag`, `signal_noise`, `aurora`, `fof2`, `muf_factor` |
| HF groups (inside `conditions`) | `80m-40m_day`, `80m-40m_night`, `30m-20m_day`, `30m-20m_night`, `17m-15m_day`, `17m-15m_night`, `12m-10m_day`, `12m-10m_night` |
| Per band (inside `conditions`) | `80m`, `40m`, `30m`, `20m`, `17m`, `15m`, `12m`, `10m` (used as day values when a group is missing) |
| VHF | `vhf_aurora`, `aurora_lat` / `lat_degree`, `es_europe`, `es_north_america`, `es_europe_6m`, `es_europe_4m` |
| Meta | `updated` (ISO time), `ok` (`false` → treated as failure) |

Example:

```json
{
  "ok": true,
  "updated": "2026-09-28T14:00:00Z",
  "sfi": "152", "a_index": "8", "k_index": "2", "sunspots": "110",
  "xray": "B6.3", "solar_wind": "420", "bz": "-1.2",
  "geomag": "Quiet", "signal_noise": "S1-S2", "aurora": "2",
  "conditions": {
    "80m-40m_day": "Fair", "80m-40m_night": "Good",
    "30m-20m_day": "Good", "30m-20m_night": "Good",
    "17m-15m_day": "Good", "17m-15m_night": "Fair",
    "12m-10m_day": "Fair", "12m-10m_night": "Poor"
  },
  "vhf_aurora": "Band Closed", "es_europe": "Band Closed"
}
```

A response is accepted only if SFI, A and K are all present.

## Refresh behaviour

- Interval: `propagationRefreshMinutes` (1–120, default 15). First fetch immediately after boot.
- Without Wi-Fi the status becomes `WiFi offline` and a retry is scheduled 5 s later.
- On failure the status becomes `Fetch failed` or `Parse failed`; **the previous values are kept**.
- Centre-tap on HF or VHF, or saving settings, requests an immediate refresh.
