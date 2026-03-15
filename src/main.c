/*
 * Loop CGM Monitor - Pebble Watch App
 * 
 * Displays blood glucose, trend, IOB, and loop status
 * Supports bolus requests and carb entries with iOS confirmation
 * Off-grid communication via Bluetooth to iPhone localhost
 */

#include <pebble.h>

// ==================== UI Elements ====================

// Main window
static Window *s_main_window;
static TextLayer *s_glucose_layer;
static TextLayer *s_trend_layer;
static TextLayer *s_iob_layer;
static TextLayer *s_status_layer;
static TextLayer *s_time_layer;
static TextLayer *s_hint_layer;

// Command menu window
static Window *s_menu_window;
static SimpleMenuLayer *s_menu_layer;
static SimpleMenuItem s_menu_items[3];
static SimpleMenuSection s_menu_section;

// Image resources
static GBitmap *s_icon_bolus;
static GBitmap *s_icon_carbs;
static GBitmap *s_icon_alert;
static GBitmap *s_icon_check;
static GBitmap *s_icon_reject;

// Bolus entry window
static Window *s_bolus_window;
static TextLayer *s_bolus_title_layer;
static TextLayer *s_bolus_amount_layer;
static TextLayer *s_bolus_hint_layer;

// Carb entry window
static Window *s_carbs_window;
static TextLayer *s_carbs_title_layer;
static TextLayer *s_carbs_amount_layer;
static TextLayer *s_carbs_hint_layer;

// Confirmation window
static Window *s_confirm_window;
static TextLayer *s_confirm_title_layer;
static TextLayer *s_confirm_msg_layer;

// ==================== Data ====================

static char glucose_buffer[16];
static char trend_buffer[8];
static char iob_buffer[24];
static char status_buffer[32];
static char time_buffer[8];

// Entry values
static double bolus_amount = 0.5;  // Starting at 0.5U
static int carbs_amount = 10;      // Starting at 10g
static char amount_buffer[16];

// AppMessage keys
#define KEY_GLUCOSE 0
#define KEY_TREND 1
#define KEY_IOB 2
#define KEY_IS_CLOSED_LOOP 3
#define KEY_COB 4
#define KEY_BATTERY 5
#define KEY_REQUEST_DATA 6
#define KEY_BOLUS_REQUEST 7
#define KEY_CARB_REQUEST 8
#define KEY_ABSORPTION_HOURS 9
#define KEY_COMMAND_STATUS 10
#define KEY_COMMAND_MSG 11

// Refresh interval: 5 minutes
#define REFRESH_INTERVAL_MS (5 * 60 * 1000)

// Alert thresholds
#define LOW_THRESHOLD 70
#define HIGH_THRESHOLD 180

// Bolus limits (safety)
#define BOLUS_MIN 0.05
#define BOLUS_MAX 10.0
#define BOLUS_STEP 0.05

// Carb limits
#define CARBS_MIN 5
#define CARBS_MAX 200
#define CARBS_STEP 5

// ==================== Helper Functions ====================

static void request_data(void) {
    DictionaryIterator *iter;
    if (app_message_outbox_begin(&iter) == APP_MSG_OK) {
        dict_write_uint8(iter, KEY_REQUEST_DATA, 1);
        app_message_outbox_send();
    }
}

static void request_bolus(double units) {
    DictionaryIterator *iter;
    if (app_message_outbox_begin(&iter) == APP_MSG_OK) {
        // Send as integer (units * 20 for 0.05U precision)
        dict_write_int32(iter, KEY_BOLUS_REQUEST, (int)(units * 20));
        app_message_outbox_send();
    }
}

static void request_carbs(int grams) {
    DictionaryIterator *iter;
    if (app_message_outbox_begin(&iter) == APP_MSG_OK) {
        dict_write_int32(iter, KEY_CARB_REQUEST, grams);
        dict_write_int32(iter, KEY_ABSORPTION_HOURS, 3);  // Default 3h absorption
        app_message_outbox_send();
    }
}

static void update_time(void) {
    time_t temp = time(NULL);
    struct tm *tick_time = localtime(&temp);
    strftime(time_buffer, sizeof(time_buffer), "%H:%M", tick_time);
    text_layer_set_text(s_time_layer, time_buffer);
}

static void check_alerts(int glucose) {
    static time_t last_alert = 0;
    time_t now = time(NULL);
    
    if (now - last_alert < 15 * 60) return;
    
    if (glucose > 0 && glucose < LOW_THRESHOLD) {
        vibes_double_pulse();
        last_alert = now;
    } else if (glucose > HIGH_THRESHOLD) {
        vibes_short_pulse();
        last_alert = now;
    }
}

// ==================== Confirmation Window ====================

static void confirm_window_load(Window *window) {
    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(window_layer);
    
    s_confirm_title_layer = text_layer_create(GRect(0, 20, bounds.size.w, 30));
    text_layer_set_font(s_confirm_title_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
    text_layer_set_text_alignment(s_confirm_title_layer, GTextAlignmentCenter);
    text_layer_set_text(s_confirm_title_layer, "Request Sent!");
    text_layer_set_background_color(s_confirm_title_layer, GColorClear);
    #ifdef PBL_COLOR
    text_layer_set_text_color(s_confirm_title_layer, GColorGreen);
    #endif
    layer_add_child(window_layer, text_layer_get_layer(s_confirm_title_layer));
    
    s_confirm_msg_layer = text_layer_create(GRect(10, 60, bounds.size.w - 20, 80));
    text_layer_set_font(s_confirm_msg_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
    text_layer_set_text_alignment(s_confirm_msg_layer, GTextAlignmentCenter);
    text_layer_set_text(s_confirm_msg_layer, "Check your iPhone to confirm.");
    text_layer_set_background_color(s_confirm_msg_layer, GColorClear);
    layer_add_child(window_layer, text_layer_get_layer(s_confirm_msg_layer));
}

static void confirm_window_unload(Window *window) {
    text_layer_destroy(s_confirm_title_layer);
    text_layer_destroy(s_confirm_msg_layer);
}

static void show_confirmation(const char *message) {
    s_confirm_window = window_create();
    window_set_background_color(s_confirm_window, GColorBlack);
    window_set_window_handlers(s_confirm_window, (WindowHandlers) {
        .load = confirm_window_load,
        .unload = confirm_window_unload
    });
    window_stack_push(s_confirm_window, true);
    
    if (message) {
        text_layer_set_text(s_confirm_msg_layer, message);
    }
    
    // Auto-dismiss after 3 seconds
    app_timer_register(3000, (AppTimerCallback)window_stack_pop, s_confirm_window);
}

// ==================== Bolus Entry Window ====================

static void update_bolus_display(void) {
    snprintf(amount_buffer, sizeof(amount_buffer), "%.2f U", bolus_amount);
    text_layer_set_text(s_bolus_amount_layer, amount_buffer);
}

static void bolus_select_click(ClickRecognizerRef recognizer, void *context) {
    // Send bolus request (will require iOS confirmation)
    request_bolus(bolus_amount);
    window_stack_pop(false);
    show_confirmation("Confirm bolus on iPhone");
}

static void bolus_up_click(ClickRecognizerRef recognizer, void *context) {
    if (bolus_amount + BOLUS_STEP <= BOLUS_MAX) {
        bolus_amount += BOLUS_STEP;
        update_bolus_display();
        vibes_short_pulse();
    }
}

static void bolus_down_click(ClickRecognizerRef recognizer, void *context) {
    if (bolus_amount - BOLUS_STEP >= BOLUS_MIN) {
        bolus_amount -= BOLUS_STEP;
        update_bolus_display();
        vibes_short_pulse();
    }
}

static void bolus_click_config(void *context) {
    window_single_click_subscribe(BUTTON_ID_SELECT, bolus_select_click);
    window_single_click_subscribe(BUTTON_ID_UP, bolus_up_click);
    window_single_click_subscribe(BUTTON_ID_DOWN, bolus_down_click);
    window_long_click_subscribe(BUTTON_ID_SELECT, 1000, NULL, bolus_select_click);
}

static void bolus_window_load(Window *window) {
    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(window_layer);
    
    s_bolus_title_layer = text_layer_create(GRect(0, 10, bounds.size.w, 30));
    text_layer_set_font(s_bolus_title_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
    text_layer_set_text_alignment(s_bolus_title_layer, GTextAlignmentCenter);
    text_layer_set_text(s_bolus_title_layer, "Request Bolus");
    text_layer_set_background_color(s_bolus_title_layer, GColorClear);
    layer_add_child(window_layer, text_layer_get_layer(s_bolus_title_layer));
    
    s_bolus_amount_layer = text_layer_create(GRect(0, 50, bounds.size.w, 40));
    text_layer_set_font(s_bolus_amount_layer, fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK));
    text_layer_set_text_alignment(s_bolus_amount_layer, GTextAlignmentCenter);
    text_layer_set_background_color(s_bolus_amount_layer, GColorClear);
    #ifdef PBL_COLOR
    text_layer_set_text_color(s_bolus_amount_layer, GColorCyan);
    #endif
    layer_add_child(window_layer, text_layer_get_layer(s_bolus_amount_layer));
    
    s_bolus_hint_layer = text_layer_create(GRect(10, 100, bounds.size.w - 20, 60));
    text_layer_set_font(s_bolus_hint_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
    text_layer_set_text_alignment(s_bolus_hint_layer, GTextAlignmentCenter);
    text_layer_set_text(s_bolus_hint_layer, "▲▼ to adjust\nSELECT to send\nRequires iPhone confirmation");
    text_layer_set_background_color(s_bolus_hint_layer, GColorClear);
    layer_add_child(window_layer, text_layer_get_layer(s_bolus_hint_layer));
    
    update_bolus_display();
}

static void bolus_window_unload(Window *window) {
    text_layer_destroy(s_bolus_title_layer);
    text_layer_destroy(s_bolus_amount_layer);
    text_layer_destroy(s_bolus_hint_layer);
}

// ==================== Carb Entry Window ====================

static void update_carbs_display(void) {
    snprintf(amount_buffer, sizeof(amount_buffer), "%d g", carbs_amount);
    text_layer_set_text(s_carbs_amount_layer, amount_buffer);
}

static void carbs_select_click(ClickRecognizerRef recognizer, void *context) {
    request_carbs(carbs_amount);
    window_stack_pop(false);
    show_confirmation("Confirm carbs on iPhone");
}

static void carbs_up_click(ClickRecognizerRef recognizer, void *context) {
    if (carbs_amount + CARBS_STEP <= CARBS_MAX) {
        carbs_amount += CARBS_STEP;
        update_carbs_display();
        vibes_short_pulse();
    }
}

static void carbs_down_click(ClickRecognizerRef recognizer, void *context) {
    if (carbs_amount - CARBS_STEP >= CARBS_MIN) {
        carbs_amount -= CARBS_STEP;
        update_carbs_display();
        vibes_short_pulse();
    }
}

static void carbs_click_config(void *context) {
    window_single_click_subscribe(BUTTON_ID_SELECT, carbs_select_click);
    window_single_click_subscribe(BUTTON_ID_UP, carbs_up_click);
    window_single_click_subscribe(BUTTON_ID_DOWN, carbs_down_click);
}

static void carbs_window_load(Window *window) {
    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(window_layer);
    
    s_carbs_title_layer = text_layer_create(GRect(0, 10, bounds.size.w, 30));
    text_layer_set_font(s_carbs_title_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
    text_layer_set_text_alignment(s_carbs_title_layer, GTextAlignmentCenter);
    text_layer_set_text(s_carbs_title_layer, "Log Carbs");
    text_layer_set_background_color(s_carbs_title_layer, GColorClear);
    layer_add_child(window_layer, text_layer_get_layer(s_carbs_title_layer));
    
    s_carbs_amount_layer = text_layer_create(GRect(0, 50, bounds.size.w, 40));
    text_layer_set_font(s_carbs_amount_layer, fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK));
    text_layer_set_text_alignment(s_carbs_amount_layer, GTextAlignmentCenter);
    text_layer_set_background_color(s_carbs_amount_layer, GColorClear);
    #ifdef PBL_COLOR
    text_layer_set_text_color(s_carbs_amount_layer, GColorOrange);
    #endif
    layer_add_child(window_layer, text_layer_get_layer(s_carbs_amount_layer));
    
    s_carbs_hint_layer = text_layer_create(GRect(10, 100, bounds.size.w - 20, 60));
    text_layer_set_font(s_carbs_hint_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
    text_layer_set_text_alignment(s_carbs_hint_layer, GTextAlignmentCenter);
    text_layer_set_text(s_carbs_hint_layer, "▲▼ to adjust\nSELECT to send\nRequires iPhone confirmation");
    text_layer_set_background_color(s_carbs_hint_layer, GColorClear);
    layer_add_child(window_layer, text_layer_get_layer(s_carbs_hint_layer));
    
    update_carbs_display();
}

static void carbs_window_unload(Window *window) {
    text_layer_destroy(s_carbs_title_layer);
    text_layer_destroy(s_carbs_amount_layer);
    text_layer_destroy(s_carbs_hint_layer);
}

// ==================== Command Menu ====================

static void menu_bolus_callback(int index, void *ctx) {
    bolus_amount = 0.5;  // Reset to default
    s_bolus_window = window_create();
    window_set_background_color(s_bolus_window, GColorBlack);
    window_set_click_config_provider(s_bolus_window, bolus_click_config);
    window_set_window_handlers(s_bolus_window, (WindowHandlers) {
        .load = bolus_window_load,
        .unload = bolus_window_unload
    });
    window_stack_push(s_bolus_window, true);
}

static void menu_carbs_callback(int index, void *ctx) {
    carbs_amount = 10;  // Reset to default
    s_carbs_window = window_create();
    window_set_background_color(s_carbs_window, GColorBlack);
    window_set_click_config_provider(s_carbs_window, carbs_click_config);
    window_set_window_handlers(s_carbs_window, (WindowHandlers) {
        .load = carbs_window_load,
        .unload = carbs_window_unload
    });
    window_stack_push(s_carbs_window, true);
}

static void menu_window_load(Window *window) {
    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(window_layer);
    
    // Load icons
    s_icon_bolus = gbitmap_create_with_resource(IMAGE_BOLUS);
    s_icon_carbs = gbitmap_create_with_resource(IMAGE_CARBS);
    
    s_menu_items[0] = (SimpleMenuItem){
        .title = "Request Bolus",
        .icon = s_icon_bolus,
        .callback = menu_bolus_callback,
    };
    s_menu_items[1] = (SimpleMenuItem){
        .title = "Log Carbs",
        .icon = s_icon_carbs,
        .callback = menu_carbs_callback,
    };
    
    s_menu_section = (SimpleMenuSection){
        .items = s_menu_items,
        .num_items = 2,
    };
    
    s_menu_layer = simple_menu_layer_create(bounds, window, &s_menu_section, 1, NULL);
    layer_add_child(window_layer, simple_menu_layer_get_layer(s_menu_layer));
}

static void menu_window_unload(Window *window) {
    simple_menu_layer_destroy(s_menu_layer);
}

// ==================== Main Window ====================

static void main_select_click(ClickRecognizerRef recognizer, void *context) {
    // Open command menu
    s_menu_window = window_create();
    window_set_window_handlers(s_menu_window, (WindowHandlers) {
        .load = menu_window_load,
        .unload = menu_window_unload
    });
    window_stack_push(s_menu_window, true);
}

static void main_click_config(void *context) {
    window_single_click_subscribe(BUTTON_ID_SELECT, main_select_click);
}

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
    // Command status updates
    Tuple *status_tuple = dict_find(iterator, KEY_COMMAND_STATUS);
    if (status_tuple) {
        int status = (int)status_tuple->value->int32;
        Tuple *msg_tuple = dict_find(iterator, KEY_COMMAND_MSG);
        const char *msg = msg_tuple ? msg_tuple->value->cstring : NULL;
        
        if (status == 1) {
            // Pending confirmation
            show_confirmation(msg ? msg : "Check iPhone to confirm");
        } else if (status == -1) {
            // Error
            show_confirmation(msg ? msg : "Request failed");
        }
        return;
    }
    
    // CGM data updates
    Tuple *glucose_tuple = dict_find(iterator, KEY_GLUCOSE);
    if (glucose_tuple) {
        int glucose = (int)glucose_tuple->value->int32;
        snprintf(glucose_buffer, sizeof(glucose_buffer), "%d", glucose);
        text_layer_set_text(s_glucose_layer, glucose_buffer);
        check_alerts(glucose);
        
        #ifdef PBL_COLOR
        if (glucose < LOW_THRESHOLD) {
            text_layer_set_text_color(s_glucose_layer, GColorRed);
        } else if (glucose > HIGH_THRESHOLD) {
            text_layer_set_text_color(s_glucose_layer, GColorOrange);
        } else {
            text_layer_set_text_color(s_glucose_layer, GColorGreen);
        }
        #endif
    }
    
    Tuple *trend_tuple = dict_find(iterator, KEY_TREND);
    if (trend_tuple) {
        snprintf(trend_buffer, sizeof(trend_buffer), "%s", trend_tuple->value->cstring);
        text_layer_set_text(s_trend_layer, trend_buffer);
    }
    
    Tuple *iob_tuple = dict_find(iterator, KEY_IOB);
    if (iob_tuple) {
        int iob = (int)iob_tuple->value->int32;
        snprintf(iob_buffer, sizeof(iob_buffer), "IOB: %d.%dU", iob / 10, abs(iob % 10));
        text_layer_set_text(s_iob_layer, iob_buffer);
    }
    
    Tuple *loop_tuple = dict_find(iterator, KEY_IS_CLOSED_LOOP);
    if (loop_tuple) {
        bool is_closed = loop_tuple->value->int32 > 0;
        snprintf(status_buffer, sizeof(status_buffer), "%s", is_closed ? "Loop: ON" : "Loop: OFF");
        text_layer_set_text(s_status_layer, status_buffer);
        
        #ifdef PBL_COLOR
        text_layer_set_text_color(s_status_layer, is_closed ? GColorGreen : GColorRed);
        #endif
    }
    
    update_time();
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped: %d", reason);
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed: %d", reason);
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Outbox send success");
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
    update_time();
    if (tick_time->tm_min % 5 == 0) {
        request_data();
    }
}

static void main_window_load(Window *window) {
    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(window_layer);
    
    // Time (top)
    s_time_layer = text_layer_create(GRect(0, 0, bounds.size.w, 24));
    text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
    text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
    text_layer_set_background_color(s_time_layer, GColorClear);
    layer_add_child(window_layer, text_layer_get_layer(s_time_layer));
    
    // Glucose (large)
    s_glucose_layer = text_layer_create(GRect(0, 28, bounds.size.w, 40));
    text_layer_set_font(s_glucose_layer, fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK));
    text_layer_set_text_alignment(s_glucose_layer, GTextAlignmentCenter);
    text_layer_set_background_color(s_glucose_layer, GColorClear);
    text_layer_set_text(s_glucose_layer, "---");
    layer_add_child(window_layer, text_layer_get_layer(s_glucose_layer));
    
    // Trend
    s_trend_layer = text_layer_create(GRect(0, 72, bounds.size.w, 30));
    text_layer_set_font(s_trend_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
    text_layer_set_text_alignment(s_trend_layer, GTextAlignmentCenter);
    text_layer_set_background_color(s_trend_layer, GColorClear);
    layer_add_child(window_layer, text_layer_get_layer(s_trend_layer));
    
    // IOB
    s_iob_layer = text_layer_create(GRect(0, 108, bounds.size.w, 24));
    text_layer_set_font(s_iob_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
    text_layer_set_text_alignment(s_iob_layer, GTextAlignmentCenter);
    text_layer_set_background_color(s_iob_layer, GColorClear);
    text_layer_set_text(s_iob_layer, "IOB: --");
    layer_add_child(window_layer, text_layer_get_layer(s_iob_layer));
    
    // Loop status
    s_status_layer = text_layer_create(GRect(0, 136, bounds.size.w, 24));
    text_layer_set_font(s_status_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
    text_layer_set_text_alignment(s_status_layer, GTextAlignmentCenter);
    text_layer_set_background_color(s_status_layer, GColorClear);
    text_layer_set_text(s_status_layer, "Loop: --");
    layer_add_child(window_layer, text_layer_get_layer(s_status_layer));
    
    // Hint
    s_hint_layer = text_layer_create(GRect(0, bounds.size.h - 20, bounds.size.w, 20));
    text_layer_set_font(s_hint_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
    text_layer_set_text_alignment(s_hint_layer, GTextAlignmentCenter);
    text_layer_set_background_color(s_hint_layer, GColorClear);
    text_layer_set_text(s_hint_layer, "SELECT for actions");
    layer_add_child(window_layer, text_layer_get_layer(s_hint_layer));
}

static void main_window_unload(Window *window) {
    text_layer_destroy(s_glucose_layer);
    text_layer_destroy(s_trend_layer);
    text_layer_destroy(s_iob_layer);
    text_layer_destroy(s_status_layer);
    text_layer_destroy(s_time_layer);
    text_layer_destroy(s_hint_layer);
}

// ==================== Init/Deinit ====================

static void init(void) {
    app_message_register_inbox_received(inbox_received_callback);
    app_message_register_inbox_dropped(inbox_dropped_callback);
    app_message_register_outbox_failed(outbox_failed_callback);
    app_message_register_outbox_sent(outbox_sent_callback);
    
    // Larger buffers for command messages
    app_message_open(256, 128);
    
    s_main_window = window_create();
    window_set_background_color(s_main_window, GColorBlack);
    window_set_click_config_provider(s_main_window, main_click_config);
    window_set_window_handlers(s_main_window, (WindowHandlers) {
        .load = main_window_load,
        .unload = main_window_unload
    });
    window_stack_push(s_main_window, true);
    
    tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
    
    request_data();
    update_time();
}

static void deinit(void) {
    // Destroy bitmaps
    gbitmap_destroy(s_icon_bolus);
    gbitmap_destroy(s_icon_carbs);
    gbitmap_destroy(s_icon_alert);
    gbitmap_destroy(s_icon_check);
    gbitmap_destroy(s_icon_reject);
    
    window_destroy(s_main_window);
}

int main(void) {
    init();
    app_event_loop();
    deinit();
}
