/*
 * Loop CGM Watchface for Pebble
 * 
 * Displays: BG, trend, delta, IOB, time, date, loop status, mini chart
 * Inspired by: T1000 CGM, Nightscout CGM, Urchin CGM watchfaces
 */

#include <pebble.h>

// ==================== Display Layers ====================

static Window *s_main_window;
static Layer *s_chart_layer;
static Layer *s_battery_layer;

// Time and date combined
static TextLayer *s_time_date_layer;

// CGM data - main display
static TextLayer *s_cgm_value_layer;    // Large BG number
static BitmapLayer *s_trend_layer;      // Trend arrow bitmap
static GBitmap *s_trend_bitmap;
static TextLayer *s_delta_layer;        // BG change
static TextLayer *s_time_ago_layer;     // Time since reading

// Secondary info
static TextLayer *s_iob_layer;          // Insulin on board
static TextLayer *s_status_layer;       // Loop + COB combined

// Setup message (shown when no data)
static TextLayer *s_setup_layer;

// ==================== Data Storage ====================

static char time_date_buffer[24];
static char cgm_value_buffer[8];
static char delta_buffer[12];
static char time_ago_buffer[16];
static char iob_buffer[20];
static char status_buffer[24];

// Chart data (24 points = 2 hours at 5-min intervals)
#define CHART_MAX_POINTS 24
#define CHART_Y_MIN 40
#define CHART_Y_MAX 300
static int16_t chart_values[CHART_MAX_POINTS];
static int chart_count = 0;

// Current data values
static int current_bg = 0;
static int current_bg_delta = 0;
static time_t last_reading_time = 0;
static bool loop_is_active = false;
static int current_iob = 0;
static int current_cob = 0;
static bool has_data = false;

// Demo mode for testing without iPhone
#define DEMO_MODE true

// Trend arrow index
static uint8_t current_trend = 0;

// Battery state
static int battery_level = 0;
static bool battery_charging = false;

// ==================== AppMessage Keys ====================

#define KEY_GLUCOSE 0
#define KEY_TREND 1
#define KEY_IOB 2
#define KEY_IS_CLOSED_LOOP 3
#define KEY_COB 4
#define KEY_BATTERY 5
#define KEY_GLUCOSE_DATE 6
#define KEY_DELTA 7
#define KEY_HISTORY 8
#define KEY_REQUEST_DATA 9

// ==================== Alert Configuration ====================

static int alert_low = 70;
static int alert_high = 180;
static time_t last_alert_time = 0;
#define ALERT_SNOOZE_MIN 15

// ==================== Colors ====================

#ifdef PBL_COLOR
  #define COLOR_BG_NORMAL GColorGreen
  #define COLOR_BG_LOW GColorRed
  #define COLOR_BG_HIGH GColorOrange
  #define COLOR_BG_STALE GColorDarkGray
  #define COLOR_IOB GColorCyan
  #define COLOR_COB GColorYellow
  #define COLOR_LOOP_ON GColorGreen
  #define COLOR_LOOP_OFF GColorRed
  #define COLOR_CHART_LINE GColorCyan
  #define COLOR_CHART_RANGE GColorDarkGreen
#else
  #define COLOR_BG_NORMAL GColorWhite
  #define COLOR_BG_LOW GColorWhite
  #define COLOR_BG_HIGH GColorWhite
  #define COLOR_BG_STALE GColorLightGray
  #define COLOR_IOB GColorWhite
  #define COLOR_COB GColorWhite
  #define COLOR_LOOP_ON GColorWhite
  #define COLOR_LOOP_OFF GColorWhite
  #define COLOR_CHART_LINE GColorWhite
  #define COLOR_CHART_RANGE GColorLightGray
#endif

// ==================== Trend Arrows (text-based, no bitmaps needed) ====================

static const char* TREND_SYMBOLS[] = {"", "↑↑", "↑", "↗", "→", "↘", "↓", "↓↓"};

// ==================== Forward Declarations ====================

static void update_display(void);
static void request_data(void);
static void add_demo_data(void);

// ==================== Chart Drawing ====================

static void chart_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  
  // Background
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  
  // Target range shading (70-180 mg/dL)
  int y_low = bounds.size.h - ((alert_low - CHART_Y_MIN) * bounds.size.h / (CHART_Y_MAX - CHART_Y_MIN));
  int y_high = bounds.size.h - ((alert_high - CHART_Y_MIN) * bounds.size.h / (CHART_Y_MAX - CHART_Y_MIN));
  
  #ifdef PBL_COLOR
  graphics_context_set_fill_color(ctx, GColorDarkGreen);
  graphics_fill_rect(ctx, GRect(0, y_high, bounds.size.w, y_low - y_high), GCornerNone, 0);
  #endif
  
  // Draw low/high lines
  graphics_context_set_stroke_color(ctx, GColorDarkGray);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, GPoint(0, y_low), GPoint(bounds.size.w, y_low));
  graphics_draw_line(ctx, GPoint(0, y_high), GPoint(bounds.size.w, y_high));
  
  // Draw BG line
  if (chart_count < 2) return;
  
  graphics_context_set_stroke_color(ctx, COLOR_CHART_LINE);
  graphics_context_set_stroke_width(ctx, 2);
  
  int x_step = bounds.size.w / (CHART_MAX_POINTS - 1);
  
  for (int i = 1; i < chart_count; i++) {
    int bg1 = chart_values[i - 1];
    int bg2 = chart_values[i];
    
    if (bg1 <= 0 || bg2 <= 0) continue;
    
    // Clamp values
    if (bg1 < CHART_Y_MIN) bg1 = CHART_Y_MIN;
    if (bg1 > CHART_Y_MAX) bg1 = CHART_Y_MAX;
    if (bg2 < CHART_Y_MIN) bg2 = CHART_Y_MIN;
    if (bg2 > CHART_Y_MAX) bg2 = CHART_Y_MAX;
    
    int x1 = (i - 1) * x_step;
    int x2 = i * x_step;
    int y1 = bounds.size.h - ((bg1 - CHART_Y_MIN) * bounds.size.h / (CHART_Y_MAX - CHART_Y_MIN));
    int y2 = bounds.size.h - ((bg2 - CHART_Y_MIN) * bounds.size.h / (CHART_Y_MAX - CHART_Y_MIN));
    
    graphics_draw_line(ctx, GPoint(x1, y1), GPoint(x2, y2));
  }
  
  // Draw current BG dot
  if (current_bg > 0 && chart_count > 0) {
    int x = (chart_count - 1) * x_step;
    int bg = current_bg;
    if (bg < CHART_Y_MIN) bg = CHART_Y_MIN;
    if (bg > CHART_Y_MAX) bg = CHART_Y_MAX;
    int y = bounds.size.h - ((bg - CHART_Y_MIN) * bounds.size.h / (CHART_Y_MAX - CHART_Y_MIN));
    
    graphics_context_set_fill_color(ctx, COLOR_BG_NORMAL);
    graphics_fill_circle(ctx, GPoint(x, y), 4);
  }
}

// ==================== Battery Drawing ====================

static void battery_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  
  int bat_w = 20;
  int bat_h = 10;
  int tip_w = 2;
  int tip_h = 4;
  int x = (bounds.size.w - bat_w - tip_w) / 2;
  int y = (bounds.size.h - bat_h) / 2;
  
  // Outline
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_draw_round_rect(ctx, GRect(x, y, bat_w, bat_h), 1);
  
  // Tip
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, GRect(x + bat_w, y + (bat_h - tip_h) / 2, tip_w, tip_h), 0, GCornerNone);
  
  // Fill level
  int fill_w = (bat_w - 2) * battery_level / 100;
  if (fill_w > 0) {
    if (battery_level <= 20) {
      graphics_context_set_fill_color(ctx, GColorRed);
    } else if (battery_charging) {
      graphics_context_set_fill_color(ctx, GColorGreen);
    } else {
      graphics_context_set_fill_color(ctx, GColorWhite);
    }
    graphics_fill_rect(ctx, GRect(x + 1, y + 1, fill_w, bat_h - 2), 0, GCornerNone);
  }
}

// ==================== Alert System ====================

static void check_alerts(void) {
  if (!has_data || current_bg <= 0) return;
  
  time_t now = time(NULL);
  if (now - last_alert_time < ALERT_SNOOZE_MIN * 60) return;
  
  if (current_bg < alert_low || current_bg > alert_high) {
    vibes_short_pulse();
    last_alert_time = now;
  }
}

// ==================== Display Update ====================

static void update_display(void) {
  // Update time and date
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);
  strftime(time_date_buffer, sizeof(time_date_buffer), "%H:%M  %a %m/%d", tick_time);
  text_layer_set_text(s_time_date_layer, time_date_buffer);
  
  if (!has_data) {
    #if DEMO_MODE
    add_demo_data();
    #else
    text_layer_set_text(s_cgm_value_layer, "---");
    text_layer_set_text(s_delta_layer, "");
    text_layer_set_text(s_time_ago_layer, "No data");
    text_layer_set_text(s_iob_layer, "");
    text_layer_set_text(s_status_layer, "Open Loop app");
    layer_set_hidden(s_chart_layer, true);
    return;
    #endif
  }
  
  // Update CGM value
  snprintf(cgm_value_buffer, sizeof(cgm_value_buffer), "%d", current_bg);
  text_layer_set_text(s_cgm_value_layer, cgm_value_buffer);
  
  // Color based on range
  #ifdef PBL_COLOR
  if (current_bg < alert_low) {
    text_layer_set_text_color(s_cgm_value_layer, COLOR_BG_LOW);
  } else if (current_bg > alert_high) {
    text_layer_set_text_color(s_cgm_value_layer, COLOR_BG_HIGH);
  } else {
    text_layer_set_text_color(s_cgm_value_layer, COLOR_BG_NORMAL);
  }
  #endif
  
  // Update trend
  if (current_trend > 0 && current_trend < 8) {
    text_layer_set_text(s_delta_layer, TREND_SYMBOLS[current_trend]);
  }
  
  // Update delta
  if (current_bg_delta != 0) {
    snprintf(time_ago_buffer, sizeof(time_ago_buffer), "%+d", current_bg_delta);
    text_layer_set_text(s_time_ago_layer, time_ago_buffer);
  }
  
  // Update IOB
  if (current_iob > 0) {
    snprintf(iob_buffer, sizeof(iob_buffer), "IOB %d.%dU", current_iob / 10, current_iob % 10);
    text_layer_set_text(s_iob_layer, iob_buffer);
  }
  
  // Update status (Loop + COB)
  if (loop_is_active) {
    if (current_cob > 0) {
      snprintf(status_buffer, sizeof(status_buffer), "● Loop  COB %dg", current_cob);
    } else {
      snprintf(status_buffer, sizeof(status_buffer), "● Loop");
    }
  } else {
    snprintf(status_buffer, sizeof(status_buffer), "○ Open loop");
  }
  text_layer_set_text(s_status_layer, status_buffer);
  
  #ifdef PBL_COLOR
  text_layer_set_text_color(s_status_layer, loop_is_active ? COLOR_LOOP_ON : COLOR_LOOP_OFF);
  #endif
  
  // Update time ago
  if (last_reading_time > 0) {
    int minutes_ago = (int)(time(NULL) - last_reading_time) / 60;
    if (minutes_ago < 1) {
      snprintf(iob_buffer, sizeof(iob_buffer), "just now");
    } else if (minutes_ago < 60) {
      snprintf(iob_buffer, sizeof(iob_buffer), "%dm ago", minutes_ago);
    } else {
      snprintf(iob_buffer, sizeof(iob_buffer), "%dh ago", minutes_ago / 60);
    }
  }
  
  // Show chart
  layer_set_hidden(s_chart_layer, false);
  layer_mark_dirty(s_chart_layer);
  
  // Update battery
  layer_mark_dirty(s_battery_layer);
  
  // Check alerts
  check_alerts();
}

// ==================== Demo Data (for testing without iPhone) ====================

static void add_demo_data(void) {
  has_data = true;
  current_bg = 125;
  current_trend = 4;  // Flat
  current_bg_delta = 3;
  current_iob = 25;   // 2.5U
  current_cob = 15;
  loop_is_active = true;
  last_reading_time = time(NULL) - 120;  // 2 minutes ago
  battery_level = 85;
  
  // Add demo chart data
  int demo_values[] = {110, 115, 120, 118, 122, 128, 135, 140, 138, 132, 128, 125,
                       122, 120, 118, 115, 112, 110, 108, 112, 118, 122, 125, 125};
  for (int i = 0; i < 24; i++) {
    chart_values[i] = demo_values[i];
  }
  chart_count = 24;
}

// ==================== AppMessage Handler ====================

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  has_data = true;
  last_reading_time = time(NULL);
  
  Tuple *glucose_tuple = dict_find(iterator, KEY_GLUCOSE);
  if (glucose_tuple) {
    current_bg = (int)glucose_tuple->value->int32;
    
    // Add to chart
    if (chart_count < CHART_MAX_POINTS) {
      chart_values[chart_count++] = current_bg;
    } else {
      // Shift left
      for (int i = 0; i < CHART_MAX_POINTS - 1; i++) {
        chart_values[i] = chart_values[i + 1];
      }
      chart_values[CHART_MAX_POINTS - 1] = current_bg;
    }
  }
  
  Tuple *trend_tuple = dict_find(iterator, KEY_TREND);
  if (trend_tuple) {
    current_trend = (uint8_t)trend_tuple->value->uint8;
  }
  
  Tuple *delta_tuple = dict_find(iterator, KEY_DELTA);
  if (delta_tuple) {
    current_bg_delta = (int)delta_tuple->value->int32;
  }
  
  Tuple *glucose_date_tuple = dict_find(iterator, KEY_GLUCOSE_DATE);
  if (glucose_date_tuple) {
    last_reading_time = (time_t)glucose_date_tuple->value->uint32;
  }
  
  Tuple *iob_tuple = dict_find(iterator, KEY_IOB);
  if (iob_tuple) {
    current_iob = (int)iob_tuple->value->int32;
  }
  
  Tuple *cob_tuple = dict_find(iterator, KEY_COB);
  if (cob_tuple) {
    current_cob = (int)cob_tuple->value->int32;
  }
  
  Tuple *loop_tuple = dict_find(iterator, KEY_IS_CLOSED_LOOP);
  if (loop_tuple) {
    loop_is_active = loop_tuple->value->int32 > 0;
  }
  
  Tuple *battery_tuple = dict_find(iterator, KEY_BATTERY);
  if (battery_tuple) {
    battery_level = (int)battery_tuple->value->int32;
  }
  
  update_display();
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped: %d", reason);
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed: %d", reason);
}

// ==================== Data Request ====================

static void request_data(void) {
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) == APP_MSG_OK) {
    dict_write_uint8(iter, KEY_REQUEST_DATA, 1);
    app_message_outbox_send();
  }
}

// ==================== Bluetooth & Battery ====================

static void bluetooth_callback(bool connected) {
  if (!connected) {
    vibes_short_pulse();
  }
}

static void battery_callback(BatteryChargeState state) {
  battery_level = state.charge_percent;
  battery_charging = state.is_charging;
  layer_mark_dirty(s_battery_layer);
}

// ==================== Tick Handler ====================

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_display();
  
  if (tick_time->tm_min % 5 == 0) {
    request_data();
  }
}

// ==================== Window Setup ====================

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  
  window_set_background_color(window, GColorBlack);
  
  int y = 0;
  
  // === Time and Date (top) ===
  s_time_date_layer = text_layer_create(GRect(0, y, bounds.size.w, 24));
  text_layer_set_font(s_time_date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_color(s_time_date_layer, GColorWhite);
  text_layer_set_background_color(s_time_date_layer, GColorClear);
  text_layer_set_text_alignment(s_time_date_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_time_date_layer));
  
  y += 26;
  
  // === Large CGM Value ===
  s_cgm_value_layer = text_layer_create(GRect(0, y, bounds.size.w - 20, 42));
  text_layer_set_font(s_cgm_value_layer, fonts_get_system_font(FONT_KEY_BITHAM_34_MEDIUM_NUMBERS));
  text_layer_set_text_color(s_cgm_value_layer, GColorWhite);
  text_layer_set_background_color(s_cgm_value_layer, GColorClear);
  text_layer_set_text_alignment(s_cgm_value_layer, GTextAlignmentRight);
  layer_add_child(window_layer, text_layer_get_layer(s_cgm_value_layer));
  
  // === Trend + Delta (right of CGM, stacked) ===
  s_delta_layer = text_layer_create(GRect(bounds.size.w - 42, y + 2, 40, 22));
  text_layer_set_font(s_delta_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_color(s_delta_layer, GColorWhite);
  text_layer_set_background_color(s_delta_layer, GColorClear);
  text_layer_set_text_alignment(s_delta_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_delta_layer));
  
  s_time_ago_layer = text_layer_create(GRect(bounds.size.w - 42, y + 22, 40, 18));
  text_layer_set_font(s_time_ago_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_color(s_time_ago_layer, GColorLightGray);
  text_layer_set_background_color(s_time_ago_layer, GColorClear);
  text_layer_set_text_alignment(s_time_ago_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_time_ago_layer));
  
  y += 44;
  
  // === Chart ===
  s_chart_layer = layer_create(GRect(5, y, bounds.size.w - 10, 36));
  layer_set_update_proc(s_chart_layer, chart_update_proc);
  layer_add_child(window_layer, s_chart_layer);
  layer_set_hidden(s_chart_layer, true);  // Hidden until data arrives
  
  y += 40;
  
  // === IOB ===
  s_iob_layer = text_layer_create(GRect(0, y, bounds.size.w, 20));
  text_layer_set_font(s_iob_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_color(s_iob_layer, COLOR_IOB);
  text_layer_set_background_color(s_iob_layer, GColorClear);
  text_layer_set_text_alignment(s_iob_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_iob_layer));
  
  y += 22;
  
  // === Status (Loop + COB) ===
  s_status_layer = text_layer_create(GRect(0, y, bounds.size.w - 24, 20));
  text_layer_set_font(s_status_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_color(s_status_layer, GColorLightGray);
  text_layer_set_background_color(s_status_layer, GColorClear);
  text_layer_set_text_alignment(s_status_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_status_layer));
  
  // === Battery (bottom right) ===
  s_battery_layer = layer_create(GRect(bounds.size.w - 28, y, 28, 18));
  layer_set_update_proc(s_battery_layer, battery_update_proc);
  layer_add_child(window_layer, s_battery_layer);
  
  // === Setup message (shown when no data) ===
  s_setup_layer = text_layer_create(GRect(10, 60, bounds.size.w - 20, 60));
  text_layer_set_font(s_setup_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_color(s_setup_layer, GColorLightGray);
  text_layer_set_background_color(s_setup_layer, GColorClear);
  text_layer_set_text_alignment(s_setup_layer, GTextAlignmentCenter);
  text_layer_set_text(s_setup_layer, "Waiting for\ndata from Loop...");
  layer_add_child(window_layer, text_layer_get_layer(s_setup_layer));
  
  // Initial request
  request_data();
  
  #if DEMO_MODE
  // Show demo data immediately
  add_demo_data();
  layer_set_hidden(s_setup_layer, true);
  #endif
}

static void main_window_unload(Window *window) {
  text_layer_destroy(s_time_date_layer);
  text_layer_destroy(s_cgm_value_layer);
  text_layer_destroy(s_delta_layer);
  text_layer_destroy(s_time_ago_layer);
  text_layer_destroy(s_iob_layer);
  text_layer_destroy(s_status_layer);
  text_layer_destroy(s_setup_layer);
  layer_destroy(s_chart_layer);
  layer_destroy(s_battery_layer);
}

// ==================== Init/Deinit ====================

static void init(void) {
  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  
  app_message_open(512, 256);
  
  s_main_window = window_create();
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });
  window_stack_push(s_main_window, true);
  
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  bluetooth_connection_service_subscribe(bluetooth_callback);
  battery_state_service_subscribe(battery_callback);
  
  // Get initial battery state
  BatteryChargeState bat = battery_state_service_peek();
  battery_level = bat.charge_percent;
  battery_charging = bat.is_charging;
  
  update_display();
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  bluetooth_connection_service_unsubscribe();
  battery_state_service_unsubscribe();
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
