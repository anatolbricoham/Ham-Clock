# DMR hotspot and OpenWebRX

Files: `src/dmr_panel.h`, `src/dmr_panel.cpp` (uses `links2004/WebSockets`)

One module feeds two pages: **OpenWebRX** (page 9) and **DMR** (page 10).

## Public API

```cpp
constexpr uint8_t kMaxDmrCalls = 6;
constexpr uint8_t kMaxOpenWebRxChatMessages = 4;
void dmrPanelBegin();                                   // reset timer, force next refresh
bool refreshDmrPanelIfNeeded(bool wifi);                // HTTP polling (both services)
void serviceOpenWebRxChat(bool wifi, bool pageActive);  // WebSocket, only while page 9 is shown
void requestDmrPanelRefresh();
const DmrPanelData& getDmrPanelData();
```

`DmrPanelData`:

| Group | Fields |
| --- | --- |
| OpenWebRX | `openWebRxOnline`, `openWebRxStatus`, `openWebRxName`, `openWebRxLocation`, `openWebRxVersion`, `openWebRxMaxClients`, `openWebRxActiveClients`, `openWebRxSdrCount`, `openWebRxChatStatus`, `openWebRxChat[4]` (`name`, `text`), `openWebRxChatCount` |
| DMR | `hotspotStatus`, `calls[6]` (`timeUtc`, `callsign`, `country`, `target`, `slot` TS1/TS2, `source`, `duration`, `ber`, `packetLoss`), `callCount`, `updated` |

## HTTP polling

Every `dmrRefreshSeconds` (15–600, default 30; minimum 15 s) while Wi-Fi is up, **regardless of the page shown**, `refreshDmrPanelIfNeeded()` runs, in order:

1. **OpenWebRX status** – `GET <openWebRxUrl>/status.json`, filtered to `receiver.name`, `receiver.location`, `version`, `max_clients`, `sdrs` (count).
2. **DMR hotspot, JSON** – `GET <dmrHotspotUrl>/api/last_heard.php?num_transmissions=12` (Pi-Star style). Expects an array of objects with `mode` (must start with `DMR`), `time_utc`, `callsign`, `country`, `target`, `src`, `duration`, `bit_error_rate`.
3. **DMR hotspot, WPSD fallback** – if the JSON call fails: `GET <dmrHotspotUrl>/mmdvmhost/last_heard_table.php`, parsed as an HTML table (must contain `table-header-bar`). Columns: time, callsign, country, mode, target, source, duration, BER/loss. A value ending in `%` is shown as *Packet Loss*.

HTTP(S) with connect timeout 3.5 s and read timeout 5 s; HTTPS certificates are not checked. Enter base URLs **without** the API path, e.g. `http://pi-star.local` or `http://192.168.1.60:8073`.

Status texts: `Not configured`, `Online`, `Online / no DMR`, `HTTP <code>`, `Connect failed`, `Bad JSON`, `No Wi-Fi`.

> Because the three requests run synchronously in the main loop, an unreachable hotspot or receiver can freeze the display and touch for up to ~25 s every refresh. Leave unused URLs empty.

## OpenWebRX chat (WebSocket)

- Only active while the OpenWebRX page is on screen; otherwise the socket is closed and status is `Paused`.
- URL derived from the base URL: `ws://host:port/<path>/ws/` (`wss://` for HTTPS).
- On connect it sends `SERVER DE CLIENT client=openwebrx.js type=receiver` (the same handshake the web client uses).
- Handled text messages: `clients` (active client count), `receiver_details` (name/location), `chat_message` (name ≤16 chars, text ≤100 chars; last 4 kept). Other messages – including binary waterfall/audio frames – are ignored. Messages larger than 512 bytes are dropped.
- Reconnect interval 10 s. Status: `Connecting`, `Connected`, `Disconnected`, `Socket error`, `Invalid server URL`, `No Wi-Fi`, `Not configured`.
- Connecting as a receiver may count as one user on the OpenWebRX server while the page is shown.

## Legacy overlay script

`tools/apply_dmr_patch.py` and `README_DMR_ES.md` describe an earlier single-page "DMR Monitor" overlay for the upstream `clean-main` branch. The script no longer matches this code (different NVS key, refresh range and page count) and should not be run on this tree.
