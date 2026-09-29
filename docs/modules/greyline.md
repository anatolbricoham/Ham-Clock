# Greyline and geo helpers

Files: `src/greyline.h`, `src/greyline.cpp`

Pure computation – no network. Provides the Greyline page data and geographic helpers used by almost every other module.

## Public API

```cpp
void greylineBegin();
void requestGreylineRefresh();
bool updateGreylineData(time_t epoch, bool timeValid);  // recompute once per minute
const GreylineData& getGreylineData();

String getConfiguredLocator();                          // settings locator (or default)
bool getConfiguredLatitude(double& lat);
bool getConfiguredLongitude(double& lon);
bool maidenheadToLatLon(const String& loc, double& lat, double& lon);  // centre of square
float greylineNightFraction(time_t epoch, uint16_t fadeMinutes);       // 0 day … 1 night
uint32_t greatCircleKm(double lat1, double lon1, double lat2, double lon2);
```

`GreylineData`: `valid`, `sunIsDown`, numeric QTH and subsolar lat/lon, and display strings `qth`, `latitude`, `longitude`, `utcTime`, `sunriseUtc`, `sunsetUtc`, `noonUtc`, `dayLength`, `sunLatitude`, `sunLongitude`, `status` (*Daylight*, *Darkness*, *Twilight* within ±45 min of a crossing, *Polar daylight*, *Polar darkness*, *Waiting for NTP*, *Location invalid*), `greyline` (*Morning greyline*, *Evening greyline*, *Not near greyline*).

## Algorithms

- **Maidenhead** – 4- or 6-character locators; returns the centre of the square/subsquare.
- **Sunrise/sunset** – NOAA "general solar position" approximation: fractional year γ, equation of time and solar declination, zenith 90.833° (includes refraction). Handles polar day/night (`kPolarDay` / `kPolarNight`).
- **Subsolar point** – declination for latitude; longitude from UTC and equation of time.
- **Greyline status** – "near greyline" within ±45 min (`kGreylineWindowMinutes`) of sunrise or sunset.
- **Night fraction** – linear ramp across `fadeMinutes` centred on each crossing; used by the backlight night dimming. It takes the live epoch so the fade is smooth (not minute-stepped).
- **Great-circle distance** – haversine, Earth radius 6371 km.

## Refresh

`updateGreylineData()` recomputes when the UTC minute changes (or at least every 60 s) and returns `true` so the map can be redrawn. Without NTP the status is *Waiting for NTP*; with an invalid locator, *Location invalid*. Saving settings calls `requestGreylineRefresh()`, so a new locator is applied immediately.
