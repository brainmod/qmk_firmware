// Copyright 2023 Colin Lam (Ploopy Corporation)
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// Custom DPI options
#define PLOOPY_DPI_OPTIONS { 1600, 2000, 2400, 3000 }
#define PLOOPY_DPI_DEFAULT 2

#define POINTING_DEVICE_HIRES_SCROLL_ENABLE 0
#define POINTING_DEVICE_HIRES_SCROLL_MULTIPLIER 15

#define PLOOPY_DRAGSCROLL_DIVISOR_H 4.0
#define PLOOPY_DRAGSCROLL_DIVISOR_V 4.0

// Invert vertical scroll direction
#define PLOOPY_DRAGSCROLL_INVERT

// Enable hybrid dragscroll mode (tap to toggle, hold to momentary)
#define PLOOPY_DRAGSCROLL_HYBRID

// Enable sticky axis (locks to horizontal OR vertical based on velocity)
#define PLOOPY_DRAGSCROLL_STICKY_AXIS

// Auto-disable dragscroll when clicking mouse buttons
#define PLOOPY_DRAGSCROLL_DISABLE_ON_CLICK
