/*
 *  File: header.c
 *
 *  Hyperloop FX header.
 *
 *
 *  2023-2024 (c) Oleg Burdaev
 *  mailto: dukesrg@gmail.com
 */

#include "unit.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | UNIT_TARGET_MODULE,
    .api = UNIT_API_VERSION,
    .dev_id = 0x656B7544U,
    .unit_id = 0x706F6F4C,
    .version = 0x00000300U,
    .name = "Hyperloop",
    .num_presets = 0,
    .num_params = PARAM_COUNT,
    .params = {
        {0, 1, 0, 0, k_unit_param_type_bitmaps, 0, k_unit_param_frac_mode_fixed, 0, {"Play 1"}},
        {0, 1, 0, 0, k_unit_param_type_bitmaps, 0, k_unit_param_frac_mode_fixed, 0, {"Play 2"}},
        {0, 1, 0, 0, k_unit_param_type_bitmaps, 0, k_unit_param_frac_mode_fixed, 0, {"Play 3"}},
        {0, 1, 0, 0, k_unit_param_type_bitmaps, 0, k_unit_param_frac_mode_fixed, 0, {"Play 4"}},
        {0, 1, 0, 0, k_unit_param_type_bitmaps, 0, k_unit_param_frac_mode_fixed, 0, {"Play 5"}},
        {0, 1, 0, 0, k_unit_param_type_bitmaps, 0, k_unit_param_frac_mode_fixed, 0, {"Play 6"}},
        {0, 1, 0, 0, k_unit_param_type_bitmaps, 0, k_unit_param_frac_mode_fixed, 0, {"Play 7"}},
        {0, 1, 0, 0, k_unit_param_type_bitmaps, 0, k_unit_param_frac_mode_fixed, 0, {"Play 8"}},
        {0, 1, 0, 0, k_unit_param_type_bitmaps, 0, k_unit_param_frac_mode_fixed, 0, {"Record 1"}},
        {0, 1, 0, 0, k_unit_param_type_bitmaps, 0, k_unit_param_frac_mode_fixed, 0, {"Record 2"}},
        {0, 1, 0, 0, k_unit_param_type_bitmaps, 0, k_unit_param_frac_mode_fixed, 0, {"Record 3"}},
        {0, 1, 0, 0, k_unit_param_type_bitmaps, 0, k_unit_param_frac_mode_fixed, 0, {"Record 4"}},
        {0, 1, 0, 0, k_unit_param_type_bitmaps, 0, k_unit_param_frac_mode_fixed, 0, {"Record 5"}},
        {0, 1, 0, 0, k_unit_param_type_bitmaps, 0, k_unit_param_frac_mode_fixed, 0, {"Record 6"}},
        {0, 1, 0, 0, k_unit_param_type_bitmaps, 0, k_unit_param_frac_mode_fixed, 0, {"Record 7"}},
        {0, 1, 0, 0, k_unit_param_type_bitmaps, 0, k_unit_param_frac_mode_fixed, 0, {"Record 8"}},
        {0, 15, 0, 6, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Length 1"}},
        {0, 15, 0, 6, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Length 2"}},
        {0, 15, 0, 6, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Length 3"}},
        {0, 15, 0, 6, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Length 4"}},
        {0, 15, 0, 6, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Length 5"}},
        {0, 15, 0, 6, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Length 6"}},
        {0, 15, 0, 6, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Length 7"}},
        {0, 15, 0, 6, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Length 8"}},
    }
};
