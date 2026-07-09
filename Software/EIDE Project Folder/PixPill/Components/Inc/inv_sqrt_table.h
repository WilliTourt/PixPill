#pragma once

#include <cstdint>

// Compact inv_sqrt LUT: 512 float entries = 2048 bytes
// Store 1/sqrt(dist_sq) directly, no conversion math needed
// index = (int)(dist_sq * 128), clamped to [0, 511]
// Covers dist_sq ∈ [0, ~4.0], step = 1/128 = 0.0078125

#define INV_SQRT_TABLE_SIZE      512
#define INV_SQRT_TABLE_SCALE     128    // multiply dist_sq by this to get index

float inv_sqrt_lookup(float dist_sq);

extern const float inv_sqrt_table[INV_SQRT_TABLE_SIZE];
