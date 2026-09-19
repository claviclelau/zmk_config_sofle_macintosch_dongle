/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/bluetooth/services/bas.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/usb.h>
#include <zmk/ble.h>
#include <zmk/endpoints.h>

#include "output_status.h"
#include "layer_status.h"
#include "helpers/display.h"

static bool status_widget_initialized = false;
static struct output_status_state status_state;
static uint16_t *scaled_bitmap_status;
static uint16_t *scaled_bitmap_symbol;
static uint16_t *scaled_bitmap_bt_num;

static const uint16_t status_height = 9;
static const uint16_t status_width = 9;
static const uint16_t status_scale = 3;

static const uint16_t symbol_scale = 1;
static const uint16_t symbol_width = 9;
static const uint16_t symbol_height = 15;

static const uint16_t bt_num_scale = 4;
static const uint16_t bt_num_width = 5;
static const uint16_t bt_num_height = 7;

static const uint16_t usb_ready_bitmap[] = {
    0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0,
    0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 1,
    1, 0, 0, 0, 0, 1, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 1, 1, 0, 0, 0, 1,
    1, 0, 0, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
};
static const uint16_t usb_not_ready_bitmap[] = {
    0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0,
    0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 0, 0, 0, 1, 0, 1,
    1, 0, 0, 1, 0, 1, 0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0, 0, 1,
    1, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
};
static const uint16_t bluetooth_bitmap[] = {
    0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0,
    0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 0, 0, 1, 1, 1, 0, 1, 1, 0,
    0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0,
    0, 0, 1, 1, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 1, 1, 0,
    0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0,
};
static const uint16_t none_bitmap[] = {
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1,
};

static const uint16_t open[] = {
    1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
};
static const uint16_t not_ok[] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 0, 0, 0, 1, 0, 1,
    1, 0, 0, 1, 0, 1, 0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0, 0, 1,
    1, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
};
static const uint16_t ok[] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 1,
    1, 0, 0, 0, 0, 1, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 1, 1, 0, 0, 0, 1,
    1, 0, 0, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
};
static const uint16_t none[] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1,
    1, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 0, 0, 1, 0, 1,
    1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
};

struct output_status_state {
    struct zmk_endpoint_instance selected_endpoint;
    int active_profile_index;
    bool active_profile_connected;
    bool active_profile_bonded;
    bool usb_is_hid_ready;
};

void print_bitmap_transport(uint16_t *scaled_bitmap, Transport t, bool is_ready, uint16_t x,
                            uint16_t y, uint16_t scale, uint16_t color, uint16_t bg_color) {
    switch (t) {
    case TRANSPORT_USB:
        if (is_ready) {
            render_bitmap(scaled_bitmap, usb_ready_bitmap, x, y, symbol_width, symbol_height, scale,
                          color, bg_color);
        } else {
            render_bitmap(scaled_bitmap, usb_not_ready_bitmap, x, y, symbol_width, symbol_height,
                          scale, color, bg_color);
        }
        break;
    case TRANSPORT_BLUETOOTH:
        render_bitmap(scaled_bitmap, bluetooth_bitmap, x, y, symbol_width, symbol_height, scale,
                      color, bg_color);
        break;
    default:
        render_bitmap(scaled_bitmap, none_bitmap, x, y, symbol_width, symbol_height, scale, color,
                      bg_color);
    }
}

void print_bitmap_status(uint16_t *scaled_bitmap, Status s, uint16_t x, uint16_t y, uint16_t scale,
                         uint16_t color, uint16_t bg_color) {
    switch (s) {
    case STATUS_OPEN:
        render_bitmap(scaled_bitmap, open, x, y, status_width, status_height, scale, color,
                      bg_color);
        break;
    case STATUS_OK:
        render_bitmap(scaled_bitmap, ok, x, y, status_width, status_height, scale, color, bg_color);
        break;
    case STATUS_NOT_OK:
        render_bitmap(scaled_bitmap, not_ok, x, y, status_width, status_height, scale, color,
                      bg_color);
        break;
    default:
        render_bitmap(scaled_bitmap, none, x, y, 4, 6, scale, color, bg_color);
    }
}

static struct output_status_state get_state(const zmk_event_t *_eh) {
    return (struct output_status_state){.selected_endpoint = zmk_endpoints_selected(),
                                        .active_profile_index = zmk_ble_active_profile_index(),
                                        .active_profile_connected =
                                            zmk_ble_active_profile_is_connected(),
                                        .active_profile_bonded = !zmk_ble_active_profile_is_open(),
                                        .usb_is_hid_ready = zmk_usb_is_hid_ready()};
}

void print_bluetooth_status(uint16_t x, uint16_t y, struct output_status_state state) {
    if (state.active_profile_bonded) {
        if (state.active_profile_connected) {
            print_bitmap_status(scaled_bitmap_status, STATUS_OK, x, y, status_scale,
                                get_bt_status_ok_color(), get_bt_status_bg_color());
        } else {
            print_bitmap_status(scaled_bitmap_status, STATUS_NOT_OK, x, y, status_scale,
                                get_bt_status_not_ok_color(), get_bt_status_bg_color());
        }
    } else {
        print_bitmap_status(scaled_bitmap_status, STATUS_OPEN, x, y, status_scale,
                            get_bt_status_open_color(), get_bt_status_bg_color());
    }
}

void print_bluetooth_profile(uint16_t x, uint16_t y, int active_profile) {
    if (active_profile < 0 || active_profile > 4) {
        print_bitmap(scaled_bitmap_bt_num, CHAR_NONE, x, y, bt_num_scale, get_bt_num_color(),
                     get_bt_bg_color(), FONT_SIZE_5x7);
        return;
    }
    print_bitmap(scaled_bitmap_bt_num, active_profile + 1, x, y, bt_num_scale, get_bt_num_color(),
                 get_bt_bg_color(), FONT_SIZE_5x7);
}

void print_bluetooth_profiles(uint16_t x, uint16_t y, struct output_status_state state) {
    print_bluetooth_profile(x, y, state.active_profile_index);
}

void print_symbols(uint16_t usb_x, uint16_t ble_x, uint16_t y, struct output_status_state state) {
    const uint16_t usb_active = rgb888_to_rgb565(0x38BDF8);
    const uint16_t ble_active = rgb888_to_rgb565(0xA78BFA);
    const uint16_t inactive = rgb888_to_rgb565(0x475569);

    switch (state.selected_endpoint.transport) {
    case ZMK_TRANSPORT_USB:
        print_bitmap_transport(scaled_bitmap_symbol, TRANSPORT_USB, state.usb_is_hid_ready, usb_x,
                               y, symbol_scale, usb_active, get_symbol_bg_color());
        print_bitmap_transport(scaled_bitmap_symbol, TRANSPORT_BLUETOOTH, true, ble_x, y,
                               symbol_scale, inactive, get_symbol_bg_color());
        break;
    case ZMK_TRANSPORT_BLE:
        print_bitmap_transport(scaled_bitmap_symbol, TRANSPORT_USB, state.usb_is_hid_ready, usb_x,
                               y, symbol_scale, inactive, get_symbol_bg_color());
        print_bitmap_transport(scaled_bitmap_symbol, TRANSPORT_BLUETOOTH, true, ble_x, y,
                               symbol_scale, ble_active, get_symbol_bg_color());
        break;
    }
}

static void print_connection_label(struct output_status_state state) {
    const uint16_t background = rgb888_to_rgb565(0x070B14);
    const uint16_t usb_color =
        rgb888_to_rgb565(state.usb_is_hid_ready ? 0xFFD700 : 0x475569);
    const uint16_t bluetooth_color =
        rgb888_to_rgb565(state.active_profile_connected
                             ? 0x008FD5
                             : (state.active_profile_bonded ? 0xFBBF24 : 0x475569));

    print_filled_screen_area(176, 3, 64, 23, background);
    print_bitmap_transport(scaled_bitmap_symbol, TRANSPORT_USB, state.usb_is_hid_ready, 176, 7,
                           symbol_scale, usb_color, background);
    print_bitmap_transport(scaled_bitmap_symbol, TRANSPORT_BLUETOOTH, true, 196, 7, symbol_scale,
                           bluetooth_color, background);
    if (state.active_profile_index >= 0 && state.active_profile_index <= 4) {
        char profile[] = {(char)('1' + state.active_profile_index)};
        print_char_array(scaled_bitmap_bt_num, profile, 216, 4, 4, bluetooth_color, background,
                         FONT_SIZE_3x5, 1, ARRAY_SIZE(profile), ARRAY_SIZE(profile));
    }
}

void set_status_symbol() {
    const uint16_t background = rgb888_to_rgb565(0x070B14);
    const uint16_t color = rgb888_to_rgb565(0xA7B0C5);
    char name[] = "YIMING";
    char suffix[] = "S";
    char keyboard[] = "KB";

    print_filled_screen_area(0, 0, 240, 28, background);
    print_char_array(scaled_bitmap_bt_num, name, 4, 4, 4, color, background, FONT_SIZE_3x5, 1,
                     ARRAY_SIZE(name) - 1, ARRAY_SIZE(name) - 1);
    print_filled_screen_area(84, 4, 4, 8, color);
    print_char_array(scaled_bitmap_bt_num, suffix, 92, 4, 4, color, background, FONT_SIZE_3x5, 1,
                     ARRAY_SIZE(suffix) - 1, ARRAY_SIZE(suffix) - 1);
    print_char_array(scaled_bitmap_bt_num, keyboard, 120, 4, 4, color, background, FONT_SIZE_3x5, 1,
                     ARRAY_SIZE(keyboard) - 1, ARRAY_SIZE(keyboard) - 1);
    print_connection_label(status_state);
}

void output_status_update_cb(struct output_status_state state) {
    struct output_status_state previous = status_state;
    status_state = state;
    if (status_widget_initialized) {
        if (previous.selected_endpoint.transport != state.selected_endpoint.transport ||
            previous.usb_is_hid_ready != state.usb_is_hid_ready ||
            previous.active_profile_connected != state.active_profile_connected ||
            previous.active_profile_bonded != state.active_profile_bonded ||
            previous.active_profile_index != state.active_profile_index) {
            print_connection_label(state);
        }
    }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_output_status, struct output_status_state,
                            output_status_update_cb, get_state)
ZMK_SUBSCRIPTION(widget_output_status, zmk_endpoint_changed);
ZMK_SUBSCRIPTION(widget_output_status, zmk_ble_active_profile_changed);
ZMK_SUBSCRIPTION(widget_output_status, zmk_usb_conn_state_changed);

void zmk_widget_output_status_init() {
    /* Two original 9 x 15 transport glyphs rendered at 2x. */
    scaled_bitmap_symbol =
        k_malloc((symbol_width * symbol_scale) * (symbol_height * symbol_scale) * sizeof(uint16_t));

    /* One 7 x 9 layer-style glyph at factor 2 expands to 15 x 19 pixels. */
    scaled_bitmap_bt_num = k_malloc((7 * 2 + 1) * (9 * 2 + 1) * sizeof(uint16_t));

    widget_output_status_init();
}

void start_output_status() {
    set_status_symbol();
    status_widget_initialized = true;
}

void stop_output_status(void) { status_widget_initialized = false; }
