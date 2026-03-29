# Weather Dial 12 for Pebble Time Steel

Custom rebuild inspired by AWW1.

## Features

- Analog watchface for **Pebble Time Steel** (`basalt` target)
- 12 cute pixel-style weather icons around the dial for the next 12 hours
- Center shows **day of month**
- 10-segment battery ring around the center
- Battery ring switches to a separate low-battery color below 20%
- Current temperature near the bottom
- Settings for:
  - background color
  - hour hand color
  - minute hand color
  - center circle color
  - battery ring color
  - battery ring low color
  - temperature text color
  - tick mark color
  - weather icon accent color
  - Celsius/Fahrenheit
  - provider selection: **Open-Meteo** or **OpenWeatherMap**
  - OpenWeatherMap API key input box

## Notes

- The build currently targets **Basalt** only, which covers Pebble Time Steel.
- Default provider is **Open-Meteo** so it works even without an API key.
- If OpenWeatherMap is selected but no key is present, the code falls back to Open-Meteo.
- The weather icons are drawn in C as lightweight pixel-art primitives, so there are no external image assets yet.

## Expected project layout

- `appinfo.json`
- `wscript`
- `src/c/main.c`
- `src/pkjs/index.js`

## Build outline

Typical Pebble SDK flow:

1. Install Pebble SDK
2. In the project folder run dependency install for JS config support
3. Build the app
4. Install the resulting `.pbw`

## Likely next polish pass

- refine icon art so each icon is cuter and more detailed
- improve battery ring geometry into arc-like dashes
- add optional invert-on-night theme behavior
- add separate day/night icon accent colors
- smooth placement tuning for Time Steel screen margins
- optional weather refresh interval control

## Important caveat

This is a first implementation scaffold. It is designed to be a strong starting point, but it may need a compile/debug pass inside an actual Pebble SDK environment before final install.
