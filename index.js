var Clay = require('pebble-clay');

var clayConfig = [
  {
    "type": "heading",
    "defaultValue": "Weather Dial 12"
  },
  {
    "type": "section",
    "items": [
      {
        "type": "select",
        "messageKey": "KEY_PROVIDER",
        "label": "Weather provider",
        "defaultValue": "openmeteo",
        "options": [
          { "label": "Open-Meteo", "value": "openmeteo" },
          { "label": "OpenWeatherMap", "value": "owm" }
        ]
      },
      {
        "type": "input",
        "messageKey": "KEY_OWM_API_KEY",
        "label": "OpenWeatherMap API key",
        "defaultValue": "",
        "attributes": { "placeholder": "Paste API key here" }
      },
      {
        "type": "select",
        "messageKey": "KEY_UNITS",
        "label": "Units",
        "defaultValue": "c",
        "options": [
          { "label": "Celsius", "value": "c" },
          { "label": "Fahrenheit", "value": "f" }
        ]
      }
    ]
  },
  {
    "type": "section",
    "items": [
      { "type": "color", "messageKey": "KEY_BG_COLOR", "label": "Background", "defaultValue": "0x000000" },
      { "type": "color", "messageKey": "KEY_HOUR_COLOR", "label": "Hour hand", "defaultValue": "0xFFFFFF" },
      { "type": "color", "messageKey": "KEY_MINUTE_COLOR", "label": "Minute hand", "defaultValue": "0x55AAFF" },
      { "type": "color", "messageKey": "KEY_CENTER_COLOR", "label": "Center circle", "defaultValue": "0x55AAFF" },
      { "type": "color", "messageKey": "KEY_BATTERY_COLOR", "label": "Battery ring", "defaultValue": "0x55AAFF" },
      { "type": "color", "messageKey": "KEY_BATTERY_LOW_COLOR", "label": "Battery ring below 20%", "defaultValue": "0xFF5555" },
      { "type": "color", "messageKey": "KEY_TEMP_COLOR", "label": "Temperature text", "defaultValue": "0xFFFFFF" },
      { "type": "color", "messageKey": "KEY_TICK_COLOR", "label": "Tick marks", "defaultValue": "0xFFFFFF" },
      { "type": "color", "messageKey": "KEY_ICON_ACCENT_COLOR", "label": "Weather icon accent", "defaultValue": "0xFFFFAA" }
    ]
  },
  {
    "type": "submit",
    "defaultValue": "Save"
  }
];

var clay = new Clay(clayConfig, null, { autoHandleEvents: false });

function isNight(hour) {
  return hour < 6 || hour >= 21;
}

function mapOpenMeteoCode(code, hour) {
  var night = isNight(hour);
  if (code === 0) return night ? 'clear_night' : 'clear_day';
  if (code === 1 || code === 2) return night ? 'partly_night' : 'partly_day';
  if (code === 3) return 'cloudy';
  if (code === 45 || code === 48) return 'fog';
  if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) return 'rain';
  if ((code >= 71 && code <= 77) || code === 85 || code === 86) return 'snow';
  if (code >= 95) return 'thunder';
  return 'cloudy';
}

function mapOWMCode(weatherId, icon) {
  if (weatherId >= 200 && weatherId < 300) return 'thunder';
  if (weatherId >= 300 && weatherId < 600) return 'rain';
  if (weatherId >= 600 && weatherId < 700) return 'snow';
  if (weatherId >= 700 && weatherId < 800) return 'fog';
  if (weatherId === 800) return icon && icon.indexOf('n') > -1 ? 'clear_night' : 'clear_day';
  if (weatherId === 801 || weatherId === 802) return icon && icon.indexOf('n') > -1 ? 'partly_night' : 'partly_day';
  if (weatherId >= 803) return 'cloudy';
  return 'cloudy';
}

function sendWeather(tempText, codes) {
  Pebble.sendAppMessage({
    KEY_TEMP: tempText,
    KEY_CODES: codes.join(',')
  }, function () {}, function () {});
}

function getSettings() {
  var cfg = clay.getSettings();
  return {
    provider: cfg.KEY_PROVIDER || 'openmeteo',
    units: cfg.KEY_UNITS || 'c',
    apiKey: cfg.KEY_OWM_API_KEY || ''
  };
}

function formatTemp(value, units) {
  return Math.round(value) + '°' + (units === 'f' ? 'F' : 'C');
}

function fetchOpenMeteo(lat, lon, units) {
  var tempUnit = units === 'f' ? 'fahrenheit' : 'celsius';
  var url = 'https://api.open-meteo.com/v1/forecast?latitude=' + lat + '&longitude=' + lon + '&current=' +
    'temperature_2m&hourly=temperature_2m,weathercode&forecast_days=2&temperature_unit=' + tempUnit + '&timezone=auto';

  var req = new XMLHttpRequest();
  req.onload = function () {
    var data = JSON.parse(this.responseText);
    var temp = data.current.temperature_2m;
    var nowIso = data.current.time;
    var idx = data.hourly.time.indexOf(nowIso);
    if (idx < 0) idx = 0;

    var codes = [];
    for (var i = 0; i < 12; i++) {
      var hidx = idx + i;
      var timeStr = data.hourly.time[hidx] || data.hourly.time[idx];
      var hour = parseInt(timeStr.slice(11, 13), 10);
      codes.push(mapOpenMeteoCode(data.hourly.weathercode[hidx], hour));
    }
    sendWeather(formatTemp(temp, units), codes);
  };
  req.open('GET', url);
  req.send();
}

function fetchOWM(lat, lon, units, apiKey) {
  if (!apiKey) {
    fetchOpenMeteo(lat, lon, units);
    return;
  }

  var owmUnits = units === 'f' ? 'imperial' : 'metric';
  var url = 'https://api.openweathermap.org/data/3.0/onecall?lat=' + lat + '&lon=' + lon + '&exclude=minutely,daily,alerts&units=' + owmUnits + '&appid=' + encodeURIComponent(apiKey);
  var req = new XMLHttpRequest();
  req.onload = function () {
    var data = JSON.parse(this.responseText);
    var codes = [];
    for (var i = 0; i < 12; i++) {
      var h = data.hourly[i];
      codes.push(mapOWMCode(h.weather[0].id, h.weather[0].icon));
    }
    sendWeather(formatTemp(data.current.temp, units), codes);
  };
  req.onerror = function () {
    fetchOpenMeteo(lat, lon, units);
  };
  req.open('GET', url);
  req.send();
}

function fetchWeather() {
  var settings = getSettings();
  navigator.geolocation.getCurrentPosition(function (pos) {
    var lat = pos.coords.latitude;
    var lon = pos.coords.longitude;
    if (settings.provider === 'owm') {
      fetchOWM(lat, lon, settings.units, settings.apiKey);
    } else {
      fetchOpenMeteo(lat, lon, settings.units);
    }
  }, function () {
    sendWeather('--°' + (settings.units === 'f' ? 'F' : 'C'), [
      'cloudy','cloudy','cloudy','cloudy','cloudy','cloudy',
      'cloudy','cloudy','cloudy','cloudy','cloudy','cloudy'
    ]);
  }, { timeout: 15000, maximumAge: 10 * 60 * 1000 });
}

Pebble.addEventListener('ready', function () {
  fetchWeather();
});

Pebble.addEventListener('showConfiguration', function () {
  Pebble.openURL(clay.generateUrl());
});

Pebble.addEventListener('webviewclosed', function (e) {
  if (!e || !e.response) return;
  var dict = clay.getSettings(e.response);
  Pebble.sendAppMessage(dict, function () {
    fetchWeather();
  }, function () {
    fetchWeather();
  });
});

Pebble.addEventListener('appmessage', function (e) {
  if (e.payload.KEY_REQUEST_WEATHER) {
    fetchWeather();
  }
});
