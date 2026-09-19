/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_input_processor_trackpad_mode

#include <zephyr/device.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>

#include <drivers/input_processor.h>
#include <dt-bindings/zmk/modifiers.h>
#include <zmk/hid.h>
#include <zmk/keymap.h>

#define LOWER_LAYER 1
#define RAISE_LAYER 2

static bool pointer_mode_active(void) {
    zmk_mod_flags_t modifiers = zmk_hid_get_explicit_mods();

    return zmk_keymap_layer_active(LOWER_LAYER) || zmk_keymap_layer_active(RAISE_LAYER) ||
           (modifiers & (MOD_LCTL | MOD_RCTL)) != 0;
}

static int trackpad_mode_handle_event(const struct device *dev, struct input_event *event,
                                      uint32_t param1, uint32_t param2,
                                      struct zmk_input_processor_state *state) {
    if (event->type != INPUT_EV_REL || !pointer_mode_active()) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    if (event->code == INPUT_REL_HWHEEL) {
        event->code = INPUT_REL_X;
    } else if (event->code == INPUT_REL_WHEEL) {
        event->code = INPUT_REL_Y;
        event->value = -event->value;
    }

    return ZMK_INPUT_PROC_CONTINUE;
}

static struct zmk_input_processor_driver_api trackpad_mode_driver_api = {
    .handle_event = trackpad_mode_handle_event,
};

#define TRACKPAD_MODE_INST(n)                                                                      \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, NULL, NULL, POST_KERNEL,                                  \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &trackpad_mode_driver_api);

DT_INST_FOREACH_STATUS_OKAY(TRACKPAD_MODE_INST)
