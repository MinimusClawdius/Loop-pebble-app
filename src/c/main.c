/*
 * Loop CGM Watchface for Pebble
 * 
 * Displays: BG, trend, delta, IOB, time, date, loop status
 * Alerts: configurable low/high BG thresholds
 * Inspired by: Nightscout CGM, Urchin CGM watchfaces
 */

#include <pebble.h>

// ==================== Display Layers ====================

static Window *s_main_window;

// Time and date
static TextLayer *s_time_layer;
static TextLayer *s_date_layer;

// CGM data - main display
static TextLayer *s_bg_layer;           // Large BG number
static TextLayer *s_trend_layer;        // Trend arrow
static TextLayer *s_delta_layer;        // BG change
static TextLayer *s_ago_layer;          // Time since reading

// Secondary info
static TextLayer *s_iob_layer;          // Insulin on board
static TextLayer *s_loop_status_layer;  // Loop on/off indicator
static TextLayer *s_cob_layer;          // Carbs on board

// Status bar
static TextLayer *s_battery_layer;
static BitmapLayer *s_bt_icon_layer;
static GBitmap *s_bt_icon_bitmap;

// Graph layer for BG history
static Layer *s_graph_layer;

// ==================== Data Storage ====================

static char bg_buffer[8] = "--";
static char trend_buffer[4] = "";
static char delta_buffer[10] = "";
static char ago_buffer[12] = "";
static char iob_buffer[16] = "";
static char cob_buffer[16] = "";
static char loop_buffer[4] = "";
static char time_buffer[8] = "";
static char date_buffer[12] = "";
static char battery_buffer[8] = "";

// BG history for graph (last 12 readings, ~1 hour)
#define BG_HISTORY_SIZE 12
static int bg_history[BG_HISTORY_SIZE] = {0};
static int bg_history_index = 0;
static bool bg_history_full = false;

// Current data values
static int current_bg = 0;
static int current_bg_delta = 0;
static bool bg_is_stale = true;
static time_t last_reading_time = 0;
static bool loop_is_active = false;

// ==================== AppMessage Keys ====================

#define KEY_GLUCOSE 0
#define KEY_TREND 1
#define KEY_IOB 2
#define KEY_IS_CLOSED_LOOP 3
#define KEY_COB 4
#define KEY_BATTERY 5
#define KEY_GLUCOSE_DATE 6
#define KEY_DELTA 7
#define KEY_ALERT_CONFIG 8

// ==================== Alert Configuration ====================

static int alert_low = 70;      // mg/dL - configurable
static int alert_high = 180;    // mg/dL - configurable
static int alert_urgent_low = 55;
static int alert_urgent_high = 250;

// Alert snooze (minutes)
static time_t last_low_alert = 0;
static time_t last_high_alert = 0;
static const int ALERT_SNOOZE_MIN = 15;

// ==================== Colors (Basalt+) ====================

#ifdef PBL_COLOR
  #define COLOR_BG_NORMAL GColorGreen
  #define COLOR_BG_LOW GColorRed
  #define COLOR_BG_HIGH GColorOrange
  #define COLOR_BG_STALE GColorDarkGray
  #define COLOR_LOOP_ON GColorGreen
  #define COLOR_LOOP_OFF GColorRed
  #define COLOR_IOB GColorCyan
  #define COLOR_COB GColorYellow
#else
  #define COLOR_BG_NORMAL GColorWhite
  #define COLOR_BG_LOW GColorWhite
  #define COLOR_BG_HIGH GColorWhite
  #define COLOR_BG_STALE GColorLightGray
  #define COLOR_LOOP_ON GColorWhite
  #define COLOR_LOOP_OFF GColorWhite
  #define COLOR_IOB GColorWhite
  #define COLOR_COB GColorWhite
#endif

// ==================== Forward Declarations ====================

static void update_display(void);
static void check_alerts(void);
static void add_bg_to_history(int bg);

// ==================== Graph Drawing ====================

static void graph_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  
  // Background
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  
  // Target range shading (70-180 mg/dL)
  int y_low = bounds.size.h - ((70 - 40) * bounds.size.h / (400 - 40));
  int y_high = bounds.size.h - ((180 - 40) * bounds.size.h / (400 - 40));
  
  #ifdef PBL_COLOR
  graphics_context_set_fill_color(ctx, GColorDarkGreen);
  graphics_fill_rect(ctx, GRect(0, y_high, bounds.size.w, y_low - y_high), 0, GCornerNone);
  #endif
  
  // Draw BG line
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, 2);
  
  int count = bg_history_full ? BG_HISTORY_SIZE : bg_history_index;
  if (count < 2) return;
  
  int x_step = bounds.size.w / (BG_HISTORY_SIZE - 1);
  
  for (int i = 1; i < count; i++) {
    int idx1 = bg_history_full ? ((bg_history_index + i - 1) % BG_HISTORY_SIZE) : (i - 1);
    int idx2 = bg_history_full ? ((bg_history_index + i) % BG_HISTORY_SIZE) : i;
    
    int bg1 = bg_history[idx1];
    int bg2 = bg_history[idx2];
    
    if (bg1 <= 0 || bg2 <= 0) continue;
    
    // Clamp values
    if (bg1 < 40) bg1 = 40;
    if (bg1 > 400) bg1 = 400;
    if (bg2 < 40) bg2 = 40;
    if (bg2 > 400) bg2 = 400;
    
    int x1 = (i - 1) * x_step;
    int x2 = i * x_step;
    int y1 = bounds.size.h - ((bg1 - 40) * bounds.size.h / (400 - 40));
    int y2 = bounds.size.h - ((bg2 - 40) * bounds.size.h / (400 - 40));
    
    graphics_draw_line(ctx, GPoint(x1, y1), GPoint(x2, y2));
  }
  
  // Draw current BG dot
  if (current_bg > 0) {
    int last_idx = bg_history_full ? ((bg_history_index + BG_HISTORY_SIZE - 1) % BG_HISTORY_SIZE) : (bg_history_index - 1);
    if (last_idx >= 0) {
      int x = (count - 1) * x_step;
      int y = bounds.size.h - ((current_bg - 40) * bounds.size.h / (400 - 40));
      graphics_context_set_fill_color(ctx, COLOR_BG_NORMAL);
      graphics_fill_circle(ctx, GPoint(x, y), 4);
    }
  }
}

// ==================== Alert System ====================

static void check_alerts(void) {
  if (bg_is_stale || current_bg <= 0) return;
  
  time_t now = time(NULL);
  
  // Urgent low
  if (current_bg <= alert_urgent_low) {
    if (now - last_low_alert > ALERT_SNOOZE_MIN * 60) {
      vibes_double_pulse();
      last_low_alert = now;
    }
    return;
  }
  
  // Low
  if (current_bg <= alert_low) {
    if (now - last_low_alert > ALERT_SNOOZE_MIN * 60) {
      vibes_short_pulse();
      last_low_alert = now;
    }
    return;
  }
  
  // Urgent high
  if (current_bg >= alert_urgent_high) {
    if (now - last_high_alert > ALERT_SNOOZE_MIN * 60) {
      vibes_double_pulse();
      last_high_alert = now;
    }
    return;
  }
  
  // High
  if (current_bg >= alert_high) {
    if (now - last_high_alert > ALERT_SNOOZE_MIN * 60) {
      vibes_short_pulse();
      last_high_alert = now;
    }
    return;
  }
}

// ==================== Display Update ====================

static void update_display(void) {
  // Update BG
  if (current_bg > 0) {
    snprintf(bg_buffer, sizeof(bg_buffer), "%d", current_bg);
  } else {
    snprintf(bg_buffer, sizeof(bg_buffer), "---");
  }
  text_layer_set_text(s_bg_layer, bg_buffer);
  
  // BG color based on range
  #ifdef PBL_COLOR
  if (bg_is_stale) {
    text_layer_set_text_color(s_bg_layer, COLOR_BG_STALE);
  } else if (current_bg <= alert_low) {
    text_layer_set_text_color(s_bg_layer, COLOR_BG_LOW);
  } else if (current_bg >= alert_high) {
    text_layer_set_text_color(s_bg_layer, COLOR_BG_HIGH);
  } else {
    text_layer_set_text_color(s_bg_layer, COLOR_BG_NORMAL);
  }
  #endif
  
  // Update delta
  if (current_bg_delta != 0) {
    snprintf(delta_buffer, sizeof(delta_buffer), "%+d", current_bg_delta);
  } else {
    snprintf(delta_buffer, sizeof(delta_buffer), "");
  }
  text_layer_set_text(s_delta_layer, delta_buffer);
  
  // Update trend
  text_layer_set_text(s_trend_layer, trend_buffer);
  
  // Update time ago
  if (last_reading_time > 0) {
    int minutes_ago = (int)(time(NULL) - last_reading_time) / 60;
    if (minutes_ago < 1) {
      snprintf(ago_buffer, sizeof(ago_buffer), "now");
    } else if (minutes_ago < 60) {
      snprintf(ago_buffer, sizeof(ago_buffer), "%dm ago", minutes_ago);
    } else {
      snprintf(ago_buffer, sizeof(ago_buffer), "%dh ago", minutes_ago / 60);
    }
    bg_is_stale = (minutes_ago > 15);
  }
  text_layer_set_text(s_ago_layer, ago_buffer);
  
  // Update IOB
  text_layer_set_text(s_iob_layer, iob_buffer);
  
  // Update COB
  text_layer_set_text(s_cob_layer, cob_buffer);
  
  // Update loop status
  text_layer_set_text(s_loop_status_layer, loop_buffer);
  #ifdef PBL_COLOR
  text_layer_set_text_color(s_loop_status_layer, loop_is_active ? COLOR_LOOP_ON : COLOR_LOOP_OFF);
  #endif
  
  // Update time
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);
  strftime(time_buffer, sizeof(time_buffer), "%H:%M", tick_time);
  text_layer_set_text(s_time_layer, time_buffer);
  
  // Update date
  strftime(date_buffer, sizeof(date_buffer), "%a %m/%d", tick_time);
  text_layer_set_text(s_date_layer, date_buffer);
  
  // Redraw graph
  layer_mark_dirty(s_graph_layer);
  
  // Check alerts
  check_alerts();
}

static void add_bg_to_history(int bg) {
  bg_history[bg_history_index] = bg;
  bg_history_index = (bg_history_index + 1) % BG_HISTORY_SIZE;
  if (bg_history_index == 0) bg_history_full = true;
}

// ==================== AppMessage Handler ====================

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  // Glucose
  Tuple *glucose_tuple = dict_find(iterator, KEY_GLUCOSE);
  if (glucose_tuple) {
    current_bg = (int)glucose_tuple->value->int32;
    add_bg_to_history(current_bg);
  }
  
  // Trend
  Tuple *trend_tuple = dict_find(iterator, KEY_TREND);
  if (trend_tuple) {
    snprintf(trend_buffer, sizeof(trend_buffer), "%s", trend_tuple->value->cstring);
  }
  
  // Delta
  Tuple *delta_tuple = dict_find(iterator, KEY_DELTA);
  if (delta_tuple) {
    current_bg_delta = (int)delta_tuple->value->int32;
  }
  
  // Glucose date (for time ago)
  Tuple *glucose_date_tuple = dict_find(iterator, KEY_GLUCOSE_DATE);
  if (glucose_date_tuple) {
    last_reading_time = (time_t)glucose_date_tuple->value->uint32;
  }
  
  // IOB
  Tuple *iob_tuple = dict_find(iterator, KEY_IOB);
  if (iob_tuple) {
    int iob = (int)iob_tuple->value->int32;
    snprintf(iob_buffer, sizeof(iob_buffer), "IOB %d.%dU", iob / 10, abs(iob % 10));
  }
  
  // COB
  Tuple *cob_tuple = dict_find(iterator, KEY_COB);
  if (cob_tuple) {
    int cob = (int)cob_tuple->value->int32;
    snprintf(cob_buffer, sizeof(cob_buffer), "COB %dg", cob);
  }
  
  // Loop status
  Tuple *loop_tuple = dict_find(iterator, KEY_IS_CLOSED_LOOP);
  if (loop_tuple) {
    loop_is_active = loop_tuple->value->int32 > 0;
    snprintf(loop_buffer, sizeof(loop_buffer), "%s", loop_is_active ? "●" : "○");
  }
  
  // Battery
  Tuple *battery_tuple = dict_find(iterator, KEY_BATTERY);
  if (battery_tuple) {
    int battery = (int)battery_tuple->value->int32;
    snprintf(battery_buffer, sizeof(battery_buffer), "%d%%", battery);
  }
  
  update_display();
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped: %d", reason);
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed: %d", reason);
}

// ==================== Bluetooth & Battery ====================

static void bluetooth_callback(bool connected) {
  layer_set_hidden(bitmap_layer_get_layer(s_bt_icon_layer), connected);
  if (!connected) {
    vibes_short_pulse();
  }
}

static void battery_callback(BatteryChargeState state) {
  snprintf(battery_buffer, sizeof(battery_buffer), "%d%%", state.charge_percent);
  text_layer_set_text(s_battery_layer, battery_buffer);
}

// ==================== Data Request ====================

static void request_data(void) {
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) == APP_MSG_OK) {
    dict_write_uint8(iter, 0, 0);
    app_message_outbox_send();
  }
}

// ==================== Tick Handler ====================

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  // Update time every minute
  update_display();
  
  // Request data every 5 minutes
  if (tick_time->tm_min % 5 == 0) {
    request_data();
  }
}

// ==================== Window Setup ====================

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  
  // Background
  window_set_background_color(window, GColorBlack);
  
  int y = 0;
  
  // === Top Bar: Time and Date ===
  s_time_layer = text_layer_create(GRect(0, y, 80, 28));
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_color(s_time_layer, GColorWhite);
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentLeft);
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));
  
  s_date_layer = text_layer_create(GRect(80, y + 4, bounds.size.w - 80, 20));
  text_layer_set_font(s_date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_color(s_date_layer, GColorLightGray);
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentRight);
  layer_add_child(window_layer, text_layer_get_layer(s_date_layer));
  
  y += 28;
  
  // === Main BG Display ===
  // BG number - large, centered
  s_bg_layer = text_layer_create(GRect(0, y, bounds.size.w - 50, 50));
  text_layer_set_font(s_bg_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
  text_layer_set_text_color(s_bg_layer, GColorWhite);
  text_layer_set_background_color(s_bg_layer, GColorClear);
  text_layer_set_text_alignment(s_bg_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_bg_layer));
  
  // Trend arrow - right of BG
  s_trend_layer = text_layer_create(GRect(bounds.size.w - 50, y + 8, 50, 40));
  text_layer_set_font(s_trend_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_color(s_trend_layer, GColorWhite);
  text_layer_set_background_color(s_trend_layer, GColorClear);
  text_layer_set_text_alignment(s_trend_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_trend_layer));
  
  y += 48;
  
  // === Delta and Time Ago Row ===
  s_delta_layer = text_layer_create(GRect(0, y, bounds.size.w / 2, 22));
  text_layer_set_font(s_delta_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_color(s_delta_layer, GColorLightGray);
  text_layer_set_background_color(s_delta_layer, GColorClear);
  text_layer_set_text_alignment(s_delta_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_delta_layer));
  
  s_ago_layer = text_layer_create(GRect(bounds.size.w / 2, y, bounds.size.w / 2, 22));
  text_layer_set_font(s_ago_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_color(s_ago_layer, GColorLightGray);
  text_layer_set_background_color(s_ago_layer, GColorClear);
  text_layer_set_text_alignment(s_ago_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_ago_layer));
  
  y += 24;
  
  // === Graph ===
  s_graph_layer = layer_create(GRect(5, y, bounds.size.w - 10, 40));
  layer_set_update_proc(s_graph_layer, graph_update_proc);
  layer_add_child(window_layer, s_graph_layer);
  
  y += 44;
  
  // === IOB and COB Row ===
  s_iob_layer = text_layer_create(GRect(0, y, bounds.size.w / 2, 22));
  text_layer_set_font(s_iob_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_color(s_iob_layer, COLOR_IOB);
  text_layer_set_background_color(s_iob_layer, GColorClear);
  text_layer_set_text_alignment(s_iob_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_iob_layer));
  
  s_cob_layer = text_layer_create(GRect(bounds.size.w / 2, y, bounds.size.w / 2, 22));
  text_layer_set_font(s_cob_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_color(s_cob_layer, COLOR_COB);
  text_layer_set_background_color(s_cob_layer, GColorClear);
  text_layer_set_text_alignment(s_cob_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_cob_layer));
  
  y += 24;
  
  // === Bottom Status Bar ===
  // Loop status
  s_loop_status_layer = text_layer_create(GRect(0, y, 30, 22));
  text_layer_set_font(s_loop_status_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_background_color(s_loop_status_layer, GColorClear);
  text_layer_set_text_alignment(s_loop_status_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_loop_status_layer));
  
  // Battery
  s_battery_layer = text_layer_create(GRect(30, y, 50, 22));
  text_layer_set_font(s_battery_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_color(s_battery_layer, GColorLightGray);
  text_layer_set_background_color(s_battery_layer, GColorClear);
  layer_add_child(window_layer, text_layer_get_layer(s_battery_layer));
  
  // Bluetooth icon (shown when disconnected)
  s_bt_icon_layer = bitmap_layer_create(GRect(bounds.size.w - 30, y, 24, 24));
  bitmap_layer_set_compositing_mode(s_bt_icon_layer, GCompOpSet);
  layer_set_hidden(bitmap_layer_get_layer(s_bt_icon_layer), true);
  layer_add_child(window_layer, bitmap_layer_get_layer(s_bt_icon_layer));
  
  // Initial data request
  request_data();
}

static void main_window_unload(Window *window) {
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_date_layer);
  text_layer_destroy(s_bg_layer);
  text_layer_destroy(s_trend_layer);
  text_layer_destroy(s_delta_layer);
  text_layer_destroy(s_ago_layer);
  text_layer_destroy(s_iob_layer);
  text_layer_destroy(s_cob_layer);
  text_layer_destroy(s_loop_status_layer);
  text_layer_destroy(s_battery_layer);
  bitmap_layer_destroy(s_bt_icon_layer);
  layer_destroy(s_graph_layer);
}

// ==================== Init/Deinit ====================

static void init(void) {
  // Register callbacks
  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  
  // Open app message (larger buffer for config)
  app_message_open(512, 256);
  
  // Create main window
  s_main_window = window_create();
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });
  window_stack_push(s_main_window, true);
  
  // Register tick handler
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  
  // Register bluetooth handler
  bluetooth_connection_service_subscribe(bluetooth_callback);
  
  // Register battery handler
  battery_state_service_subscribe(battery_callback);
  
  // Initial display
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
