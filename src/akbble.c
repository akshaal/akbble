#include <pebble.h>
#include "message-queue.h"
#include "utils.h"

#define TOTAL_IMAGE_SLOTS 3
#define NUMBER_OF_IMAGES 12
#define NUMBER_OF_WEATHER_ICONS 5

#define SCR_WIDTH 144
#define SCR_HEIGHT 168

#define ANIM_DURATION 2000
#define ANIM_DELAY 1000
#define ANIM_HEIGHT 25

#define MASK_WATCHFACE_REQUEST_ALARM 1
#define MASK_WATCHFACE_REQUEST_TEMP 2
#define MASK_WATCHFACE_REQUEST_ALL (MASK_WATCHFACE_REQUEST_TEMP | MASK_WATCHFACE_REQUEST_ALARM)

enum {
    CMD_OUT_GET_DATA = 20,
    CMD_IN_GET_DATA_RESPONSE = 21,

    CMD_IN_GET_DATA_RESPONSE_ALARM_TIME = 30,
    CMD_IN_GET_DATA_RESPONSE_WEATHER_TEMP = 31,
    CMD_IN_GET_DATA_RESPONSE_WEATHER_COND = 32,
    CMD_IN_GET_DATA_RESPONSE_WEATHER_HUM = 33,
    CMD_IN_GET_DATA_RESPONSE_WEATHER_WIND = 34
};

static const uint32_t WEATHER_ICONS[NUMBER_OF_WEATHER_ICONS] = {
    RESOURCE_ID_IMAGE_SUN, // 0
    RESOURCE_ID_IMAGE_CLOUD, // 1
    RESOURCE_ID_IMAGE_RAIN, // 2
    RESOURCE_ID_IMAGE_SNOW, // 3
    RESOURCE_ID_IMAGE_NOWEATHER // 4
};

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
static GBitmap *s_weather_images[NUMBER_OF_WEATHER_ICONS];
static GBitmap *s_anim_image = NULL;
static GBitmap *s_ingress_image = NULL;
static GBitmap *s_resist_image = NULL;
static GBitmap *s_noise_image = NULL;
static GBitmap *s_ingress_slice_image1 = NULL;
static GBitmap *s_ingress_slice_image2 = NULL;
static BitmapLayer *s_image_layers[TOTAL_IMAGE_SLOTS];
static BitmapLayer *s_weather_layer = NULL;
static TextLayer *s_time_details_layer_bg = NULL;
static TextLayer *s_time_details_layer = NULL;
static TextLayer *s_day_layer_bg = NULL;
static TextLayer *s_day_layer = NULL;
static TextLayer *s_temp_layer_bg = NULL;
static TextLayer *s_temp_layer = NULL;
static Layer *s_battery_layer = NULL;
static Layer *s_bt_layer = NULL;
static BitmapLayer *s_ingress_layer1 = NULL;
static BitmapLayer *s_ingress_layer2 = NULL;
static TextLayer *s_alarm_layer_bg = NULL;
static TextLayer *s_alarm_layer = NULL;
static BatteryChargeState s_battery_state;
static bool s_bt_connected;
static int s_temp = 99;
static int s_weather_icon = 4;
static time_t s_last_temp_update_secs = 0;
static time_t s_last_anim_secs = 0;
static time_t s_alarm_secs = 0;
static bool s_animation_running = false;
static int s_animation_mode;
static bool s_alarm_faraway = 0;
static GFont s_font30;

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
    if (!s_bt_connected) {
        graphics_context_set_stroke_color(ctx, GColorRed);
        graphics_context_set_stroke_width(ctx, 4);
        graphics_draw_line(ctx, GPoint(0, 0), GPoint(144, 0));
        graphics_draw_line(ctx, GPoint(143, 0), GPoint(143, 168));
        graphics_draw_line(ctx, GPoint(143, 167), GPoint(0, 167));
        graphics_draw_line(ctx, GPoint(0, 167), GPoint(0, 0));
    }
}

static void update_temp() {
    static char buf[4];

    if (s_temp_layer != NULL) {
        snprintf(buf, sizeof(buf), "%d", abs(s_temp));
        text_layer_set_text(s_temp_layer, buf);
        text_layer_set_background_color(s_temp_layer_bg, (s_temp >= 0) ? GColorBulgarianRose : GColorOxfordBlue);
    }
}

static void update_weather_icon() {
    if (s_weather_icon < 0 || s_weather_icon >= NUMBER_OF_WEATHER_ICONS) {
        s_weather_icon = 4;
    }

    if (s_weather_layer != NULL) {
        bitmap_layer_set_bitmap(s_weather_layer, s_weather_images[s_weather_icon]);
    }
}

static void update_alarm(char *str, bool inv) {
    if (s_alarm_layer != NULL) {
        text_layer_set_background_color(s_alarm_layer_bg, inv ? GColorWhite : GColorBlack);
        text_layer_set_text_color(s_alarm_layer, inv ? GColorBlack : GColorWhite);
        text_layer_set_text(s_alarm_layer, str);
    }
}

static void update_alarm_time() {
    static char buf[5];

    time_t cur_time = time(NULL);

    if (s_alarm_secs < 0) {
        s_alarm_secs = 0;
    }

    if (s_alarm_secs == 0) {
        s_alarm_faraway = false;
        update_alarm("", false);
    } else if (s_alarm_secs - cur_time > 24 * 60 * 60) {
        s_alarm_faraway = true;
        snprintf(buf, sizeof(buf), ">%ldd", (s_alarm_secs - cur_time) / (24 * 60 * 60));
        update_alarm(buf, false);
    } else {
        s_alarm_faraway = false;
        tm *tm = localtime(&s_alarm_secs);
        int rh = tm->tm_hour;
        int h = rh % 12;
        int m = tm->tm_min;
        bool inv = rh >= 12;
        char hc = h == 10 ? 'a' : (h == 11 ? 'b' : (h + '0'));
        snprintf(buf, sizeof(buf), "%c%02u", hc, m);
        update_alarm(buf, inv);
    }
}

static void inbox_received_callback(DictionaryIterator *iterator) {
    uint8_t cmd = 0;

    int8_t msg_weather_temp = 99;
    int8_t msg_weather_hum = 0;
    int8_t msg_weather_wind = 99;
    int8_t msg_weather_icon = 4;
    int32_t msg_alarm_mins = 0;

    // Read first item
    Tuple *t = dict_read_first(iterator);

    // Gather information from the dictionary
    while(t != NULL) {
        // Which key was received?
        switch(t->key) {
            case MSG_KEY_CMD:
                cmd = t->value->uint8;
                break;

            case CMD_IN_GET_DATA_RESPONSE_ALARM_TIME:
                msg_alarm_mins = t->value->int32;
                break;

            case CMD_IN_GET_DATA_RESPONSE_WEATHER_TEMP:
                msg_weather_temp = t->value->int8;
                break;

            case CMD_IN_GET_DATA_RESPONSE_WEATHER_COND:
                msg_weather_icon = t->value->int8;
                break;

            case CMD_IN_GET_DATA_RESPONSE_WEATHER_HUM:
                //msg_weather_hum = t->value->int8;  TODO!!!!!!!!!!!!!!!!!!!!!!!!!
                break;

            case CMD_IN_GET_DATA_RESPONSE_WEATHER_WIND:
                //msg_weather_wind = t->value->int8; TODO!!!!!!!!!!!!!!!!!!!!!!!!!
                break;

            default:
                break;
        }

        // Look for next item
        t = dict_read_next(iterator);
    }

    if (!cmd) {
        return; // cmd is missing
    }

    // Perform command
    switch(cmd) {
        case CMD_IN_GET_DATA_RESPONSE:
            s_temp = msg_weather_temp;
            s_weather_icon = msg_weather_icon;

            if (msg_alarm_mins < 0) {
                msg_alarm_mins = 0;
            }

            s_alarm_secs = ((long)msg_alarm_mins) * 60L;

            update_temp();
            update_weather_icon();
            update_alarm_time();
            break;

        default:
            break;
    }
}

static void my_animation_started(Animation *animation, void *context) {
    if (s_animation_mode & 1) {
        int anim_img_y1 = 0;
        int anim_scr_y1 = 0;

        // Prepare initial layers and stuff
        if (s_ingress_layer1) {
            bitmap_layer_destroy(s_ingress_layer1);
            s_ingress_layer1 = NULL;
        }
        if (s_ingress_layer2) {
            bitmap_layer_destroy(s_ingress_layer2);
            s_ingress_layer2 = NULL;
        }

        s_ingress_layer1 = bitmap_layer_create(GRect(0, anim_scr_y1, SCR_WIDTH, ANIM_HEIGHT));
        bitmap_layer_set_compositing_mode(s_ingress_layer1, GCompOpSet);
        layer_add_child(window_get_root_layer(s_window), bitmap_layer_get_layer(s_ingress_layer1));

        if (s_ingress_slice_image1) {
            gbitmap_destroy(s_ingress_slice_image1);
            s_ingress_slice_image1 = NULL;
        }

        s_ingress_slice_image1 = gbitmap_create_as_sub_bitmap(s_anim_image, GRect(0, anim_img_y1, SCR_WIDTH, ANIM_HEIGHT));

        bitmap_layer_set_bitmap(s_ingress_layer1, s_ingress_slice_image1);
    }

    if (s_animation_mode & 2) {
        int anim_img_y2 = SCR_HEIGHT - ANIM_HEIGHT;
        int anim_scr_y2 = SCR_HEIGHT - ANIM_HEIGHT;

        s_ingress_layer2 = bitmap_layer_create(GRect(0, anim_scr_y2, SCR_WIDTH, ANIM_HEIGHT));
        bitmap_layer_set_compositing_mode(s_ingress_layer2, GCompOpSet);
        layer_add_child(window_get_root_layer(s_window), bitmap_layer_get_layer(s_ingress_layer2));

        if (s_ingress_slice_image2) {
            gbitmap_destroy(s_ingress_slice_image2);
            s_ingress_slice_image2 = NULL;
        }

        s_ingress_slice_image2 = gbitmap_create_as_sub_bitmap(s_anim_image, GRect(0, anim_img_y2, SCR_WIDTH, ANIM_HEIGHT));

        bitmap_layer_set_bitmap(s_ingress_layer2, s_ingress_slice_image2);
    }
}

static void my_animation_update(Animation *animation, AnimationProgress progress) {
    if (s_animation_mode & 1) {
        int anim_img_y1 = SCR_HEIGHT - ANIM_HEIGHT - progress / (ANIMATION_NORMALIZED_MAX / (SCR_HEIGHT - ANIM_HEIGHT));
        int anim_scr_y1 = anim_img_y1;

        //APP_LOG(APP_LOG_LEVEL_DEBUG, "Loop index now p=%d, imgy=%d, scry=%d", (int)progress, anim_img_y, anim_scr_y);

        if (s_ingress_slice_image1) {
            gbitmap_destroy(s_ingress_slice_image1);
            s_ingress_slice_image1 = NULL;
        }

        s_ingress_slice_image1 = gbitmap_create_as_sub_bitmap(s_anim_image, GRect(0, anim_img_y1, SCR_WIDTH, ANIM_HEIGHT));

        layer_set_frame(bitmap_layer_get_layer(s_ingress_layer1), GRect(0, anim_scr_y1, SCR_WIDTH, ANIM_HEIGHT));

        bitmap_layer_set_bitmap(s_ingress_layer1, s_ingress_slice_image1);
    }

    if (s_animation_mode & 2) {
        int anim_img_y2 = progress / (ANIMATION_NORMALIZED_MAX / (SCR_HEIGHT - ANIM_HEIGHT));
        int anim_scr_y2 = anim_img_y2;

        //APP_LOG(APP_LOG_LEVEL_DEBUG, "Loop index now p=%d, imgy=%d, scry=%d", (int)progress, anim_img_y, anim_scr_y);

        if (s_ingress_slice_image2) {
            gbitmap_destroy(s_ingress_slice_image2);
            s_ingress_slice_image2 = NULL;
        }

        s_ingress_slice_image2 = gbitmap_create_as_sub_bitmap(s_anim_image, GRect(0, anim_img_y2, SCR_WIDTH, ANIM_HEIGHT));

        layer_set_frame(bitmap_layer_get_layer(s_ingress_layer2), GRect(0, anim_scr_y2, SCR_WIDTH, ANIM_HEIGHT));

        bitmap_layer_set_bitmap(s_ingress_layer2, s_ingress_slice_image2);
    }

    layer_mark_dirty(window_get_root_layer(s_window));
}

static void my_animation_stopped(Animation *animation, bool finished, void *context) {
    s_animation_running = false;

    if (s_ingress_layer1) {
        bitmap_layer_destroy(s_ingress_layer1);
        s_ingress_layer1 = NULL;
    }

    if (s_ingress_layer2) {
        bitmap_layer_destroy(s_ingress_layer2);
        s_ingress_layer2 = NULL;
    }
    animation_destroy(animation);
}

static void start_animation() {
    time_t cur_time = time(NULL);
    if (cur_time - s_last_anim_secs < 20) {
        return;
    }

    s_last_anim_secs = cur_time;
    s_animation_running = true;
    s_animation_mode = rand() % 3 + 1;

    int imgi = rand() % 3;
    s_anim_image = imgi == 0 ? s_resist_image : (imgi == 1 ? s_ingress_image : s_noise_image);

    // Animation itself
    Animation *animation = animation_create();
    animation_set_duration(animation, ANIM_DURATION);
    animation_set_delay(animation, ANIM_DELAY);
    animation_set_curve(animation, AnimationCurveEaseInOut);
    animation_set_reverse(animation, true);

    static AnimationImplementation anim_impl = {
        .update = my_animation_update
    };

    animation_set_implementation(animation, &anim_impl);
    animation_set_handlers(animation, (AnimationHandlers) {
        .started = my_animation_started,
        .stopped = my_animation_stopped,
    }, NULL);

    // Start it
    animation_schedule(animation);
}

static void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {
    display_time(tick_time);

    time_t cur_time = time(NULL);

    if (cur_time - s_last_temp_update_secs > 30*60) {
        // Send a message to android pebble app
        s_last_temp_update_secs = cur_time;
        mq_add(CMD_OUT_GET_DATA, "");
    }

    if (rand() % 5 == 0) {
        start_animation();
    }

    if (s_alarm_faraway && s_alarm_secs != 0) {
        update_alarm_time();
    }

    if (!s_bt_connected) {
        // Vibrate
        static const uint32_t const segments[] = { 25 };
        VibePattern pat = {
            .durations = segments,
            .num_segments = ARRAY_LENGTH(segments),
        };
        vibes_enqueue_custom_pattern(pat);
    }
}

static void window_load(Window *window) {
    window_set_background_color(window, GColorBlack);

    s_font30 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_34));

    s_ingress_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_INGRESS);
    s_resist_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_RESIST);
    s_noise_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_NOISE);

    // 48x67
    for (int i = 0; i < NUMBER_OF_IMAGES; i++) {
        s_d_images[i] = gbitmap_create_with_resource(IMAGE_RESOURCE_D_IDS[i]);
        s_b_images[i] = gbitmap_create_with_resource(IMAGE_RESOURCE_B_IDS[i]);
    }

    for (uint i = 0; i < NUMBER_OF_WEATHER_ICONS; i++) {
        s_weather_images[i] = gbitmap_create_with_resource(WEATHER_ICONS[i]);
    }

    Layer *window_layer = window_get_root_layer(window);
    for (int i = 0; i < TOTAL_IMAGE_SLOTS; i++) {
        BitmapLayer *bitmap_layer = bitmap_layer_create(GRect(i * 48, 0, 48, 67));
        s_image_layers[i] = bitmap_layer;
        layer_add_child(window_layer, bitmap_layer_get_layer(bitmap_layer));
    }

    // Create time details TextLayer
    s_time_details_layer_bg = text_layer_create(GRect(0, 67, 144, 34));
    text_layer_set_background_color(s_time_details_layer_bg, GColorImperialPurple);
    layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_time_details_layer_bg));

    s_time_details_layer = text_layer_create(GRect(0, 67-4, 144, 34+2));
    text_layer_set_background_color(s_time_details_layer, GColorClear);
    text_layer_set_text_color(s_time_details_layer, GColorWhite);
    text_layer_set_font(s_time_details_layer, s_font30);
    text_layer_set_text_alignment(s_time_details_layer, GTextAlignmentCenter);
    layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_time_details_layer));

    // Create day details TextLayer
    s_day_layer_bg = text_layer_create(GRect(0, 101, 144, 34));
    text_layer_set_background_color(s_day_layer_bg, GColorBulgarianRose);
    layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_day_layer_bg));

    s_day_layer = text_layer_create(GRect(0, 101-6, 144, 34+2));
    text_layer_set_background_color(s_day_layer, GColorClear);
    text_layer_set_text_color(s_day_layer, GColorWhite);
    text_layer_set_font(s_day_layer, s_font30);
    text_layer_set_text_alignment(s_day_layer, GTextAlignmentCenter);
    layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_day_layer));

    // Create temp details TextLayer
    s_temp_layer_bg = text_layer_create(GRect(0, 135, 40, 33));
    layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_temp_layer_bg));

    s_temp_layer = text_layer_create(GRect(0, 135-6, 40, 33+2));
    text_layer_set_background_color(s_temp_layer, GColorClear);
    text_layer_set_text_color(s_temp_layer, GColorWhite);
    text_layer_set_font(s_temp_layer, s_font30);
    text_layer_set_text_alignment(s_temp_layer, GTextAlignmentCenter);
    layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_temp_layer));

    // Create weather icon layer
    s_weather_layer = bitmap_layer_create(GRect(40, 135, 33, 33));
    layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(s_weather_layer));

    // Create alarm details TextLayer
    s_alarm_layer_bg = text_layer_create(GRect(73, 135, 144-73, 33));
    layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_alarm_layer_bg));

    s_alarm_layer = text_layer_create(GRect(73, 135-6, 144-73, 33+2));
    text_layer_set_font(s_alarm_layer, s_font30);
    text_layer_set_background_color(s_alarm_layer, GColorClear);
    text_layer_set_text_alignment(s_alarm_layer, GTextAlignmentCenter);
    layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_alarm_layer));

    // Create battery layer THIS SHOULD BE AFTER OTHER TEXTS
    s_battery_layer = layer_create(GRect(0, 67, 144, 4));
    layer_set_update_proc(s_battery_layer, paint_battery_layer);
    layer_add_child(window_get_root_layer(window), s_battery_layer);

    // Create bt layer: THIS SHOULD BE THE LAST LAYER
    s_bt_layer = layer_create(GRect(0, 0, 144, 168));
    layer_set_update_proc(s_bt_layer, paint_bt_layer);
    layer_add_child(window_get_root_layer(window), s_bt_layer);

    // Display current time
    time_t now = time(NULL);
    struct tm *tick_time = localtime(&now);
    display_time(tick_time);

    // Display some temp
    update_alarm("", false);
    update_temp();
    update_weather_icon();

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

    // Animate everything
    if (rand() % 3 == 0) {
        start_animation();
    }
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

    for (uint i = 0; i < NUMBER_OF_WEATHER_ICONS; i++) {
        gbitmap_destroy(s_weather_images[i]);
    }

    if (s_ingress_slice_image1) {
        gbitmap_destroy(s_ingress_slice_image1);
        s_ingress_slice_image1 = NULL;
    }
    if (s_ingress_slice_image2) {
        gbitmap_destroy(s_ingress_slice_image2);
        s_ingress_slice_image2 = NULL;
    }

    if (s_ingress_image) {
        gbitmap_destroy(s_ingress_image);
        s_ingress_image = NULL;
    }

    if (s_resist_image) {
        gbitmap_destroy(s_resist_image);
        s_resist_image = NULL;
    }

    if (s_noise_image) {
        gbitmap_destroy(s_noise_image);
        s_noise_image = NULL;
    }

    // Destroy layers
    for (int i = 0; i < TOTAL_IMAGE_SLOTS; i++) {
        layer_remove_from_parent(bitmap_layer_get_layer(s_image_layers[i]));
        bitmap_layer_destroy(s_image_layers[i]);
    }

    text_layer_destroy(s_time_details_layer);
    text_layer_destroy(s_time_details_layer_bg);
    text_layer_destroy(s_day_layer);
    text_layer_destroy(s_day_layer_bg);
    text_layer_destroy(s_alarm_layer);
    text_layer_destroy(s_alarm_layer_bg);
    text_layer_destroy(s_temp_layer);
    text_layer_destroy(s_temp_layer_bg);
    layer_destroy(s_battery_layer);
    layer_destroy(s_bt_layer);
    bitmap_layer_destroy(s_weather_layer);

    if (s_ingress_layer1) {
        bitmap_layer_destroy(s_ingress_layer1);
        s_ingress_layer1 = NULL;
    }

    if (s_ingress_layer2) {
        bitmap_layer_destroy(s_ingress_layer2);
        s_ingress_layer2 = NULL;
    }

    fonts_unload_custom_font(s_font30);

    s_weather_layer = NULL;
    s_temp_layer = NULL;
}

static void init(void) {
    s_last_temp_update_secs = time(NULL);

    // Create main Window element and assign to pointer
    s_window = window_create();

    // Set handlers to manage the elements inside the Window
    window_set_window_handlers(s_window, (WindowHandlers) {
        .load = window_load,
        .unload = window_unload
    });

    // Show the Window on the watch, with animated=true
    window_stack_push(s_window, true);

    mq_init(inbox_received_callback);

    // Initial request
    mq_add(CMD_OUT_GET_DATA, "");
}

static void deinit(void) {
    window_destroy(s_window);
}

int main(void) {
    init();
    app_event_loop();
    deinit();
}
