/*
 * Copyright (c) 2019 Jan Van Winkel <jan.van_winkel@dxplore.eu>
 *
 * Based on ST7789V sample:
 * Copyright (c) 2019 Marc Reilly
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zephyr/kernel.h>
#include <drivers/behavior.h>
#include <zephyr/bluetooth/services/bas.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zmk/display.h>
#include <zmk/display/widgets/layer_status.h>
// #include <zmk_dongle_events/dongle_action_event.h>
#include <zmk/event_manager.h>
#include <zmk/endpoints.h>
#include <zmk/keymap.h>
#include <zmk/display.h>

#include "action_button.h"
#include "splash.h"
#include "snake.h"
#include "output_status.h"
#include "battery_status.h"
#include "layer_status.h"
#include "helpers/display.h"
#include "helpers/buzzer.h"
#include "helpers/settings.h"
#include "theme.h"
#include "wpm.h"
// #include "snake_image.h"
#include "logo.h"
#include "assets/copilot_logo_rgb565.h"
#include "assets/excel_logo_rgb565.h"
#include <stdint.h>
#include <zephyr/random/random.h>

static uint16_t menu_threshold = 0;
static uint16_t theme_threshold = 300;
static uint16_t mute_threshold = 600;
static int64_t pressed_timestamp = 0;
static int64_t released_timestamp = 0;

static bool action_button_initialized = false;
static struct layer_status_state ls_state;

static bool menu_on = false;
static bool dongle_lock = false;
static bool screen_saver_running = false;
static uint8_t current_screen_saver = UINT8_MAX;
static struct k_work_delayable screen_saver_work;

static const uint8_t base_layer = 0;
static const uint8_t menu_layer = 4;
static const uint8_t theme_layer = 5;
static const uint8_t mute_layer = 6;

#define SCREEN_SAVER_INTERVAL K_SECONDS(300)

enum screen_saver_kind {
    SCREEN_SAVER_WINDOWS,
    SCREEN_SAVER_EXCEL,
    SCREEN_SAVER_COPILOT,
    SCREEN_SAVER_COUNT,
};

struct layer_status_state {
    uint8_t index;
    const char *label;
};

void set_theme_threshold(uint16_t term_ms) { theme_threshold = term_ms; }

void set_mute_threshold(uint16_t term_ms) { mute_threshold = term_ms; }

static void apply_windows_status_colors(void) {
    const uint32_t ink = 0xF5F3FF;
    const uint32_t card = 0x111827;
    const uint32_t border = 0x334155;
    const uint32_t secondary = 0xA7B0C5;

    set_menu_bg_color(card);
    set_frame_color(ink);
    set_frame_color_1(border);
    set_layer_font_color(ink);
    set_layer_font_bg_color(card);
    set_wpm_font_color(ink);
    set_wpm_font_1_color(secondary);
    set_wpm_font_bg_color(card);
    set_symbol_selected_color(ink);
    set_symbol_unselected_color(secondary);
    set_symbol_bg_color(0x070B14);
    set_bt_num_color(ink);
    set_bt_bg_color(0x070B14);
    set_bt_status_ok_color(0x4ADE80);
    set_bt_status_not_ok_color(0xFB7185);
    set_bt_status_open_color(0xFBBF24);
    set_bt_status_bg_color(0x070B14);
    set_battery_num_color(ink);
    set_battery_percentage_color(secondary);
    set_battery_bg_color(card);
    set_battery_num_color_1(ink);
    set_battery_percentage_color_1(secondary);
    set_battery_bg_color_1(card);
    set_modifier_selected_color(0x38BDF8);
    set_modifier_unselected_color(secondary);
    set_modifier_bg_color(card);
    set_theme_font_color(ink);
    set_theme_font_color_1(secondary);
    set_theme_font_bg_color(card);
}

void print_frames() {
    const uint16_t canvas = rgb888_to_rgb565(0x070B14);
    const uint16_t card = get_menu_bg_color();
    const uint16_t border = rgb888_to_rgb565(0x334155);
    const uint16_t purple = rgb888_to_rgb565(0x8B5CF6);

    print_filled_screen_area(0, 0, 240, 240, canvas);
    print_filled_rounded_screen_area(8, 30, 224, 58, 7, purple);
    print_filled_rounded_screen_area(9, 31, 222, 56, 6, card);
#ifndef CONFIG_SHOW_SINGLE_BATTERY
    print_filled_rounded_screen_area(8, 94, 109, 68, 7, border);
    print_filled_rounded_screen_area(9, 95, 107, 66, 6, card);
    print_filled_rounded_screen_area(123, 94, 109, 68, 7, border);
    print_filled_rounded_screen_area(124, 95, 107, 66, 6, card);
#else
    print_filled_rounded_screen_area(66, 94, 108, 68, 7, border);
    print_filled_rounded_screen_area(67, 95, 106, 66, 6, card);
#endif
}

void print_menu() {
    stop_screen_saver();
    invalidate_splash();
    apply_windows_status_colors();
    stop_animation();
    print_frames();
    start_battery_status();
    start_output_status();
    start_modifier_status();
    start_layer_status();
    set_battery_symbol();
    print_layer();
    print_themes();
    print_modifiers();
}

static void print_windows_screen_saver(void) {
    const uint16_t background = rgb888_to_rgb565(0x0F172A);
    const uint16_t blue = rgb888_to_rgb565(0x60A5FA);
    const uint16_t dim_blue = rgb888_to_rgb565(0x2563EB);

    print_filled_screen_area(0, 0, 240, 240, background);
    print_filled_screen_area(76, 76, 41, 41, blue);
    print_filled_screen_area(123, 76, 41, 41, blue);
    print_filled_screen_area(76, 123, 41, 41, dim_blue);
    print_filled_screen_area(123, 123, 41, 41, dim_blue);
    print_filled_screen_area(98, 183, 44, 2, dim_blue);
}

static void print_screen_saver_image(const uint8_t *image, uint16_t width, uint16_t height) {
    struct display_buffer_descriptor descriptor = {
        .buf_size = width * height * 2u,
        .pitch = width,
        .width = width,
        .height = height,
    };
    display_write_wrapper((240 - width) / 2, (240 - height) / 2, &descriptor, (uint8_t *)image);
}

static void print_excel_screen_saver(void) {
    print_filled_screen_area(0, 0, 240, 240, rgb888_to_rgb565(0x0B2016));
    print_screen_saver_image(excel_logo_rgb565, EXCEL_LOGO_WIDTH, EXCEL_LOGO_HEIGHT);
}

static void print_copilot_screen_saver(void) {
    print_filled_screen_area(0, 0, 240, 240, rgb888_to_rgb565(0x101827));
    print_screen_saver_image(copilot_logo_rgb565, COPILOT_LOGO_WIDTH, COPILOT_LOGO_HEIGHT);
}

static uint8_t next_screen_saver(void) {
    if (current_screen_saver >= SCREEN_SAVER_COUNT) {
        return sys_rand32_get() % SCREEN_SAVER_COUNT;
    }

    uint8_t next = sys_rand32_get() % (SCREEN_SAVER_COUNT - 1);
    return next >= current_screen_saver ? next + 1 : next;
}

static void draw_next_screen_saver(void) {
    current_screen_saver = next_screen_saver();
    switch (current_screen_saver) {
    case SCREEN_SAVER_WINDOWS:
        print_windows_screen_saver();
        break;
    case SCREEN_SAVER_EXCEL:
        print_excel_screen_saver();
        break;
    case SCREEN_SAVER_COPILOT:
        print_copilot_screen_saver();
        break;
    default:
        break;
    }
}

static void screen_saver_work_handler(struct k_work *work) {
    ARG_UNUSED(work);
    if (!screen_saver_running) {
        return;
    }

    draw_next_screen_saver();
    k_work_schedule(&screen_saver_work, SCREEN_SAVER_INTERVAL);
}

void start_screen_saver(void) {
    screen_saver_running = true;
    k_work_cancel_delayable(&screen_saver_work);
    draw_next_screen_saver();
    k_work_schedule(&screen_saver_work, SCREEN_SAVER_INTERVAL);
}

void stop_screen_saver(void) {
    screen_saver_running = false;
    k_work_cancel_delayable(&screen_saver_work);
}

void toggle_menu() {
#ifdef CONFIG_USE_BUZZER
#ifdef CONFIG_USE_MENU_SOUND
    play_notification_song();
#endif
#endif
    if (menu_on) {
        stop_modifier_status();
        stop_output_status();
        stop_battery_status();
        stop_animation();
        stop_layer_status();
        start_snake();
        menu_on = false;
    } else {
        stop_snake();
        print_menu();
        menu_on = true;
    }
}

void change_theme() {
    set_next_theme();
#ifdef CONFIG_USE_BUZZER
#ifdef CONFIG_USE_THEME_SOUND
    play_startup_song();
#endif
#endif
    if (menu_on) {
        print_menu();
        apply_theme_snake();
    } else {
        stop_snake();
        apply_theme_snake();
        start_snake();
    }
}

void set_layer_symbol() {
    if (dongle_lock) {
        return;
    }
    dongle_lock = true;
    if (ls_state.index == menu_layer) {
        toggle_menu();
    }
    if (ls_state.index == theme_layer) {
        change_theme();
    }
    if (ls_state.index == mute_layer) {
#ifdef CONFIG_USE_BUZZER
        snake_settings_toggle_mute();
        if (!snake_settings_get_mute()) {
            play_once(coin);
        }
#endif
    }
    dongle_lock = false;
}

void zmk_widget_action_button_init() {
    // dongle_action_init();

    k_work_init_delayable(&screen_saver_work, screen_saver_work_handler);
}

void start_action_button(bool is_menu_on) {
    menu_on = is_menu_on;
    action_button_initialized = true;
}
