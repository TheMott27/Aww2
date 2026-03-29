#include <pebble.h>

#define WEATHER_SLOTS 12
#define BATTERY_SEGMENTS 10

// Message keys must match appinfo.json
enum {
  KEY_TEMP = 0,
  KEY_CODES = 1,
  KEY_BG_COLOR = 2,
  KEY_HOUR_COLOR = 3,
  KEY_MINUTE_COLOR = 4,
  KEY_CENTER_COLOR = 5,
  KEY_BATTERY_COLOR = 6,
  KEY_BATTERY_LOW_COLOR = 7,
  KEY_TEMP_COLOR = 8,
  KEY_TICK_COLOR = 9,
  KEY_ICON_ACCENT_COLOR = 10,
  KEY_UNITS = 11,
  KEY_PROVIDER = 12,
  KEY_OWM_API_KEY = 13,
  KEY_REQUEST_WEATHER = 14
};

typedef enum {
  ICON_CLEAR_DAY,
  ICON_CLEAR_NIGHT,
  ICON_PARTLY_CLOUDY_DAY,
  ICON_PARTLY_CLOUDY_NIGHT,
  ICON_CLOUDY,
  ICON_RAIN,
  ICON_THUNDER,
  ICON_SNOW,
  ICON_FOG,
  ICON_UNKNOWN
} WeatherIcon;

typedef struct {
  GColor bg;
  GColor hour;
  GColor minute;
  GColor center;
  GColor battery;
  GColor battery_low;
  GColor temp;
  GColor tick;
  GColor accent;
} Theme;

static Window *s_window;
static Layer *s_canvas_layer;
static Theme s_theme;
static char s_temp_text[16] = "--°C";
static char s_codes_text[96] = "unknown,unknown,unknown,unknown,unknown,unknown,unknown,unknown,unknown,unknown,unknown,unknown";
static WeatherIcon s_icons[WEATHER_SLOTS];

static GPoint s_center;
static int s_radius = 68;
static int s_center_radius = 18;

static GColor color_from_uint(uint32_t value) {
  return GColorFromHEX(value);
}

static void load_defaults(void) {
  s_theme.bg = GColorBlack;
  s_theme.hour = GColorWhite;
  s_theme.minute = GColorPictonBlue;
  s_theme.center = GColorPictonBlue;
  s_theme.battery = GColorPictonBlue;
  s_theme.battery_low = GColorRed;
  s_theme.temp = GColorWhite;
  s_theme.tick = GColorWhite;
  s_theme.accent = GColorPastelYellow;

  for (int i = 0; i < WEATHER_SLOTS; i++) {
    s_icons[i] = ICON_UNKNOWN;
  }
}

static WeatherIcon icon_from_token(const char *token) {
  if (!token) return ICON_UNKNOWN;

  if (strcmp(token, "clear_day") == 0) return ICON_CLEAR_DAY;
  if (strcmp(token, "clear_night") == 0) return ICON_CLEAR_NIGHT;
  if (strcmp(token, "partly_day") == 0) return ICON_PARTLY_CLOUDY_DAY;
  if (strcmp(token, "partly_night") == 0) return ICON_PARTLY_CLOUDY_NIGHT;
  if (strcmp(token, "cloudy") == 0) return ICON_CLOUDY;
  if (strcmp(token, "rain") == 0) return ICON_RAIN;
  if (strcmp(token, "thunder") == 0) return ICON_THUNDER;
  if (strcmp(token, "snow") == 0) return ICON_SNOW;
  if (strcmp(token, "fog") == 0) return ICON_FOG;

  return ICON_UNKNOWN;
}

static void parse_codes(void) {
  char buffer[sizeof(s_codes_text)];
  snprintf(buffer, sizeof(buffer), "%s", s_codes_text);

  char *saveptr;
  char *token = strtok_r(buffer, ",", &saveptr);
  int i = 0;
  while (token && i < WEATHER_SLOTS) {
    s_icons[i++] = icon_from_token(token);
    token = strtok_r(NULL, ",", &saveptr);
  }
  while (i < WEATHER_SLOTS) {
    s_icons[i++] = ICON_UNKNOWN;
  }
}

static void fill_rect(GContext *ctx, int x, int y, int w, int h, GColor color) {
  graphics_context_set_fill_color(ctx, color);
  graphics_fill_rect(ctx, GRect(x, y, w, h), 0, GCornerNone);
}

static void draw_pixel_sun(GContext *ctx, GPoint p, bool partial) {
  GColor ray = GColorMelon;
  GColor fill = s_theme.accent;
  for (int i = -6; i <= 6; i += 3) {
    fill_rect(ctx, p.x + i, p.y - 8, 1, 3, ray);
    fill_rect(ctx, p.x + i, p.y + 6, 1, 3, ray);
    fill_rect(ctx, p.x - 8, p.y + i, 3, 1, ray);
    fill_rect(ctx, p.x + 6, p.y + i, 3, 1, ray);
  }
  fill_rect(ctx, p.x - 5, p.y - 5, 10, 10, ray);
  fill_rect(ctx, p.x - 4, p.y - 4, 8, 8, fill);

  if (partial) {
    GColor cloud = GColorWhite;
    fill_rect(ctx, p.x - 8, p.y + 2, 14, 5, cloud);
    fill_rect(ctx, p.x - 5, p.y, 5, 4, cloud);
    fill_rect(ctx, p.x - 1, p.y - 1, 5, 5, cloud);
  }
}

static void draw_pixel_moon(GContext *ctx, GPoint p) {
  fill_rect(ctx, p.x - 5, p.y - 5, 10, 10, GColorPictonBlue);
  fill_rect(ctx, p.x - 2, p.y - 5, 8, 10, s_theme.bg);
}

static void draw_pixel_cloud(GContext *ctx, GPoint p, GColor color) {
  fill_rect(ctx, p.x - 7, p.y + 1, 15, 6, color);
  fill_rect(ctx, p.x - 4, p.y - 2, 6, 5, color);
  fill_rect(ctx, p.x + 1, p.y - 3, 6, 6, color);
}

static void draw_pixel_rain(GContext *ctx, GPoint p) {
  draw_pixel_cloud(ctx, p, GColorWhite);
  GColor drop = GColorPictonBlue;
  fill_rect(ctx, p.x - 5, p.y + 8, 2, 4, drop);
  fill_rect(ctx, p.x, p.y + 9, 2, 4, drop);
  fill_rect(ctx, p.x + 5, p.y + 8, 2, 4, drop);
}

static void draw_pixel_thunder(GContext *ctx, GPoint p) {
  draw_pixel_cloud(ctx, p, GColorWhite);
  GColor bolt = GColorYellow;
  fill_rect(ctx, p.x, p.y + 6, 2, 4, bolt);
  fill_rect(ctx, p.x - 2, p.y + 9, 4, 2, bolt);
  fill_rect(ctx, p.x - 1, p.y + 11, 2, 4, bolt);
}

static void draw_pixel_snow(GContext *ctx, GPoint p) {
  draw_pixel_cloud(ctx, p, GColorWhite);
  GColor flake = GColorPastelYellow;
  fill_rect(ctx, p.x - 5, p.y + 8, 1, 5, flake);
  fill_rect(ctx, p.x - 7, p.y + 10, 5, 1, flake);
  fill_rect(ctx, p.x, p.y + 8, 1, 5, flake);
  fill_rect(ctx, p.x - 2, p.y + 10, 5, 1, flake);
}

static void draw_pixel_fog(GContext *ctx, GPoint p) {
  GColor fog = GColorLightGray;
  fill_rect(ctx, p.x - 7, p.y - 2, 14, 3, fog);
  fill_rect(ctx, p.x - 8, p.y + 3, 16, 3, fog);
  fill_rect(ctx, p.x - 6, p.y + 8, 12, 3, fog);
}

static void draw_icon(GContext *ctx, GPoint p, WeatherIcon icon) {
  switch (icon) {
    case ICON_CLEAR_DAY:
      draw_pixel_sun(ctx, p, false);
      break;
    case ICON_CLEAR_NIGHT:
      draw_pixel_moon(ctx, p);
      break;
    case ICON_PARTLY_CLOUDY_DAY:
      draw_pixel_sun(ctx, GPoint(p.x - 2, p.y - 1), true);
      break;
    case ICON_PARTLY_CLOUDY_NIGHT:
      draw_pixel_moon(ctx, GPoint(p.x - 3, p.y - 1));
      draw_pixel_cloud(ctx, GPoint(p.x + 2, p.y + 2), GColorWhite);
      break;
    case ICON_CLOUDY:
      draw_pixel_cloud(ctx, p, GColorWhite);
      break;
    case ICON_RAIN:
      draw_pixel_rain(ctx, p);
      break;
    case ICON_THUNDER:
      draw_pixel_thunder(ctx, p);
      break;
    case ICON_SNOW:
      draw_pixel_snow(ctx, p);
      break;
    case ICON_FOG:
      draw_pixel_fog(ctx, p);
      break;
    default:
      fill_rect(ctx, p.x - 2, p.y - 2, 4, 4, GColorDarkGray);
      break;
  }
}

static void draw_ticks(GContext *ctx) {
  graphics_context_set_stroke_color(ctx, s_theme.tick);
  for (int i = 0; i < 60; i++) {
    int32_t angle = TRIG_MAX_ANGLE * i / 60;
    int outer = s_radius + 2;
    int inner = (i % 5 == 0) ? s_radius - 9 : s_radius - 4;
    GPoint p1 = {
      .x = (int16_t)(s_center.x + sin_lookup(angle) * inner / TRIG_MAX_RATIO),
      .y = (int16_t)(s_center.y - cos_lookup(angle) * inner / TRIG_MAX_RATIO)
    };
    GPoint p2 = {
      .x = (int16_t)(s_center.x + sin_lookup(angle) * outer / TRIG_MAX_RATIO),
      .y = (int16_t)(s_center.y - cos_lookup(angle) * outer / TRIG_MAX_RATIO)
    };
    graphics_draw_line(ctx, p1, p2);
  }
}

static void draw_weather_ring(GContext *ctx) {
  for (int i = 0; i < WEATHER_SLOTS; i++) {
    int32_t angle = TRIG_MAX_ANGLE * i / 12;
    int icon_radius = s_radius - 18;
    GPoint p = {
      .x = (int16_t)(s_center.x + sin_lookup(angle) * icon_radius / TRIG_MAX_RATIO),
      .y = (int16_t)(s_center.y - cos_lookup(angle) * icon_radius / TRIG_MAX_RATIO)
    };
    draw_icon(ctx, p, s_icons[i]);
  }
}

static void draw_battery_segments(GContext *ctx) {
  BatteryChargeState state = battery_state_service_peek();
  int filled = state.charge_percent / 10;
  bool low = state.charge_percent < 20;
  GColor ring_color = low ? s_theme.battery_low : s_theme.battery;

  graphics_context_set_stroke_width(ctx, 3);
  graphics_context_set_stroke_color(ctx, ring_color);

  int inner_r = s_center_radius + 4;
  int outer_r = s_center_radius + 9;

  for (int i = 0; i < BATTERY_SEGMENTS; i++) {
    if (i >= filled) break;
    int32_t angle = TRIG_MAX_ANGLE * i / BATTERY_SEGMENTS;
    GPoint p1 = {
      .x = (int16_t)(s_center.x + sin_lookup(angle) * inner_r / TRIG_MAX_RATIO),
      .y = (int16_t)(s_center.y - cos_lookup(angle) * inner_r / TRIG_MAX_RATIO)
    };
    GPoint p2 = {
      .x = (int16_t)(s_center.x + sin_lookup(angle) * outer_r / TRIG_MAX_RATIO),
      .y = (int16_t)(s_center.y - cos_lookup(angle) * outer_r / TRIG_MAX_RATIO)
    };
    graphics_draw_line(ctx, p1, p2);
  }
}

static void draw_hands(GContext *ctx, struct tm *t) {
  int32_t minute_angle = TRIG_MAX_ANGLE * t->tm_min / 60;
  int32_t hour_angle = TRIG_MAX_ANGLE * (((t->tm_hour % 12) * 6) + (t->tm_min / 10)) / 72;

  graphics_context_set_stroke_width(ctx, 7);
  graphics_context_set_stroke_color(ctx, s_theme.hour);
  GPoint hour_end = {
    .x = (int16_t)(s_center.x + sin_lookup(hour_angle) * (s_radius - 32) / TRIG_MAX_RATIO),
    .y = (int16_t)(s_center.y - cos_lookup(hour_angle) * (s_radius - 32) / TRIG_MAX_RATIO)
  };
  graphics_draw_line(ctx, s_center, hour_end);

  graphics_context_set_stroke_color(ctx, s_theme.minute);
  GPoint minute_end = {
    .x = (int16_t)(s_center.x + sin_lookup(minute_angle) * (s_radius - 20) / TRIG_MAX_RATIO),
    .y = (int16_t)(s_center.y - cos_lookup(minute_angle) * (s_radius - 20) / TRIG_MAX_RATIO)
  };
  graphics_draw_line(ctx, s_center, minute_end);
}

static void draw_center(GContext *ctx, struct tm *t) {
  graphics_context_set_fill_color(ctx, s_theme.center);
  graphics_fill_circle(ctx, s_center, s_center_radius);
  graphics_context_set_fill_color(ctx, s_theme.bg);
  graphics_fill_circle(ctx, s_center, s_center_radius - 4);

  static char day_buf[4];
  snprintf(day_buf, sizeof(day_buf), "%d", t->tm_mday);
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, day_buf,
                     fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(s_center.x - 12, s_center.y - 12, 24, 24),
                     GTextOverflowModeWordWrap,
                     GTextAlignmentCenter,
                     NULL);

  draw_battery_segments(ctx);
}

static void draw_temperature(GContext *ctx) {
  graphics_context_set_text_color(ctx, s_theme.temp);
  graphics_draw_text(ctx, s_temp_text,
                     fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                     GRect(20, 122, 104, 28),
                     GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter,
                     NULL);
}

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, s_theme.bg);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  time_t now = time(NULL);
  struct tm *t = localtime(&now);

  draw_ticks(ctx);
  draw_weather_ring(ctx);
  draw_hands(ctx, t);
  draw_center(ctx, t);
  draw_temperature(ctx);
}

static void tick_handler(struct tm *tick_time, TimeUnits changed) {
  if (tick_time->tm_min == 0) {
    DictionaryIterator *out;
    if (app_message_outbox_begin(&out) == APP_MSG_OK) {
      dict_write_uint8(out, KEY_REQUEST_WEATHER, 1);
      app_message_outbox_send();
    }
  }
  layer_mark_dirty(s_canvas_layer);
}

static void battery_handler(BatteryChargeState state) {
  layer_mark_dirty(s_canvas_layer);
}

static void inbox_received(DictionaryIterator *iter, void *context) {
  Tuple *t;

  if ((t = dict_find(iter, KEY_TEMP))) {
    snprintf(s_temp_text, sizeof(s_temp_text), "%s", t->value->cstring);
  }
  if ((t = dict_find(iter, KEY_CODES))) {
    snprintf(s_codes_text, sizeof(s_codes_text), "%s", t->value->cstring);
    parse_codes();
  }

  if ((t = dict_find(iter, KEY_BG_COLOR))) s_theme.bg = color_from_uint(t->value->uint32);
  if ((t = dict_find(iter, KEY_HOUR_COLOR))) s_theme.hour = color_from_uint(t->value->uint32);
  if ((t = dict_find(iter, KEY_MINUTE_COLOR))) s_theme.minute = color_from_uint(t->value->uint32);
  if ((t = dict_find(iter, KEY_CENTER_COLOR))) s_theme.center = color_from_uint(t->value->uint32);
  if ((t = dict_find(iter, KEY_BATTERY_COLOR))) s_theme.battery = color_from_uint(t->value->uint32);
  if ((t = dict_find(iter, KEY_BATTERY_LOW_COLOR))) s_theme.battery_low = color_from_uint(t->value->uint32);
  if ((t = dict_find(iter, KEY_TEMP_COLOR))) s_theme.temp = color_from_uint(t->value->uint32);
  if ((t = dict_find(iter, KEY_TICK_COLOR))) s_theme.tick = color_from_uint(t->value->uint32);
  if ((t = dict_find(iter, KEY_ICON_ACCENT_COLOR))) s_theme.accent = color_from_uint(t->value->uint32);

  layer_mark_dirty(s_canvas_layer);
}

static void inbox_dropped(AppMessageResult reason, void *context) {}
static void outbox_failed(DictionaryIterator *iter, AppMessageResult reason, void *context) {}
static void outbox_sent(DictionaryIterator *iter, void *context) {}

static void window_load(Window *window) {
  GRect bounds = window_get_root_layer(window)->bounds;
  s_center = grect_center_point(&bounds);
  s_center.y -= 8;

  s_canvas_layer = layer_create(bounds);
  layer_set_update_proc(s_canvas_layer, canvas_update_proc);
  layer_add_child(window_get_root_layer(window), s_canvas_layer);
}

static void window_unload(Window *window) {
  layer_destroy(s_canvas_layer);
}

static void init(void) {
  load_defaults();
  parse_codes();

  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  battery_state_service_subscribe(battery_handler);

  app_message_register_inbox_received(inbox_received);
  app_message_register_inbox_dropped(inbox_dropped);
  app_message_register_outbox_failed(outbox_failed);
  app_message_register_outbox_sent(outbox_sent);
  app_message_open(512, 512);

  DictionaryIterator *out;
  if (app_message_outbox_begin(&out) == APP_MSG_OK) {
    dict_write_uint8(out, KEY_REQUEST_WEATHER, 1);
    app_message_outbox_send();
  }
}

static void deinit(void) {
  battery_state_service_unsubscribe();
  tick_timer_service_unsubscribe();
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
