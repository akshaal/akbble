#include <pebble.h>

static Window *window;

static void window_load(Window *window) {
}

static void window_unload(Window *window) {
}

static void init(void) {
    // Create main Window element and assign to pointer
    window = window_create();

    // Set handlers to manage the elements inside the Window
    window_set_window_handlers(window, (WindowHandlers) {
        .load = main_window_load,
        .unload = main_window_unload
    });

    // Show the Window on the watch, with animated=true
    window_stack_push(window, true);
}

static void deinit(void) {
    window_destroy(window);
}

int main(void) {
    init();
    app_event_loop();
    deinit();
}
