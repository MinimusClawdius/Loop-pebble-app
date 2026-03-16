/*
 * Loop CGM Watchface - Multi-Theme Edition v3.1
 * 
 * 8 selectable themes including Aurora, Ocean, Neon, Matrix, etc.
 * Settings accessible via Pebble app configuration
 * Fixed: No external macros - all ternary inline
 */

#include <pebble.h>
#include <stdlib.h>

// ==================== Theme Definitions ====================

typedef enum {
    THEME_AURORA = 0,
    THEME_WHITE_CYAN,
    THEME_DARK_WARM,
    THEME_PASTEL,
    THEME_OCEAN,
    THEME_MATRIX_SUBTLE,
    THEME_MATRIX_BRIGHT,
    THEME_COUNT
} ThemeId;

// Theme colors
typedef struct {
    GColor bg_color;
    GColor bg_color2;        // For gradients
    GColor text_primary;
    GColor text_secondary;
    GColor chart_bg;
    GColor chart_line;
    GColor chart_fill;
    GColor chart_border;
    GColor iob_color;
    GColor loop_on;
    GColor loop_off;
    bool has_matrix;         // Matrix rain effect
    bool has_aurora;         // Aurora waves
    bool has_gradient_bg;    // Gradient background
} Theme;

static const Theme THEMES[THEME_COUNT] = {
    // THEME_AURORA
    {GColorBlack, GColorBlack, GColorFromRGB(0, 255, 136), GColorWhite,
     GColorFromRGB(10, 10, 30), GColorFromRGB(0, 200, 100), GColorFromRGB(0, 50, 30), GColorFromRGB(0, 100, 80),
     GColorFromRGB(0, 255, 255), GColorGreen, GColorRed,
     false, true, false},
    
    // THEME_WHITE_CYAN
    {GColorFromRGB(245, 245, 245), GColorWhite, GColorFromRGB(34, 34, 34), GColorFromRGB(102, 102, 102),
     GColorFromRGB(232, 244, 248), GColorCyan, GColorFromRGB(179, 229, 252), GColorFromRGB(0, 136, 204),
     GColorFromRGB(0, 136, 204), GColorFromRGB(76, 175, 80), GColorRed,
     false, false, false},
    
    // THEME_DARK_WARM
    {GColorFromRGB(26, 26, 46), GColorFromRGB(26, 26, 46), GColorFromRGB(255, 213, 79), GColorFromRGB(170, 170, 170),
     GColorFromRGB(42, 42, 78), GColorFromRGB(255, 138, 101), GColorFromRGB(61, 42, 42), GColorFromRGB(255, 109, 0),
     GColorFromRGB(100, 181, 246), GColorFromRGB(105, 240, 174), GColorRed,
     false, false, false},
    
    // THEME_PASTEL
    {GColorFromRGB(240, 230, 255), GColorWhite, GColorFromRGB(92, 107, 192), GColorFromRGB(136, 136, 136),
     GColorWhite, GColorFromRGB(124, 77, 255), GColorFromRGB(225, 190, 231), GColorFromRGB(156, 39, 176),
     GColorFromRGB(124, 77, 255), GColorFromRGB(102, 187, 106), GColorRed,
     false, false, false},
    
    // THEME_OCEAN
    {GColorFromRGB(227, 242, 253), GColorWhite, GColorFromRGB(21, 101, 192), GColorFromRGB(144, 164, 174),
     GColorWhite, GColorFromRGB(2, 136, 209), GColorFromRGB(179, 229, 252), GColorFromRGB(1, 87, 155),
     GColorFromRGB(2, 119, 189), GColorFromRGB(0, 200, 83), GColorRed,
     false, false, false},
    
    // THEME_MATRIX_SUBTLE
    {GColorBlack, GColorBlack, GColorFromRGB(0, 200, 0), GColorFromRGB(0, 100, 0),
     GColorFromRGB(0, 10, 0), GColorFromRGB(0, 200, 0), GColorFromRGB(0, 40, 0), GColorFromRGB(0, 100, 0),
     GColorFromRGB(0, 200, 0), GColorFromRGB(0, 255, 0), GColorRed,
     true, false, false},
    
    // THEME_MATRIX_BRIGHT
    {GColorBlack, GColorBlack, GColorFromRGB(0, 255, 68), GColorFromRGB(0, 255, 0),
     GColorFromRGB(0, 20, 0), GColorFromRGB(0, 255, 68), GColorFromRGB(0, 100, 0), GColorFromRGB(0, 255, 0),
     GColorFromRGB(0, 255, 136), GColorFromRGB(0, 255, 0), GColorRed,
     true, false, false}
};

// ==================== Display Layers ====================

static Window *s_main_window;
static Layer *s_canvas_layer;
static Layer *s_battery_layer;

// ==================== Data Storage ====================

static ThemeId s_current_theme = THEME_MATRIX_BRIGHT;
static int current_bg = 125;
static time_t last_reading_time = 0;
static bool loop_is_active = true;
static int current_iob = 25;
static bool has_data = true;
static uint8_t current_trend = 4;
static int battery_level = 85;

#define CHART_POINTS 9
static int chart_x[] = {10, 25, 40, 55, 70, 85, 100, 115, 130};
static int chart_y[] = {115, 108, 112, 105, 100, 108, 112, 108, 105};
static int chart_y_base = 90;
static int chart_y_range = 40;

#define DEMO_MODE true

// ==================== Matrix Rain ====================

#define MATRIX_COLS 14
#define MATRIX_ROWS 14
#define CHAR_WIDTH 10
#define CHAR_HEIGHT 12

typedef struct {
    int y_offset;
    int speed;
    int length;
    int brightness;
    char chars[MATRIX_ROWS];
} MatrixColumn;

static MatrixColumn s_columns[MATRIX_COLS];
static AppTimer *s_animation_timer;
static int s_frame = 0;
#define ANIM_INTERVAL 80

static void matrix_init(void) {
    for (int i = 0; i < MATRIX_COLS; i++) {
        s_columns[i].y_offset = -(rand() % 100);
        s_columns[i].speed = 1 + rand() % 3;
        s_columns[i].length = 4 + rand() % 8;
        s_columns[i].brightness = 80 + rand() % 175;
        for (int j = 0; j < MATRIX_ROWS; j++) {
            s_columns[i].chars[j] = rand() % 10;
        }
    }
}

static void matrix_update(void) {
    for (int i = 0; i < MATRIX_COLS; i++) {
        s_columns[i].y_offset += s_columns[i].speed;
        if (s_columns[i].y_offset > 168 + s_columns[i].length * CHAR_HEIGHT) {
            s_columns[i].y_offset = -(s_columns[i].length * CHAR_HEIGHT);
            s_columns[i].speed = 1 + rand() % 3;
            s_columns[i].brightness = 80 + rand() % 175;
            for (int j = 0; j < MATRIX_ROWS; j++) {
                s_columns[i].chars[j] = rand() % 10;
            }
        }
    }
}

static void matrix_draw(GContext *ctx, bool bright) {
    for (int col = 0; col < MATRIX_COLS; col++) {
        int x = col * CHAR_WIDTH + 2;
        MatrixColumn *mc = &s_columns[col];
        
        for (int row = 0; row < mc->length; row++) {
            int y = mc->y_offset + row * CHAR_HEIGHT;
            if (y < -CHAR_HEIGHT || y > 168) continue;
            
            int b;
            if (row == 0) b = bright ? 255 : 200;
            else if (row < 3) b = (bright ? 200 : 150) - row * 30;
            else b = mc->brightness - row * 15;
            if (b < 30) b = 30;
            
            char buf[2] = {'0' + (mc->chars[row % MATRIX_ROWS] % 10), 0};
            graphics_context_set_text_color(ctx, GColorFromRGB(0, b, 0));
            graphics_draw_text(ctx, buf,
                fonts_get_system_font(FONT_KEY_GOTHIC_14),
                GRect(x, y, CHAR_WIDTH, CHAR_HEIGHT),
                GTextOverflowModeFill, GTextAlignmentLeft, NULL);
        }
    }
}

// ==================== Aurora Drawing ====================

static void aurora_draw(GContext *ctx) {
    for (int wave = 0; wave < 5; wave++) {
        int base_y = 25 + wave * 12;
        for (int x = 0; x < 144; x += 3) {
            int y = base_y + (int)(8 * sin_lookup(TRIG_MAX_ANGLE * x / 200 + wave * 50) / TRIG_MAX_RATIO);
            int alpha = 80 - wave * 12;
            int b_val = (alpha + 80) < 255 ? (alpha + 80) : 255;
            GColor color = GColorFromRGB(0, alpha, b_val);
            graphics_context_set_fill_color(ctx, color);
            graphics_fill_rect(ctx, GRect(x, y, 4, 3), 0, GCornerNone);
        }
    }
    
    // Stars
    graphics_context_set_fill_color(ctx, GColorWhite);
    static GPoint stars[20];
    static bool stars_init = false;
    if (!stars_init) {
        for (int i = 0; i < 20; i++) {
            stars[i] = GPoint(rand() % 144, rand() % 60);
        }
        stars_init = true;
    }
    for (int i = 0; i < 20; i++) {
        graphics_fill_rect(ctx, GRect(stars[i].x, stars[i].y, 1, 1), 0, GCornerNone);
    }
}

// ==================== Main Canvas Drawing ====================

static void canvas_update_proc(Layer *layer, GContext *ctx) {
    const Theme *theme = &THEMES[s_current_theme];
    GRect bounds = layer_get_bounds(layer);
    
    // Background
    if (theme->has_matrix) {
        graphics_context_set_fill_color(ctx, GColorBlack);
        graphics_fill_rect(ctx, bounds, 0, GCornerNone);
        matrix_draw(ctx, s_current_theme == THEME_MATRIX_BRIGHT);
        // Semi-transparent overlay
        for (int y = 0; y < 168; y += 2) {
            graphics_context_set_stroke_color(ctx, GColorFromRGBA(0, 0, 0, 180));
            graphics_draw_line(ctx, GPoint(0, y), GPoint(144, y));
        }
    } else if (theme->has_aurora) {
        graphics_context_set_fill_color(ctx, GColorBlack);
        graphics_fill_rect(ctx, bounds, 0, GCornerNone);
        aurora_draw(ctx);
    } else if (s_current_theme == THEME_OCEAN) {
        // Ocean gradient
        for (int y = 0; y < 168; y++) {
            int b = 230 - y * 30 / 168;
            graphics_context_set_stroke_color(ctx, GColorFromRGB(200, 230, b));
            graphics_draw_line(ctx, GPoint(0, y), GPoint(144, y));
        }
    } else {
        graphics_context_set_fill_color(ctx, theme->bg_color);
        graphics_fill_rect(ctx, bounds, 0, GCornerNone);
    }
    
    // Time
    time_t temp = time(NULL);
    struct tm *tick = localtime(&temp);
    char time_buf[8];
    strftime(time_buf, sizeof(time_buf), "%H:%M", tick);
    graphics_context_set_text_color(ctx, theme->text_primary);
    graphics_draw_text(ctx, time_buf, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
        GRect(0, 3, 144, 20), GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    
    // BG number with glow for dark themes
    char bg_buf[8];
    snprintf(bg_buf, sizeof(bg_buf), "%d", current_bg);
    
    if (s_current_theme >= THEME_MATRIX_SUBTLE) {
        // Glow effect
        for (int i = 3; i > 0; i--) {
            graphics_context_set_text_color(ctx, GColorFromRGB(0, 20*i, 0));
            graphics_draw_text(ctx, bg_buf, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD),
                GRect(-1, 20, 148, 50), GTextOverflowModeFill, GTextAlignmentCenter, NULL);
        }
    }
    
    // BG color based on value
    GColor bg_text_color = theme->text_primary;
    if (current_bg < 70) bg_text_color = GColorRed;
    else if (current_bg > 180) bg_text_color = GColorOrange;
    
    graphics_context_set_text_color(ctx, bg_text_color);
    graphics_draw_text(ctx, bg_buf, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD),
        GRect(0, 20, 144, 50), GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    
    // Trend
    const char* trends[] = {"", "^", "^", "/", "-", "\\", "v", "v"};
    const char* trend = (current_trend > 0 && current_trend < 8) ? trends[current_trend] : "";
    graphics_context_set_text_color(ctx, theme->chart_line);
    graphics_draw_text(ctx, trend, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
        GRect(100, 30, 40, 30), GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    
    // Chart
    int ct = 75, ch = 45, cb = ct + ch;
    graphics_context_set_fill_color(ctx, theme->chart_bg);
    graphics_fill_rect(ctx, GRect(5, ct, 134, ch), 0, GCornerNone);
    graphics_context_set_stroke_color(ctx, theme->chart_border);
    graphics_draw_rect(ctx, GRect(5, ct, 134, ch));
    
    for (int i = 0; i < CHART_POINTS - 1; i++) {
        int y1 = ct + (chart_y[i] - chart_y_base) * ch / chart_y_range;
        int y2 = ct + (chart_y[i+1] - chart_y_base) * ch / chart_y_range;
        y1 = y1 < ct ? ct : (y1 > cb ? cb : y1);
        y2 = y2 < ct ? ct : (y2 > cb ? cb : y2);
        graphics_context_set_stroke_color(ctx, theme->chart_line);
        graphics_context_set_stroke_width(ctx, 2);
        graphics_draw_line(ctx, GPoint(chart_x[i], y1), GPoint(chart_x[i+1], y2));
    }
    
    // Current dot with pulse
    int lx = chart_x[CHART_POINTS-1], ly = ct + (chart_y[CHART_POINTS-1] - chart_y_base) * ch / chart_y_range;
    ly = ly < ct ? ct : (ly > cb ? cb : ly);
    int pulse = 8 + (s_frame % 10);
    graphics_context_set_stroke_color(ctx, theme->chart_line);
    graphics_draw_circle(ctx, GPoint(lx, ly), pulse);
    graphics_context_set_fill_color(ctx, theme->chart_line);
    graphics_fill_circle(ctx, GPoint(lx, ly), 5);
    
    // IOB
    char iob_buf[16];
    snprintf(iob_buf, sizeof(iob_buf), "IOB %d.%dU", current_iob/10, current_iob%10);
    graphics_context_set_text_color(ctx, theme->iob_color);
    graphics_draw_text(ctx, iob_buf, fonts_get_system_font(FONT_KEY_GOTHIC_14),
        GRect(0, 126, 144, 16), GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    
    // Loop status
    const char* loop_text;
    if (s_current_theme >= THEME_MATRIX_SUBTLE) {
        loop_text = loop_is_active ? ">>> LOOPING <<<" : ">>> OPEN LOOP <<<";
    } else {
        loop_text = loop_is_active ? "● LOOP ACTIVE" : "○ OPEN LOOP";
    }
    graphics_context_set_text_color(ctx, loop_is_active ? theme->loop_on : theme->loop_off);
    graphics_draw_text(ctx, loop_text, fonts_get_system_font(FONT_KEY_GOTHIC_14),
        GRect(0, 144, 144, 16), GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    
    // Time ago
    char ago_buf[20];
    if (last_reading_time > 0) {
        int mins = (int)(time(NULL) - last_reading_time) / 60;
        if (s_current_theme >= THEME_MATRIX_SUBTLE) {
            snprintf(ago_buf, sizeof(ago_buf), "SYS: %dm AGO", mins);
        } else {
            snprintf(ago_buf, sizeof(ago_buf), "%d min ago", mins);
        }
    } else {
        snprintf(ago_buf, sizeof(ago_buf), s_current_theme >= THEME_MATRIX_SUBTLE ? "SYS: LIVE" : "just now");
    }
    graphics_context_set_text_color(ctx, theme->text_secondary);
    graphics_draw_text(ctx, ago_buf, fonts_get_system_font(FONT_KEY_GOTHIC_14),
        GRect(0, 160, 144, 16), GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

// ==================== Battery ====================

static void battery_update_proc(Layer *layer, GContext *ctx) {
    const Theme *theme = &THEMES[s_current_theme];
    GColor bat_color = battery_level <= 20 ? GColorRed : theme->loop_on;
    
    graphics_context_set_stroke_color(ctx, bat_color);
    graphics_draw_round_rect(ctx, GRect(0, 4, 20, 10), 1);
    graphics_context_set_fill_color(ctx, bat_color);
    graphics_fill_rect(ctx, GRect(20, 6, 2, 4), 0, GCornerNone);
    
    int fill_w = 18 * battery_level / 100;
    if (fill_w > 0) {
        graphics_fill_rect(ctx, GRect(1, 5, fill_w, 8), 0, GCornerNone);
    }
}

// ==================== Settings ====================

#define SETTINGS_KEY 1

static void save_settings(void) {
    persist_write_int(SETTINGS_KEY, s_current_theme);
}

static void load_settings(void) {
    if (persist_exists(SETTINGS_KEY)) {
        s_current_theme = persist_read_int(SETTINGS_KEY);
        if (s_current_theme >= THEME_COUNT) s_current_theme = THEME_MATRIX_BRIGHT;
    }
}

// ==================== AppMessage ====================

#define KEY_GLUCOSE 0
#define KEY_TREND 1
#define KEY_IOB 2
#define KEY_IS_CLOSED_LOOP 3
#define KEY_COB 4
#define KEY_BATTERY 5
#define KEY_GLUCOSE_DATE 6
#define KEY_THEME 8
#define KEY_REQUEST_DATA 9

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
    has_data = true;
    last_reading_time = time(NULL);
    
    Tuple *t;
    
    t = dict_find(iterator, KEY_THEME);
    if (t) {
        s_current_theme = t->value->int32;
        if (s_current_theme >= THEME_COUNT) s_current_theme = THEME_MATRIX_BRIGHT;
        save_settings();
    }
    
    t = dict_find(iterator, KEY_GLUCOSE);
    if (t) {
        current_bg = t->value->int32;
        for (int i = 0; i < CHART_POINTS - 1; i++) chart_y[i] = chart_y[i+1];
        chart_y[CHART_POINTS-1] = current_bg;
    }
    
    t = dict_find(iterator, KEY_TREND);
    if (t) current_trend = t->value->uint8;
    
    t = dict_find(iterator, KEY_IOB);
    if (t) current_iob = t->value->int32;
    
    t = dict_find(iterator, KEY_IS_CLOSED_LOOP);
    if (t) loop_is_active = t->value->int32 > 0;
    
    t = dict_find(iterator, KEY_BATTERY);
    if (t) battery_level = t->value->int32;
    
    layer_mark_dirty(s_canvas_layer);
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {}

// ==================== Animation Timer ====================

static void anim_timer_callback(void *data) {
    if (THEMES[s_current_theme].has_matrix) {
        matrix_update();
    }
    s_frame++;
    layer_mark_dirty(s_canvas_layer);
    s_animation_timer = app_timer_register(ANIM_INTERVAL, anim_timer_callback, NULL);
}

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

// ==================== Window ====================

static void main_window_load(Window *window) {
    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(window_layer);
    
    window_set_background_color(window, GColorBlack);
    
    s_canvas_layer = layer_create(bounds);
    layer_set_update_proc(s_canvas_layer, canvas_update_proc);
    layer_add_child(window_layer, s_canvas_layer);
    
    s_battery_layer = layer_create(GRect(110, 5, 28, 18));
    layer_set_update_proc(s_battery_layer, battery_update_proc);
    layer_add_child(window_layer, s_battery_layer);
    
    matrix_init();
    
    #if DEMO_MODE
    has_data = true;
    current_bg = 125;
    current_trend = 4;
    current_iob = 25;
    loop_is_active = true;
    last_reading_time = time(NULL) - 120;
    #endif
    
    s_animation_timer = app_timer_register(ANIM_INTERVAL, anim_timer_callback, NULL);
}

static void main_window_unload(Window *window) {
    if (s_animation_timer) app_timer_cancel(s_animation_timer);
    layer_destroy(s_canvas_layer);
    layer_destroy(s_battery_layer);
}

static void init(void) {
    srand(time(NULL));
    load_settings();
    
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
