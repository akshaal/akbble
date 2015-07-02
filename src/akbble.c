#include <pebble.h>

#define TOTAL_IMAGE_SLOTS 3
#define NUMBER_OF_IMAGES 12

static Window *s_window;

const int IMAGE_RESOURCE_D_IDS[NUMBER_OF_IMAGES] = {
    RESOURCE_ID_IMAGE_NUM_0_d, RESOURCE_ID_IMAGE_NUM_1_d, RESOURCE_ID_IMAGE_NUM_2_d, RESOURCE_ID_IMAGE_NUM_3_d,
    RESOURCE_ID_IMAGE_NUM_4_d, RESOURCE_ID_IMAGE_NUM_5_d, RESOURCE_ID_IMAGE_NUM_6_d, RESOURCE_ID_IMAGE_NUM_7_d,
    RESOURCE_ID_IMAGE_NUM_8_d, RESOURCE_ID_IMAGE_NUM_9_d, RESOURCE_ID_IMAGE_NUM_10_d, RESOURCE_ID_IMAGE_NUM_11_d
};

const int IMAGE_RESOURCE_B_IDS[NUMBER_OF_IMAGES] = {
    RESOURCE_ID_IMAGE_NUM_0_b, RESOURCE_ID_IMAGE_NUM_1_b, RESOURCE_ID_IMAGE_NUM_2_b, RESOURCE_ID_IMAGE_NUM_3_b,
    RESOURCE_ID_IMAGE_NUM_4_b, RESOURCE_ID_IMAGE_NUM_5_b, RESOURCE_ID_IMAGE_NUM_6_b, RESOURCE_ID_IMAGE_NUM_7_b,
    RESOURCE_ID_IMAGE_NUM_8_b, RESOURCE_ID_IMAGE_NUM_9_b, RESOURCE_ID_IMAGE_NUM_10_b, RESOURCE_ID_IMAGE_NUM_11_b
};

static GBitmap *s_d_images[NUMBER_OF_IMAGES];
static GBitmap *s_b_images[NUMBER_OF_IMAGES];
static BitmapLayer *s_image_layers[TOTAL_IMAGE_SLOTS];

// ------------------------------------------------------
static void set_digit_into_slot(int slot_number, GBitmap *bitmap) {
    bitmap_layer_set_bitmap(s_image_layers[slot_number], bitmap);
}

static void display_time(struct tm *tick_time) {
    set_digit_into_slot(0, s_b_images[tick_time->tm_hour % 12]);
    set_digit_into_slot(1, s_d_images[tick_time->tm_min / 10]);
    set_digit_into_slot(2, s_d_images[tick_time->tm_min % 10]);
}

static void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {
    display_time(tick_time);
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

    // Display current time
    time_t now = time(NULL);
    struct tm *tick_time = localtime(&now);
    display_time(tick_time);

    // Subscribe to time updates
    tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);
}

static void window_unload(Window *window) {
    // Stop timer
    tick_timer_service_unsubscribe();

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
