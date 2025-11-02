/* Copyright 2020 Christopher Courtney, aka Drashna Jael're  (@drashna) <drashna@live.com>
 * Copyright 2019 Sunjun Kim
 * Copyright 2020 Ploopy Corporation
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ploopyco.h"
#include "analog.h"
#include "opt_encoder.h"

// for legacy support
#if defined(OPT_DEBOUNCE) && !defined(PLOOPY_SCROLL_DEBOUNCE)
#    define PLOOPY_SCROLL_DEBOUNCE OPT_DEBOUNCE
#endif
#if defined(SCROLL_BUTT_DEBOUNCE) && !defined(PLOOPY_SCROLL_BUTTON_DEBOUNCE)
#    define PLOOPY_SCROLL_BUTTON_DEBOUNCE SCROLL_BUTT_DEBOUNCE
#endif

#ifndef PLOOPY_SCROLL_DEBOUNCE
#    define PLOOPY_SCROLL_DEBOUNCE 5
#endif
#ifndef PLOOPY_SCROLL_BUTTON_DEBOUNCE
#    define PLOOPY_SCROLL_BUTTON_DEBOUNCE 100
#endif

#ifndef PLOOPY_DPI_OPTIONS
#    define PLOOPY_DPI_OPTIONS \
        { 600, 900, 1200, 1600, 2400 }
#    ifndef PLOOPY_DPI_DEFAULT
#        define PLOOPY_DPI_DEFAULT 1
#    endif
#endif
#ifndef PLOOPY_DPI_DEFAULT
#    define PLOOPY_DPI_DEFAULT 0
#endif
#ifndef PLOOPY_DRAGSCROLL_DIVISOR_H
#    define PLOOPY_DRAGSCROLL_DIVISOR_H 8.0
#endif
#ifndef PLOOPY_DRAGSCROLL_DIVISOR_V
#    define PLOOPY_DRAGSCROLL_DIVISOR_V 8.0
#endif
#ifndef ENCODER_BUTTON_ROW
#    define ENCODER_BUTTON_ROW 0
#endif
#ifndef ENCODER_BUTTON_COL
#    define ENCODER_BUTTON_COL 0
#endif

// Sticky axis configuration
#ifdef PLOOPY_DRAGSCROLL_STICKY_AXIS
#    ifndef PLOOPY_STICKY_AXIS_HISTORY_SIZE
#        define PLOOPY_STICKY_AXIS_HISTORY_SIZE 30
#    endif
#    ifndef PLOOPY_STICKY_AXIS_SAMPLE_FREQ
#        define PLOOPY_STICKY_AXIS_SAMPLE_FREQ 10
#    endif
#endif

keyboard_config_t keyboard_config;
uint16_t          dpi_array[] = PLOOPY_DPI_OPTIONS;
#define DPI_OPTION_SIZE ARRAY_SIZE(dpi_array)

// Trackball State
bool  is_scroll_clicked    = false;
bool  is_drag_scroll       = false;
float scroll_accumulated_h = 0;
float scroll_accumulated_v = 0;

#ifdef PLOOPY_DRAGSCROLL_HYBRID
static bool     dragscroll_toggled = false;
static bool     scroll_detected    = false;
static uint16_t dragscroll_timer   = 0;
#endif

#ifdef PLOOPY_DRAGSCROLL_STICKY_AXIS
typedef struct {
    int8_t   x_delta[PLOOPY_STICKY_AXIS_HISTORY_SIZE];
    int8_t   y_delta[PLOOPY_STICKY_AXIS_HISTORY_SIZE];
    uint16_t timestamp[PLOOPY_STICKY_AXIS_HISTORY_SIZE];
    uint8_t  head;
    uint8_t  tail;
} axis_history_t;

static axis_history_t axis_history = {0};
#endif

#ifdef ENCODER_ENABLE
uint16_t lastScroll        = 0; // Previous confirmed wheel event
uint16_t lastMidClick      = 0; // Stops scrollwheel from being read if it was pressed
pin_t    encoder_pins_a[1] = ENCODER_A_PINS;
pin_t    encoder_pins_b[1] = ENCODER_B_PINS;
bool     debug_encoder     = false;

bool encoder_update_kb(uint8_t index, bool clockwise) {
    if (!encoder_update_user(index, clockwise)) {
        return false;
    }
#    ifdef MOUSEKEY_ENABLE
    tap_code(clockwise ? MS_WHLU : MS_WHLD);
#    else
    report_mouse_t mouse_report = pointing_device_get_report();
    mouse_report.v              = clockwise ? 1 : -1;
    pointing_device_set_report(mouse_report);
    pointing_device_send();
#    endif
    return true;
}

void encoder_driver_init(void) {
    for (uint8_t i = 0; i < ARRAY_SIZE(encoder_pins_a); i++) {
        gpio_set_pin_input(encoder_pins_a[i]);
        gpio_set_pin_input(encoder_pins_b[i]);
    }
    opt_encoder_init();
}

void encoder_driver_task(void) {
    uint16_t p1 = analogReadPin(encoder_pins_a[0]);
    uint16_t p2 = analogReadPin(encoder_pins_b[0]);

    if (debug_encoder) dprintf("OPT1: %d, OPT2: %d\n", p1, p2);

    int8_t dir = opt_encoder_handler(p1, p2);
    // If the mouse wheel was just released, do not scroll.
    if (timer_elapsed(lastMidClick) < PLOOPY_SCROLL_BUTTON_DEBOUNCE) {
        return;
    }

    // Limit the number of scrolls per unit time.
    if (timer_elapsed(lastScroll) < PLOOPY_SCROLL_DEBOUNCE) {
        return;
    }

    // Don't scroll if the middle button is depressed.
    if (is_scroll_clicked) {
#    ifndef PLOOPY_IGNORE_SCROLL_CLICK
        return;
#    endif
    }

    if (dir == 0) return;
    encoder_queue_event(0, dir > 0);
    lastScroll = timer_read();
}
#endif

void toggle_drag_scroll(void) {
    is_drag_scroll ^= 1;
}

void cycle_dpi(void) {
    keyboard_config.dpi_config = (keyboard_config.dpi_config + 1) % DPI_OPTION_SIZE;
    eeconfig_update_kb(keyboard_config.raw);
    pointing_device_set_cpi(dpi_array[keyboard_config.dpi_config]);
}

#ifdef PLOOPY_DRAGSCROLL_STICKY_AXIS
static void axis_history_reset(void) {
    axis_history.head = 0;
    axis_history.tail = 0;
    for (uint8_t i = 0; i < PLOOPY_STICKY_AXIS_HISTORY_SIZE; i++) {
        axis_history.x_delta[i] = 0;
        axis_history.y_delta[i] = 0;
        axis_history.timestamp[i] = 0;
    }
}

static void axis_history_push(int8_t x, int8_t y, uint16_t now) {
    // Advance head if enough time has passed
    if (timer_elapsed(axis_history.timestamp[axis_history.head]) > PLOOPY_STICKY_AXIS_SAMPLE_FREQ) {
        axis_history.head = (axis_history.head + 1) % PLOOPY_STICKY_AXIS_HISTORY_SIZE;

        // If head caught up to tail, advance tail
        if (axis_history.head == axis_history.tail) {
            axis_history.tail = (axis_history.tail + 1) % PLOOPY_STICKY_AXIS_HISTORY_SIZE;
        }

        axis_history.timestamp[axis_history.head] = now;
        axis_history.x_delta[axis_history.head] = 0;
        axis_history.y_delta[axis_history.head] = 0;
    }

    // Accumulate deltas for current sample
    axis_history.x_delta[axis_history.head] += x;
    axis_history.y_delta[axis_history.head] += y;
}

static bool should_scroll_horizontally(void) {
    float velocity_x = 0.0f;
    float velocity_y = 0.0f;
    uint8_t sample_count = 0;

    // Calculate average velocity over history
    uint8_t prev_idx = axis_history.tail;
    uint8_t curr_idx = (axis_history.tail + 1) % PLOOPY_STICKY_AXIS_HISTORY_SIZE;

    while (curr_idx != (axis_history.head + 1) % PLOOPY_STICKY_AXIS_HISTORY_SIZE) {
        uint16_t time_delta = timer_elapsed(axis_history.timestamp[curr_idx]) -
                              timer_elapsed(axis_history.timestamp[prev_idx]);

        if (time_delta > 0) {
            velocity_x += (float)abs(axis_history.x_delta[curr_idx]) / (float)time_delta;
            velocity_y += (float)abs(axis_history.y_delta[curr_idx]) / (float)time_delta;
            sample_count++;
        }

        prev_idx = curr_idx;
        curr_idx = (curr_idx + 1) % PLOOPY_STICKY_AXIS_HISTORY_SIZE;
    }

    if (sample_count == 0) {
        return false;  // Default to vertical
    }

    velocity_x /= (float)sample_count;
    velocity_y /= (float)sample_count;

    return velocity_x > velocity_y;
}
#endif

report_mouse_t pointing_device_task_kb(report_mouse_t mouse_report) {
    mouse_report = pointing_device_task_user(mouse_report);
    if (is_drag_scroll) {
        scroll_accumulated_h += (float)mouse_report.x / PLOOPY_DRAGSCROLL_DIVISOR_H;
        scroll_accumulated_v += (float)mouse_report.y / PLOOPY_DRAGSCROLL_DIVISOR_V;

#ifdef PLOOPY_DRAGSCROLL_STICKY_AXIS
        // Track movement history for sticky axis detection
        axis_history_push(mouse_report.x, mouse_report.y, timer_read());

        // Determine scroll direction based on velocity history
        if (should_scroll_horizontally()) {
            mouse_report.h = (int8_t)scroll_accumulated_h;
            mouse_report.v = 0;
        } else {
#    ifdef PLOOPY_DRAGSCROLL_INVERT
            mouse_report.v = -(int8_t)scroll_accumulated_v;
#    else
            mouse_report.v = (int8_t)scroll_accumulated_v;
#    endif
            mouse_report.h = 0;
        }
#else
        // Standard dual-axis scrolling
        mouse_report.h = (int8_t)scroll_accumulated_h;
#    ifdef PLOOPY_DRAGSCROLL_INVERT
        mouse_report.v = -(int8_t)scroll_accumulated_v;
#    else
        mouse_report.v = (int8_t)scroll_accumulated_v;
#    endif
#endif

        // Update accumulated scroll values by subtracting the integer parts
        scroll_accumulated_h -= (int8_t)scroll_accumulated_h;
        scroll_accumulated_v -= (int8_t)scroll_accumulated_v;

#ifdef PLOOPY_DRAGSCROLL_HYBRID
        // Track that scrolling occurred for tap-to-toggle detection
        if (mouse_report.h != 0 || mouse_report.v != 0) {
            scroll_detected = true;
        }
#endif

        // Clear cursor movement during drag scroll
        mouse_report.x = 0;
        mouse_report.y = 0;
    }

    return mouse_report;
}

bool process_record_kb(uint16_t keycode, keyrecord_t* record) {
    if (debug_mouse) {
        dprintf("KL: kc: %u, col: %u, row: %u, pressed: %u\n", keycode, record->event.key.col, record->event.key.row, record->event.pressed);
    }

    // Update Timer to prevent accidental scrolls
#ifdef ENCODER_ENABLE
    if ((record->event.key.col == ENCODER_BUTTON_COL) && (record->event.key.row == ENCODER_BUTTON_ROW)) {
        lastMidClick      = timer_read();
        is_scroll_clicked = record->event.pressed;
    }
#endif

    if (!process_record_user(keycode, record)) {
        return false;
    }

    if (keycode == DPI_CONFIG && record->event.pressed) {
        cycle_dpi();
    }

    if (keycode == DRAG_SCROLL) {
#ifdef PLOOPY_DRAGSCROLL_HYBRID
        // Hybrid mode: tap to toggle, hold to momentary
        if (record->event.pressed) {
            // Key pressed - enable dragscroll
            if (!is_drag_scroll) {
                is_drag_scroll = true;
                scroll_detected = false;
                dragscroll_timer = timer_read();
#    ifdef PLOOPY_DRAGSCROLL_STICKY_AXIS
                axis_history_reset();
                axis_history.timestamp[0] = dragscroll_timer;
#    endif
            } else {
                // Already enabled (via toggle) - turn it off
                is_drag_scroll = false;
                dragscroll_toggled = false;
                return false;
            }
        } else {
            // Key released - decide between toggle and momentary
            if (!scroll_detected && timer_elapsed(dragscroll_timer) < TAPPING_TERM) {
                // Short tap without scrolling = toggle on
                dragscroll_toggled = true;
                is_drag_scroll = true;
            } else if (!dragscroll_toggled) {
                // Released after scrolling or long hold = disable (was momentary)
                is_drag_scroll = false;
            }
            // If toggled on, keep it on
        }
#elif defined(PLOOPY_DRAGSCROLL_MOMENTARY)
        // Momentary mode: only active while held
        is_drag_scroll = record->event.pressed;
#    ifdef PLOOPY_DRAGSCROLL_STICKY_AXIS
        if (is_drag_scroll) {
            axis_history_reset();
            axis_history.timestamp[0] = timer_read();
        }
#    endif
#else
        // Toggle mode: press to toggle on/off
        if (record->event.pressed) {
            toggle_drag_scroll();
#    ifdef PLOOPY_DRAGSCROLL_STICKY_AXIS
            if (is_drag_scroll) {
                axis_history_reset();
                axis_history.timestamp[0] = timer_read();
            }
#    endif
        }
#endif
    }

#ifdef PLOOPY_DRAGSCROLL_DISABLE_ON_CLICK
    // Auto-disable dragscroll on mouse button clicks
    if (is_drag_scroll && (keycode == MS_BTN1 || keycode == MS_BTN2) && record->event.pressed) {
        is_drag_scroll = false;
#    ifdef PLOOPY_DRAGSCROLL_HYBRID
        dragscroll_toggled = false;
#    endif
    }
#endif

    return true;
}

// Hardware Setup
void keyboard_pre_init_kb(void) {
    // debug_enable  = true;
    // debug_matrix  = true;
    // debug_mouse   = true;
    // debug_encoder = true;

    /* Ground all output pins connected to ground. This provides additional
     * pathways to ground. If you're messing with this, know this: driving ANY
     * of these pins high will cause a short. On the MCU. Ka-blooey.
     */
#ifdef UNUSABLE_PINS
    const pin_t unused_pins[] = UNUSABLE_PINS;

    for (uint8_t i = 0; i < ARRAY_SIZE(unused_pins); i++) {
        gpio_set_pin_output_push_pull(unused_pins[i]);
        gpio_write_pin_low(unused_pins[i]);
    }
#endif

    // This is the debug LED.
#if defined(DEBUG_LED_PIN)
    gpio_set_pin_output_push_pull(DEBUG_LED_PIN);
    gpio_write_pin(DEBUG_LED_PIN, debug_enable);
#endif

    keyboard_pre_init_user();
}

void pointing_device_init_kb(void) {
    keyboard_config.raw = eeconfig_read_kb();
    if (keyboard_config.dpi_config > DPI_OPTION_SIZE) {
        eeconfig_init_kb();
    }
    pointing_device_set_cpi(dpi_array[keyboard_config.dpi_config]);
}

void eeconfig_init_kb(void) {
    keyboard_config.dpi_config = PLOOPY_DPI_DEFAULT;
    eeconfig_update_kb(keyboard_config.raw);
    eeconfig_init_user();
}
