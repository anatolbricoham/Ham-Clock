# Credits and acknowledgements

## BricoHams edition

This edition of the CYD Ham Dashboard is maintained by the **BricoHams** group. It brings together the original dashboard and the BricoHams additions (OpenWebRX status and chat, DMR hotspot "last heard", World Clock, APRS nearby stations and weather, and the documentation in [`docs/`](docs/README.md)) in one project.

Credits:

- **BricoHams** – integration, new pages, testing and documentation.
- **EA5JEF, Diego** – member of the BricoHams team; thanks for his contribution to this project.

## Original project

Many thanks to **HenrysCat** for creating the original **CYD Ham Dashboard**, on which everything here is built:

<https://github.com/HenrysCat/esp32-cyd-ham-dashboard>

That covers the dashboard architecture, the Clock, HF/VHF propagation, Greyline, PSKReporter, ISS tracker, DX and POTA pages, the captive portal and web settings, and the support for the 2.8" and 4.0" boards. If you find this project useful, please star and support the original repository too.

The original project is released under the **GNU General Public License v3.0**, and so is this edition. Any redistribution must keep this attribution and the licence (see [`LICENSE`](LICENSE)).

## Libraries

| Library | Author | Used for |
| --- | --- | --- |
| [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) | Bodmer | Display driver and fonts |
| [ArduinoJson](https://arduinojson.org/) | Benoît Blanchon | JSON parsing |
| [Sgp4](https://github.com/Hopperpop/Sgp4-Library) | Hopperpop | ISS orbit propagation |
| [arduinoWebSockets](https://github.com/Links2004/arduinoWebSockets) | Markus Sattler (Links2004) | OpenWebRX chat |
| [arduino-esp32](https://github.com/espressif/arduino-esp32) | Espressif | ESP32 Arduino core (Wi-Fi, HTTP, Preferences, WebServer, DNS, mDNS) |

## Data services

Thanks to the people and projects who run the free services the dashboard reads:

- **HamQSL** (Paul Herrman, N0NBH) – solar and band-condition data
- **IZ3MEZ DX Cluster** – DX spots JSON feed
- **dxspots.com** – DX Cluster Telnet node
- **Parks on the Air (POTA)** – activator spots
- **PSKReporter** (Philip Gladstone, N1DQ) – reception reports
- **CelesTrak** – ISS orbital elements (TLE)
- **N2YO** – ISS pass predictions
- **APRS-IS / aprs2.net** – APRS feed
- **APRS.fi** (Heikki Hannikainen, OH7LZB) – APRS weather data
- **OpenWebRX**, **Pi-Star** and **WPSD** – receiver and hotspot software whose interfaces the OpenWebRX and DMR pages read

73 de BricoHams
