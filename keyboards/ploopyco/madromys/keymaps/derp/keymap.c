// Copyright 2023 Colin Lam (Ploopy Corporation)
// Copyright 2020 Christopher Courtney, aka Drashna Jael're  (@drashna) <drashna@live.com>
// Copyright 2019 Sunjun Kim
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [0] = LAYOUT(MS_BTN3, MS_BTN4, MS_BTN5, DRAG_SCROLL, MS_BTN1, MS_BTN2)
};
