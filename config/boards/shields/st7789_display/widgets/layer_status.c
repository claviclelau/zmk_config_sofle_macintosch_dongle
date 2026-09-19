/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include <zmk/display/widgets/layer_status.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/event_manager.h>
#include <zmk/endpoints.h>
#include <zmk/keymap.h>
#include "helpers/display.h"
#include <stdio.h>
#include <string.h>

struct layer_status_state {
    uint8_t index;
    const char *label;
};

static bool layer_widget_running = false;
static struct layer_status_state current_layer;
static uint16_t *scaled_bitmap_layer_font;
/* Shared scratch space avoids placing the glyph buffer on the display thread stack. */
static uint16_t layer_glyph_bitmap[(7 * 5 + 1) * (9 * 5 + 1)];

/* Complete A-Z clean sans-serif font for every possible English layer name. */
static const char mac_layer_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789'";
static const uint8_t mac_layer_rows[][9] = {
    {0x1C, 0x22, 0x41, 0x41, 0x7F, 0x41, 0x41, 0x41, 0x41}, /* A */
    {0x7E, 0x41, 0x41, 0x41, 0x7E, 0x41, 0x41, 0x41, 0x7E}, /* B */
    {0x3F, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x3F}, /* C */
    {0x7C, 0x42, 0x41, 0x41, 0x41, 0x41, 0x41, 0x42, 0x7C}, /* D */
    {0x7F, 0x40, 0x40, 0x40, 0x7E, 0x40, 0x40, 0x40, 0x7F}, /* E */
    {0x7F, 0x40, 0x40, 0x40, 0x7E, 0x40, 0x40, 0x40, 0x40}, /* F */
    {0x3E, 0x41, 0x40, 0x40, 0x4F, 0x41, 0x41, 0x41, 0x3E}, /* G */
    {0x41, 0x41, 0x41, 0x41, 0x7F, 0x41, 0x41, 0x41, 0x41}, /* H */
    {0x7F, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x7F}, /* I */
    {0x0F, 0x02, 0x02, 0x02, 0x02, 0x02, 0x42, 0x42, 0x3C}, /* J */
    {0x41, 0x42, 0x44, 0x48, 0x70, 0x48, 0x44, 0x42, 0x41}, /* K */
    {0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x7F}, /* L */
    {0x41, 0x63, 0x55, 0x49, 0x49, 0x41, 0x41, 0x41, 0x41}, /* M */
    {0x41, 0x61, 0x51, 0x49, 0x45, 0x43, 0x41, 0x41, 0x41}, /* N */
    {0x3E, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x3E}, /* O */
    {0x7E, 0x41, 0x41, 0x41, 0x7E, 0x40, 0x40, 0x40, 0x40}, /* P */
    {0x3E, 0x41, 0x41, 0x41, 0x41, 0x41, 0x45, 0x42, 0x3D}, /* Q */
    {0x7E, 0x41, 0x41, 0x41, 0x7E, 0x48, 0x44, 0x42, 0x41}, /* R */
    {0x3F, 0x40, 0x40, 0x40, 0x3E, 0x01, 0x01, 0x01, 0x7E}, /* S */
    {0x7F, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08}, /* T */
    {0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x3E}, /* U */
    {0x41, 0x41, 0x41, 0x41, 0x41, 0x22, 0x22, 0x14, 0x08}, /* V */
    {0x41, 0x41, 0x41, 0x41, 0x49, 0x49, 0x55, 0x55, 0x22}, /* W */
    {0x41, 0x41, 0x22, 0x14, 0x08, 0x14, 0x22, 0x41, 0x41}, /* X */
    {0x41, 0x41, 0x22, 0x14, 0x08, 0x08, 0x08, 0x08, 0x08}, /* Y */
    {0x7F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x7F}, /* Z */
    {0x3E, 0x41, 0x43, 0x45, 0x49, 0x51, 0x61, 0x41, 0x3E}, /* 0 */
    {0x08, 0x18, 0x28, 0x08, 0x08, 0x08, 0x08, 0x08, 0x3E}, /* 1 */
    {0x3E, 0x41, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x7F}, /* 2 */
    {0x3E, 0x41, 0x01, 0x0E, 0x01, 0x01, 0x01, 0x41, 0x3E}, /* 3 */
    {0x06, 0x0A, 0x12, 0x22, 0x42, 0x7F, 0x02, 0x02, 0x02}, /* 4 */
    {0x7F, 0x40, 0x40, 0x7E, 0x01, 0x01, 0x01, 0x41, 0x3E}, /* 5 */
    {0x3E, 0x40, 0x40, 0x7E, 0x41, 0x41, 0x41, 0x41, 0x3E}, /* 6 */
    {0x7F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10, 0x10, 0x10}, /* 7 */
    {0x3E, 0x41, 0x41, 0x41, 0x3E, 0x41, 0x41, 0x41, 0x3E}, /* 8 */
    {0x3E, 0x41, 0x41, 0x41, 0x3F, 0x01, 0x01, 0x01, 0x3E}, /* 9 */
    {0x18, 0x18, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* ' */
};

static bool make_mac_layer_glyph(char c, uint8_t x_factor, uint8_t y_factor, uint16_t *bitmap) {
    if (c >= 'a' && c <= 'z') {
        c -= 'a' - 'A';
    }

    for (uint8_t glyph = 0; glyph < sizeof(mac_layer_chars) - 1; glyph++) {
        if (mac_layer_chars[glyph] != c) {
            continue;
        }
        uint8_t width = (7 * x_factor) + 1;
        uint8_t height = (9 * y_factor) + 1;
        memset(bitmap, 0, width * height * sizeof(uint16_t));

        for (uint8_t source_y = 0; source_y < 9; source_y++) {
            for (uint8_t source_x = 0; source_x < 7; source_x++) {
                if (((mac_layer_rows[glyph][source_y] >> (6 - source_x)) & 1) == 0) {
                    continue;
                }
                /* One extra output pixel makes the strokes subtly heavier. */
                for (uint8_t dy = 0; dy <= y_factor; dy++) {
                    for (uint8_t dx = 0; dx <= x_factor; dx++) {
                        bitmap[((source_y * y_factor + dy) * width) +
                               (source_x * x_factor + dx)] = 1;
                    }
                }
            }
        }
        return true;
    }
    return false;
}

void print_layer_font_text(uint16_t *render_buffer, const char *text, uint8_t length, uint16_t x,
                           uint16_t y, uint8_t factor, uint16_t color, uint16_t bg_color) {
    factor = CLAMP(factor, 1, 5);
    uint16_t glyph_width = (7 * factor) + 1;
    uint16_t glyph_height = (9 * factor) + 1;
    uint16_t gap = factor + 1;
    for (uint8_t i = 0; i < length; i++) {
        if (make_mac_layer_glyph(text[i], factor, factor, layer_glyph_bitmap)) {
            render_bitmap(render_buffer, layer_glyph_bitmap, x + i * (glyph_width + gap), y,
                          glyph_width, glyph_height, 1, color, bg_color);
        }
    }
}

static void print_layer_label(void) {
    if (current_layer.label == NULL) {
        return;
    }

    const uint16_t pane_x = 9;
    const uint16_t pane_y = 31;
    const uint16_t pane_width = 222;
    const uint16_t pane_height = 56;
    const uint16_t max_text_width = 206;
    size_t len = strlen(current_layer.label);
    uint8_t x_factor = 3;
    uint8_t y_factor = x_factor;
    uint16_t glyph_width = (7 * x_factor) + 1;
    uint16_t glyph_height = (9 * y_factor) + 1;
    uint16_t gap = x_factor + 1;
    uint16_t total_width = len > 0 ? (len * glyph_width) + ((len - 1) * gap) : 0;

    while (total_width > max_text_width && x_factor > 1) {
        x_factor--;
        glyph_width = (7 * x_factor) + 1;
        gap = x_factor + 1;
        total_width = (len * glyph_width) + ((len - 1) * gap);
    }

    print_filled_rounded_screen_area(pane_x, pane_y, pane_width, pane_height, 6,
                                     get_layer_font_bg_color());

    uint16_t x = pane_x + (pane_width - MIN(total_width, pane_width)) / 2;
    uint16_t y = pane_y + (pane_height - glyph_height) / 2;
    for (size_t i = 0; i < len && x + glyph_width <= pane_x + pane_width; i++) {
        if (make_mac_layer_glyph(current_layer.label[i], x_factor, y_factor,
                                 layer_glyph_bitmap)) {
            render_bitmap(scaled_bitmap_layer_font, layer_glyph_bitmap, x, y, glyph_width,
                          glyph_height, 1,
                          get_layer_font_color(), get_layer_font_bg_color());
        }
        x += glyph_width + gap;
    }

}

void print_layer() {
    if (current_layer.label == NULL) {
        return;
    }

    /* Full draw is reserved for initial screen creation and theme changes. */
    print_filled_rounded_screen_area(9, 31, 222, 56, 6, get_layer_font_bg_color());
    print_layer_label();
}

static void layer_status_update_cb(struct layer_status_state state) {
    current_layer = state;
    if (layer_widget_running) {
        print_layer_label();
    }
}

static struct layer_status_state layer_status_get_state(const zmk_event_t *eh) {
    uint8_t index = zmk_keymap_highest_layer_active();
    return (struct layer_status_state){.index = index, .label = zmk_keymap_layer_name(index)};
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_layer_status, struct layer_status_state, layer_status_update_cb,
                            layer_status_get_state)

ZMK_SUBSCRIPTION(widget_layer_status, zmk_layer_state_changed);

void zmk_widget_layer_init() {
    scaled_bitmap_layer_font = k_malloc((7 * 5 + 1) * (9 * 5 + 1) * sizeof(uint16_t));
    widget_layer_status_init();
}

void start_layer_status() { layer_widget_running = true; }

void stop_layer_status() { layer_widget_running = false; }
