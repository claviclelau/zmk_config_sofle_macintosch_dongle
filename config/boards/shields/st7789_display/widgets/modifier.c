/*
 * Based on ST7789V sample:
 * Copyright (c) 2019 Marc Reilly
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(modifier, LOG_LEVEL_INF);

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/services/bas.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>

#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <dt-bindings/zmk/modifiers.h>
#include "helpers/display.h"
#include "layer_status.h"

static bool modifier_widget_running = false;
static bool modifier_widget_initialized = false;

static const uint16_t modifier_icon_scale = 2;
static uint16_t modifier_font_width = 11;
static uint16_t modifier_font_height = 11;
static uint16_t *scaled_bitmap_modifier_font;

static const uint16_t modifier_y = 174;
static const uint16_t modifier_box_height = 53;
static const uint16_t modifier_x[] = {8, 65, 122, 179};
static const uint16_t modifier_width = 53;

struct modifiers_state {
    uint8_t modifiers;
};

static struct modifiers_state modifier_state;

static const uint16_t windows_logo_bitmap[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0,
    0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0,
    0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0,
    0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0,
    0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0,
    0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0,
    0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static const uint16_t ctrl_icon_bitmap[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0,
    0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0,
    0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0,
    0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0,
    0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0,
    0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0,
    0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0,
    0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0,
};

static const uint16_t shift_icon_bitmap[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0,
    0, 0, 0, 1, 1, 0, 1, 1, 0, 0, 0,
    0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0,
    0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0,
    0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0,
    0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0,
    0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static const uint16_t alt_icon_bitmap[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 0, 0, 1, 1, 1, 0,
    0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0,
    0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0,
    0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0,
    0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0,
    0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0,
    0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static void print_modifier_key(const char *label, uint8_t label_length, const uint16_t bitmap[],
                               uint16_t x, uint16_t width, bool pressed) {
    uint16_t background =
        pressed ? rgb888_to_rgb565(0x082F49) : get_menu_bg_color();
    uint16_t foreground =
        pressed ? rgb888_to_rgb565(0x7DD3FC) : get_frame_color();
    uint16_t border =
        pressed ? rgb888_to_rgb565(0x38BDF8) : get_frame_color_1();

    print_filled_rounded_screen_area(x, modifier_y, width, modifier_box_height, 7, border);
    print_filled_rounded_screen_area(x + 1, modifier_y + 1, width - 2,
                                     modifier_box_height - 2, 6, background);
    uint16_t icon_width = modifier_font_width * modifier_icon_scale;
    render_bitmap(scaled_bitmap_modifier_font, (uint16_t *)bitmap,
                  x + (width - icon_width) / 2, modifier_y + 4, modifier_font_width,
                  modifier_font_height, modifier_icon_scale, foreground, background);

    uint16_t text_width = (label_length * 9) + (label_length - 1);
    print_char_array(scaled_bitmap_modifier_font, (char *)label, x + (width - text_width) / 2,
                     modifier_y + 33, 3, foreground, background, FONT_SIZE_3x6, 1, label_length,
                     label_length);
}

void print_modifiers() {
    print_modifier_key("CTRL", 4, ctrl_icon_bitmap, modifier_x[0], modifier_width,
                       (modifier_state.modifiers & (MOD_LCTL | MOD_RCTL)) != 0);
    print_modifier_key("SHIFT", 5, shift_icon_bitmap, modifier_x[1], modifier_width,
                       (modifier_state.modifiers & (MOD_LSFT | MOD_RSFT)) != 0);
    print_modifier_key("WIN", 3, windows_logo_bitmap, modifier_x[2], modifier_width,
                       (modifier_state.modifiers & (MOD_LGUI | MOD_RGUI)) != 0);
    print_modifier_key("ALT", 3, alt_icon_bitmap, modifier_x[3], modifier_width,
                       (modifier_state.modifiers & (MOD_LALT | MOD_RALT)) != 0);
}

static void print_changed_modifiers(uint8_t previous, uint8_t current) {
    bool was_pressed = (previous & (MOD_LSFT | MOD_RSFT)) != 0;
    bool is_pressed = (current & (MOD_LSFT | MOD_RSFT)) != 0;
    if (was_pressed != is_pressed) {
        print_modifier_key("SHIFT", 5, shift_icon_bitmap, modifier_x[1], modifier_width, is_pressed);
    }

    was_pressed = (previous & (MOD_LCTL | MOD_RCTL)) != 0;
    is_pressed = (current & (MOD_LCTL | MOD_RCTL)) != 0;
    if (was_pressed != is_pressed) {
        print_modifier_key("CTRL", 4, ctrl_icon_bitmap, modifier_x[0], modifier_width, is_pressed);
    }

    was_pressed = (previous & (MOD_LGUI | MOD_RGUI)) != 0;
    is_pressed = (current & (MOD_LGUI | MOD_RGUI)) != 0;
    if (was_pressed != is_pressed) {
        print_modifier_key("WIN", 3, windows_logo_bitmap, modifier_x[2], modifier_width, is_pressed);
    }

    was_pressed = (previous & (MOD_LALT | MOD_RALT)) != 0;
    is_pressed = (current & (MOD_LALT | MOD_RALT)) != 0;
    if (was_pressed != is_pressed) {
        print_modifier_key("ALT", 3, alt_icon_bitmap, modifier_x[3], modifier_width, is_pressed);
    }
}

static struct modifiers_state modifiers_get_state(const zmk_event_t *eh) {
    return (struct modifiers_state){.modifiers = zmk_hid_get_explicit_mods()};
}

void modifiers_update_cb(struct modifiers_state state) {
    uint8_t previous = modifier_state.modifiers;
    modifier_state = state;
    if (modifier_widget_initialized && modifier_widget_running) {
        print_changed_modifiers(previous, state.modifiers);
    }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_modifiers, struct modifiers_state, modifiers_update_cb,
                            modifiers_get_state)

ZMK_SUBSCRIPTION(widget_modifiers, zmk_keycode_state_changed);

void zmk_widget_modifier_init() {
    uint16_t modifier_font_size =
        (modifier_font_width * modifier_icon_scale) *
        (modifier_font_height * modifier_icon_scale);
    scaled_bitmap_modifier_font = k_malloc(modifier_font_size * 2 * sizeof(uint16_t));

    widget_modifiers_init();
    modifier_widget_initialized = true;
}

void start_modifier_status() { modifier_widget_running = true; }

void stop_modifier_status() { modifier_widget_running = false; }
