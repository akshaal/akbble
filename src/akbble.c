#include <pebble.h>

#define TOTAL_IMAGE_SLOTS 3
#define NUMBER_OF_IMAGES 12

static Window *s_window;

static const char *day_names[] = {
    "SØN", "MAN", "TIR", "ONS", "TOR", "FRE", "LØR"
};

static const char *month_names[] = {
    "JAN", "FEB", "MAR", "APR", "MAI", "JUN", "JUL", "AUG", "SEP", "OKT", "NOV", "DES"
};

static const int IMAGE_RESOURCE_D_IDS[NUMBER_OF_IMAGES] = {
    RESOURCE_ID_IMAGE_NUM_0_d, RESOURCE_ID_IMAGE_NUM_1_d, RESOURCE_ID_IMAGE_NUM_2_d, RESOURCE_ID_IMAGE_NUM_3_d,
    RESOURCE_ID_IMAGE_NUM_4_d, RESOURCE_ID_IMAGE_NUM_5_d, RESOURCE_ID_IMAGE_NUM_6_d, RESOURCE_ID_IMAGE_NUM_7_d,
    RESOURCE_ID_IMAGE_NUM_8_d, RESOURCE_ID_IMAGE_NUM_9_d, RESOURCE_ID_IMAGE_NUM_10_d, RESOURCE_ID_IMAGE_NUM_11_d
};

static const int IMAGE_RESOURCE_B_IDS[NUMBER_OF_IMAGES] = {
    RESOURCE_ID_IMAGE_NUM_0_b, RESOURCE_ID_IMAGE_NUM_1_b, RESOURCE_ID_IMAGE_NUM_2_b, RESOURCE_ID_IMAGE_NUM_3_b,
    RESOURCE_ID_IMAGE_NUM_4_b, RESOURCE_ID_IMAGE_NUM_5_b, RESOURCE_ID_IMAGE_NUM_6_b, RESOURCE_ID_IMAGE_NUM_7_b,
    RESOURCE_ID_IMAGE_NUM_8_b, RESOURCE_ID_IMAGE_NUM_9_b, RESOURCE_ID_IMAGE_NUM_10_b, RESOURCE_ID_IMAGE_NUM_11_b
};

static GBitmap *s_d_images[NUMBER_OF_IMAGES];
static GBitmap *s_b_images[NUMBER_OF_IMAGES];
static BitmapLayer *s_image_layers[TOTAL_IMAGE_SLOTS];
static TextLayer *s_time_details_layer;
static TextLayer *s_day_layer;
static Layer *s_battery_layer;
static Layer *s_bt_layer;
static BatteryChargeState s_battery_state;
static bool s_bt_connected;

// ------------------------------------------------------
static void set_digit_into_slot(int slot_number, GBitmap *bitmap) {
    bitmap_layer_set_bitmap(s_image_layers[slot_number], bitmap);
}

static void display_time(struct tm *tick_time) {
    static char week_text[] = "W00";
    static char time_details_text[] = "                  ";
    static char day_text[] = "                  ";

    set_digit_into_slot(0, s_b_images[tick_time->tm_hour % 12]);
    set_digit_into_slot(1, s_d_images[tick_time->tm_min / 10]);
    set_digit_into_slot(2, s_d_images[tick_time->tm_min % 10]);

    strftime(week_text, sizeof(week_text), "u%V", tick_time);
    snprintf(time_details_text, 9, "%s %s", day_names[tick_time->tm_wday], week_text);
    text_layer_set_text(s_time_details_layer, time_details_text);

    snprintf(day_text, 9, "%u %s%u", tick_time->tm_mday, month_names[tick_time->tm_mon], tick_time->tm_mon + 1);
    text_layer_set_text(s_day_layer, day_text);
}

static void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {
    display_time(tick_time);
}

static void battery_handler(BatteryChargeState new_state) {
    s_battery_state = new_state;
    layer_mark_dirty(s_battery_layer);
}

static void bt_handler(bool connected) {
    s_bt_connected = connected;
    layer_mark_dirty(window_get_root_layer(s_window));
}

static void paint_battery_layer(Layer *layer, GContext *ctx) {
    int x = (s_battery_state.charge_percent * 144) / 100;
    GPoint p0 = GPoint(0, 0);
    GPoint p1 = GPoint(x, 0);
    GPoint p2 = GPoint(144, 0);
    graphics_context_set_stroke_color(ctx, (s_battery_state.charge_percent < 15) ? GColorRed : GColorCyan);
    graphics_context_set_stroke_width(ctx, 6);
    graphics_draw_line(ctx, p0, p1);
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_draw_line(ctx, p1, p2);
}

static void paint_bt_layer(Layer *layer, GContext *ctx) {
    if (s_bt_connected) {
        graphics_context_set_stroke_color(ctx, GColorRed);
        graphics_context_set_stroke_width(ctx, 4);
        graphics_draw_line(ctx, GPoint(0, 0), GPoint(144, 0));
        graphics_draw_line(ctx, GPoint(143, 0), GPoint(143, 168));
        graphics_draw_line(ctx, GPoint(143, 167), GPoint(0, 167));
        graphics_draw_line(ctx, GPoint(0, 167), GPoint(0, 0));
    }
}

static void window_load(Window *window) {
    window_set_background_color(window, GColorBlack);

    // 48x67
    for (int i = 0; i < NUMBER_OF_IMAGES; i++) {
        s_d_images[i] = gbitmap_create_with_resource(IMAGE_RESOURCE_D_IDS[i]);
        s_b_images[i] = gbitmap_create_with_resource(IMAGE_RESOURCE_B_IDS[i]);
    }

    Layer *window_layer = window_get_root_layer(window);
    for (int i = 0; i < TOTAL_IMAGE_SLOTS; i++) {
        BitmapLayer *bitmap_layer = bitmap_layer_create(GRect(i * 48, 0, 48, 67));
        s_image_layers[i] = bitmap_layer;
        layer_add_child(window_layer, bitmap_layer_get_layer(bitmap_layer));
    }

    // Create time details TextLayer
    s_time_details_layer = text_layer_create(GRect(0, 67, 144, 34));
    text_layer_set_background_color(s_time_details_layer, GColorImperialPurple);
    text_layer_set_text_color(s_time_details_layer, GColorWhite);
    text_layer_set_font(s_time_details_layer, fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK));
    text_layer_set_text_alignment(s_time_details_layer, GTextAlignmentCenter);
    layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_time_details_layer));

    // Create day details TextLayer
    s_day_layer = text_layer_create(GRect(0, 101, 144, 34));
    text_layer_set_background_color(s_day_layer, GColorBulgarianRose);
    text_layer_set_text_color(s_day_layer, GColorWhite);
    text_layer_set_font(s_day_layer, fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK));
    text_layer_set_text_alignment(s_day_layer, GTextAlignmentCenter);
    layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_day_layer));

    // Create battery layer
    s_battery_layer = layer_create(GRect(0, 67, 144, 4));
    layer_set_update_proc(s_battery_layer, paint_battery_layer);
    layer_add_child(window_get_root_layer(window), s_battery_layer);

    // Create bt layer
    s_bt_layer = layer_create(GRect(0, 0, 144, 168));
    layer_set_update_proc(s_bt_layer, paint_bt_layer);
    layer_add_child(window_get_root_layer(window), s_bt_layer);

    // Display current time
    time_t now = time(NULL);
    struct tm *tick_time = localtime(&now);
    display_time(tick_time);

    // Subscribe to time updates
    tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);

    // Subscribe to the Battery State Service
    battery_state_service_subscribe(battery_handler);

    // Get the current battery level
    battery_handler(battery_state_service_peek());

    // Subscribe to Bluetooth updates
    bluetooth_connection_service_subscribe(bt_handler);

    // Show current connection state
    bt_handler(bluetooth_connection_service_peek());
}

static void window_unload(Window *window) {
    // Unsubscribe
    tick_timer_service_unsubscribe();
    battery_state_service_unsubscribe();
    bluetooth_connection_service_unsubscribe();

    // Destroy bitmaps
    for (int i = 0; i < NUMBER_OF_IMAGES; i++) {
        gbitmap_destroy(s_d_images[i]);
        gbitmap_destroy(s_b_images[i]);
    }

    // Destroy layers
    for (int i = 0; i < TOTAL_IMAGE_SLOTS; i++) {
        layer_remove_from_parent(bitmap_layer_get_layer(s_image_layers[i]));
        bitmap_layer_destroy(s_image_layers[i]);
    }

    text_layer_destroy(s_time_details_layer);
    text_layer_destroy(s_day_layer);
    layer_destroy(s_battery_layer);
    layer_destroy(s_bt_layer);
}

static void init(void) {
    // Create main Window element and assign to pointer
    s_window = window_create();

    // Set handlers to manage the elements inside the Window
    window_set_window_handlers(s_window, (WindowHandlers) {
        .load = window_load,
        .unload = window_unload
    });

    // Show the Window on the watch, with animated=true
    window_stack_push(s_window, true);
}

static void deinit(void) {
    window_destroy(s_window);
}

int main(void) {
    init();
    app_event_loop();
    deinit();
}
