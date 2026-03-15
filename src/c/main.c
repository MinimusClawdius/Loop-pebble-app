/*
 * Loop CGM Watchface - Matrix Edition
 * 
 * Matrix rain background with cascading kanji
 * Large green BG, animated chart, glow effects
 */

#include <pebble.h>
#include <stdlib.h>

// ==================== Display Layers ====================

static Window *s_main_window;
static Layer *s_canvas_layer;        // Main drawing canvas
static Layer *s_battery_layer;

// Text layers (drawn on canvas, these for reference)
static TextLayer *s_time_layer;
static TextLayer *s_setup_layer;

// ==================== Data Storage ====================

static int current_bg = 125;
static int current_bg_delta = 3;
static time_t last_reading_time = 0;
static bool loop_is_active = true;
static int current_iob = 25;  // 2.5U * 10
static bool has_data = true;
static uint8_t current_trend = 4;  // Flat
static int battery_level = 85;
static bool battery_charging = false;

// Chart data
#define CHART_POINTS 9
static int chart_x[] = {10, 25, 40, 55, 70, 85, 100, 115, 130};
static int chart_y[] = {115, 108, 112, 105, 100, 108, 112, 108, 105};
static int chart_y_base = 90;
static int chart_y_range = 40;

// Demo mode
#define DEMO_MODE true

// ==================== Matrix Rain ====================

#define MATRIX_COLS 14  // Number of character columns
#define MATRIX_ROWS 14  // Characters per column
#define CHAR_WIDTH 10
#define CHAR_HEIGHT 12

// Matrix characters (katakana + numbers + symbols)
static const char* MATRIX_CHARS[] = {
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J",
    "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T",
    "U", "V", "W", "X", "Y", "Z",
    "\xEF\xBD\xB1", "\xEF\xBD\xB2", "\xEF\xBD\xB3", "\xEF\xBD\xB4", "\xEF\xBD\xB5",
    "\xEF\xBD\xB6", "\xEF\xBD\xB7", "\xEF\xBD\xB8", "\xEF\xBD\xB9", "\xEF\xBD\xBA",
    "\xEF\xBD\xBB", "\xEF\xBD\xBC", "\xEF\xBD\xBD", "\xEF\xBD\xBE", "\xEF\xBD\xBF",
    "+", "-", "*", "/", "=", ">", "<", "|", "!", "?"
};
#define NUM_MATRIX_CHARS 61

// Each column has: position, speed, brightness
typedef struct {
    int y_offset;      // Current Y position (top of stream)
    int speed;         // Pixels per frame
    int length;        // Number of characters in stream
    int brightness;    // 0-255
    char chars[MATRIX_ROWS];  // Characters to display
} MatrixColumn;

static MatrixColumn s_columns[MATRIX_COLS];
static AppTimer *s_matrix_timer;
static int s_matrix_frame = 0;
#define MATRIX_INTERVAL 80  // ms between frames

// Initialize matrix columns
static void matrix_init(void) {
    for (int i = 0; i < MATRIX_COLS; i++) {
        s_columns[i].y_offset = -(rand() % 100);  // Start above screen
        s_columns[i].speed = 1 + rand() % 3;
        s_columns[i].length = 4 + rand() % 8;
        s_columns[i].brightness = 80 + rand() % 175;
        
        // Randomize characters
        for (int j = 0; j < MATRIX_ROWS; j++) {
            s_columns[i].chars[j] = rand() % NUM_MATRIX_CHARS;
        }
    }
}

// Update matrix animation
static void matrix_update(void) {
    for (int i = 0; i < MATRIX_COLS; i++) {
        s_columns[i].y_offset += s_columns[i].speed;
        
        // Reset when fully off screen
        if (s_columns[i].y_offset > 168 + s_columns[i].length * CHAR_HEIGHT) {
            s_columns[i].y_offset = -(s_columns[i].length * CHAR_HEIGHT);
            s_columns[i].speed = 1 + rand() % 3;
            s_columns[i].brightness = 80 + rand() % 175;
            
            // New random characters
            for (int j = 0; j < MATRIX_ROWS; j++) {
                s_columns[i].chars[j] = rand() % NUM_MATRIX_CHARS;
            }
        }
    }
    s_matrix_frame++;
}

// Draw matrix background
static void matrix_draw(GContext *ctx, int alpha) {
    // Black background
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_rect(ctx, GRect(0, 0, 144, 168), 0, GCornerNone);
    
    graphics_context_set_text_color(ctx, GColorFromRGB(0, 80, 0));
    
    for (int col = 0; col < MATRIX_COLS; col++) {
        int x = col * CHAR_WIDTH + 2;
        MatrixColumn *mc = &s_columns[col];
        
        for (int row = 0; row < mc->length; row++) {
            int y = mc->y_offset + row * CHAR_HEIGHT;
            
            if (y < -CHAR_HEIGHT || y > 168) continue;
            
            // Brightness decreases down the stream
            int bright;
            if (row == 0) {
                bright = 255;  // Head is bright
            } else if (row < 3) {
                bright = 200 - row * 30;
            } else {
                bright = mc->brightness - row * 15;
            }
            if (bright < 30) bright = 30;
            bright = bright * alpha / 255;
            
            // Draw character (simplified - using single chars)
            char buf[2] = {0};
            buf[0] = '0' + (mc->chars[row % MATRIX_ROWS] % 10);
            
            graphics_context_set_text_color(ctx, GColorFromRGB(0, bright, 0));
            graphics_draw_text(ctx, buf, 
                fonts_get_system_font(FONT_KEY_GOTHIC_14),
                GRect(x, y, CHAR_WIDTH, CHAR_HEIGHT),
                GTextOverflowModeFill, GTextAlignmentLeft, NULL);
        }
    }
}

// ==================== Main Canvas Drawing ====================

static void canvas_update_proc(Layer *layer, GContext *ctx) {
    GRect bounds = layer_get_bounds(layer);
    
    // Draw Matrix background (reduced brightness)
    matrix_draw(ctx, 100);  // 100/255 = ~40% brightness
    
    // Semi-transparent dark overlay for readability
    graphics_context_set_fill_color(ctx, GColorFromRGBA(0, 0, 0, 180));
    graphics_fill_rect(ctx, GRect(0, 0, 144, 168), 0, GCornerNone);
    
    // Time at top
    time_t temp = time(NULL);
    struct tm *tick_time = localtime(&temp);
    static char time_buf[8];
    strftime(time_buf, sizeof(time_buf), "%H:%M", time_buf);
    strftime(time_buf, sizeof(time_buf), "%H:%M", tick_time);
    
    graphics_context_set_text_color(ctx, GColorFromRGB(0, 255, 0));
    graphics_draw_text(ctx, time_buf,
        fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
        GRect(0, 3, 144, 20),
        GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    
    // BG number with glow effect
    static char bg_buf[8];
    snprintf(bg_buf, sizeof(bg_buf), "%d", current_bg);
    
    // Glow layers
    for (int i = 3; i > 0; i--) {
        int glow = 20 * i;
        graphics_context_set_text_color(ctx, GColorFromRGB(0, glow, 0));
        graphics_draw_text(ctx, bg_buf,
            fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD),
            GRect(-1, 20, 148, 50),
            GTextOverflowModeFill, GTextAlignmentCenter, NULL);
        graphics_draw_text(ctx, bg_buf,
            fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD),
            GRect(1, 20, 148, 50),
            GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    }
    
    // Main BG text
    graphics_context_set_text_color(ctx, GColorFromRGB(0, 255, 68));
    graphics_draw_text(ctx, bg_buf,
        fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD),
        GRect(0, 20, 144, 50),
        GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    
    // Trend arrow
    const char* trends[] = {"", "^^", "^", "/", "-", "\\", "v", "vv"};
    const char* trend = (current_trend > 0 && current_trend < 8) ? trends[current_trend] : "";
    graphics_context_set_text_color(ctx, GColorFromRGB(68, 255, 68));
    graphics_draw_text(ctx, trend,
        fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
        GRect(100, 30, 40, 30),
        GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    
    // Chart area
    int chart_top = 75;
    int chart_h = 45;
    int chart_bottom = chart_top + chart_h;
    
    // Chart border with glow
    for (int i = 2; i > 0; i--) {
        graphics_context_set_stroke_color(ctx, GColorFromRGB(0, 40*i, 0));
        graphics_draw_rect(ctx, GRect(5-i, chart_top-i, 134+i*2, chart_h+i*2));
    }
    graphics_context_set_fill_color(ctx, GColorFromRGB(0, 20, 0));
    graphics_fill_rect(ctx, GRect(5, chart_top, 134, chart_h), 0, GCornerNone);
    graphics_context_set_stroke_color(ctx, GColorFromRGB(0, 100, 0));
    graphics_draw_rect(ctx, GRect(5, chart_top, 134, chart_h));
    
    // Chart line with gradient
    for (int i = 0; i < CHART_POINTS - 1; i++) {
        int y1 = chart_top + (chart_y[i] - chart_y_base) * chart_h / chart_y_range;
        int y2 = chart_top + (chart_y[i+1] - chart_y_base) * chart_h / chart_y_range;
        
        // Clamp
        if (y1 < chart_top) y1 = chart_top;
        if (y1 > chart_bottom) y1 = chart_bottom;
        if (y2 < chart_top) y2 = chart_top;
        if (y2 > chart_bottom) y2 = chart_bottom;
        
        // Gradient color
        int g = 180 + 75 * i / CHART_POINTS;
        graphics_context_set_stroke_color(ctx, GColorFromRGB(0, g, 50));
        graphics_context_set_stroke_width(ctx, 2);
        graphics_draw_line(ctx, GPoint(chart_x[i], y1), GPoint(chart_x[i+1], y2));
    }
    
    // Current BG dot with rings (pulsing effect based on frame)
    int last_x = chart_x[CHART_POINTS - 1];
    int last_y = chart_top + (chart_y[CHART_POINTS - 1] - chart_y_base) * chart_h / chart_y_range;
    int pulse = (s_matrix_frame % 10);
    int ring_size = 8 + pulse;
    
    graphics_context_set_stroke_color(ctx, GColorFromRGB(0, 255, 0));
    graphics_draw_circle(ctx, GPoint(last_x, last_y), ring_size);
    graphics_context_set_fill_color(ctx, GColorFromRGB(0, 255, 68));
    graphics_fill_circle(ctx, GPoint(last_x, last_y), 5);
    
    // IOB
    static char iob_buf[16];
    snprintf(iob_buf, sizeof(iob_buf), "IOB %d.%dU", current_iob / 10, current_iob % 10);
    graphics_context_set_text_color(ctx, GColorFromRGB(0, 200, 0));
    graphics_draw_text(ctx, iob_buf,
        fonts_get_system_font(FONT_KEY_GOTHIC_14),
        GRect(0, 126, 144, 16),
        GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    
    // Loop status - Matrix style
    const char* loop_text = loop_is_active ? ">>> LOOPING <<<" : ">>> OPEN LOOP <<<";
    graphics_context_set_text_color(ctx, GColorFromRGB(0, 255, 0));
    graphics_draw_text(ctx, loop_text,
        fonts_get_system_font(FONT_KEY_GOTHIC_14),
        GRect(0, 144, 144, 16),
        GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    
    // Time ago - system timestamp style
    static char ago_buf[20];
    if (last_reading_time > 0) {
        int mins = (int)(time(NULL) - last_reading_time) / 60;
        snprintf(ago_buf, sizeof(ago_buf), "SYS: %dm AGO", mins);
    } else {
        snprintf(ago_buf, sizeof(ago_buf), "SYS: LIVE");
    }
    graphics_context_set_text_color(ctx, GColorFromRGB(0, 100, 0));
    graphics_draw_text(ctx, ago_buf,
        fonts_get_system_font(FONT_KEY_GOTHIC_14),
        GRect(0, 160, 144, 16),
        GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

// ==================== Battery ====================

static void battery_update_proc(Layer *layer, GContext *ctx) {
    int bat_w = 20, bat_h = 10, tip_w = 2, tip_h = 4;
    int x = 0, y = 4;
    
    graphics_context_set_stroke_color(ctx, GColorFromRGB(0, 200, 0));
    graphics_draw_round_rect(ctx, GRect(x, y, bat_w, bat_h), 1);
    graphics_context_set_fill_color(ctx, GColorFromRGB(0, 200, 0));
    graphics_fill_rect(ctx, GRect(x + bat_w, y + (bat_h - tip_h) / 2, tip_w, tip_h), 0, GCornerNone);
    
    int fill_w = (bat_w - 2) * battery_level / 100;
    if (fill_w > 0) {
        GColor color = battery_level <= 20 ? GColorRed : GColorFromRGB(0, 255, 0);
        graphics_context_set_fill_color(ctx, color);
        graphics_fill_rect(ctx, GRect(x + 1, y + 1, fill_w, bat_h - 2), 0, GCornerNone);
    }
}

// ==================== Animation Timer ====================

static void matrix_timer_callback(void *data) {
    matrix_update();
    layer_mark_dirty(s_canvas_layer);
    s_matrix_timer = app_timer_register(MATRIX_INTERVAL, matrix_timer_callback, NULL);
}

// ==================== AppMessage ====================

#define KEY_GLUCOSE 0
#define KEY_TREND 1
#define KEY_IOB 2
#define KEY_IS_CLOSED_LOOP 3
#define KEY_COB 4
#define KEY_BATTERY 5
#define KEY_GLUCOSE_DATE 6
#define KEY_REQUEST_DATA 9

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
    has_data = true;
    last_reading_time = time(NULL);
    
    Tuple *glucose_tuple = dict_find(iterator, KEY_GLUCOSE);
    if (glucose_tuple) {
        current_bg = (int)glucose_tuple->value->int32;
        // Shift chart left and add new point
        for (int i = 0; i < CHART_POINTS - 1; i++) {
            chart_y[i] = chart_y[i + 1];
        }
        chart_y[CHART_POINTS - 1] = current_bg;
    }
    
    Tuple *trend_tuple = dict_find(iterator, KEY_TREND);
    if (trend_tuple) {
        current_trend = (uint8_t)trend_tuple->value->uint8;
    }
    
    Tuple *iob_tuple = dict_find(iterator, KEY_IOB);
    if (iob_tuple) {
        current_iob = (int)iob_tuple->value->int32;
    }
    
    Tuple *loop_tuple = dict_find(iterator, KEY_IS_CLOSED_LOOP);
    if (loop_tuple) {
        loop_is_active = loop_tuple->value->int32 > 0;
    }
    
    Tuple *battery_tuple = dict_find(iterator, KEY_BATTERY);
    if (battery_tuple) {
        battery_level = (int)battery_tuple->value->int32;
    }
    
    layer_mark_dirty(s_canvas_layer);
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {}

// ==================== Tick Handler ====================

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
    layer_mark_dirty(s_canvas_layer);
    
    if (tick_time->tm_min % 5 == 0) {
        DictionaryIterator *iter;
        if (app_message_outbox_begin(&iter) == APP_MSG_OK) {
            dict_write_uint8(iter, KEY_REQUEST_DATA, 1);
            app_message_outbox_send();
        }
    }
}

// ==================== Window Setup ====================

static void main_window_load(Window *window) {
    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(window_layer);
    
    window_set_background_color(window, GColorBlack);
    
    // Main canvas for everything
    s_canvas_layer = layer_create(bounds);
    layer_set_update_proc(s_canvas_layer, canvas_update_proc);
    layer_add_child(window_layer, s_canvas_layer);
    
    // Battery indicator (top right)
    s_battery_layer = layer_create(GRect(110, 5, 28, 18));
    layer_set_update_proc(s_battery_layer, battery_update_proc);
    layer_add_child(window_layer, s_battery_layer);
    
    // Initialize matrix
    matrix_init();
    
    #if DEMO_MODE
    has_data = true;
    current_bg = 125;
    current_trend = 4;
    current_iob = 25;
    loop_is_active = true;
    last_reading_time = time(NULL) - 120;
    #endif
    
    // Start matrix animation
    s_matrix_timer = app_timer_register(MATRIX_INTERVAL, matrix_timer_callback, NULL);
}

static void main_window_unload(Window *window) {
    if (s_matrix_timer) {
        app_timer_cancel(s_matrix_timer);
    }
    layer_destroy(s_canvas_layer);
    layer_destroy(s_battery_layer);
}

// ==================== Init/Deinit ====================

static void init(void) {
    srand(time(NULL));
    
    app_message_register_inbox_received(inbox_received_callback);
    app_message_register_inbox_dropped(inbox_dropped_callback);
    app_message_open(512, 256);
    
    s_main_window = window_create();
    window_set_window_handlers(s_main_window, (WindowHandlers) {
        .load = main_window_load,
        .unload = main_window_unload
    });
    window_stack_push(s_main_window, true);
    
    tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
}

static void deinit(void) {
    tick_timer_service_unsubscribe();
    window_destroy(s_main_window);
}

int main(void) {
    init();
    app_event_loop();
    deinit();
}
