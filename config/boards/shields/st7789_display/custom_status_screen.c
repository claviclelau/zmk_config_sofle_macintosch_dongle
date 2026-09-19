/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "custom_status_screen.h"
#include "widgets/battery_status.h"
#include "widgets/output_status.h"
#include "widgets/splash.h"
#include "widgets/snake.h"
#include "widgets/helpers/display.h"
#include "widgets/action_button.h"
#include "widgets/logo.h"
#include "widgets/configuration.h"
#include "widgets/wpm.h"
#include "widgets/modifier.h"
#include "widgets/layer_status.h"
#include <zmk/activity.h>
#include <zmk/events/activity_state_changed.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define DASHBOARD_START_DELAY_MS 50

static bool dashboard_ready = false;

/* Defer the first draw until ZMK has loaded the LVGL screen. */
void timer_splash(lv_timer_t *timer) {
    if (dashboard_ready) {
        return;
    }

    initialize_battery_status();
    print_menu();
    lv_timer_pause(timer);
    dashboard_ready = true;
}

static int activity_listener_cb(const zmk_event_t *eh) {
    const struct zmk_activity_state_changed *event = as_zmk_activity_state_changed(eh);
    if (!event)
        return 0;

    if (!dashboard_ready)
        return 0;

    if (event->state == ZMK_ACTIVITY_ACTIVE) {
        LOG_INF("Keyboard active: refresh dashboard");
        stop_screen_saver();
        initialize_battery_status();
        print_menu();
    } else {
        LOG_INF("Keyboard idle: show minimal screen saver");
        stop_wpm_status();
        stop_modifier_status();
        stop_output_status();
        stop_battery_status();
        stop_animation();
        stop_layer_status();
        start_screen_saver();
    }

    return 0;
}

ZMK_LISTENER(custom_status_activity_listener, activity_listener_cb);
ZMK_SUBSCRIPTION(custom_status_activity_listener, zmk_activity_state_changed);

/* === 显示初始化 === */
lv_obj_t *zmk_display_status_screen() {
    configure();
    init_display();
    theme_init();

    zmk_widget_splash_init();
    zmk_widget_output_status_init();
    zmk_widget_peripheral_battery_status_init();
    zmk_widget_layer_init();
    zmk_widget_action_button_init();
    zmk_widget_wpm_init();
    zmk_widget_modifier_init();

    lv_timer_create(timer_splash, DASHBOARD_START_DELAY_MS, NULL);

    return lv_obj_create(NULL);
}
