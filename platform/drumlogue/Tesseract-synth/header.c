/**
 *  @file header.c
 *  @brief drumlogue SDK unit header
 *
 *  Copyright (c) 2020-2022 KORG Inc. All rights reserved.
 *
 */

#include "unit.h"  // Note: Include common definitions for all units

// ---- Unit header definition  --------------------------------------------------------------------

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),                  // leave as is, size of this header
    .target = UNIT_TARGET_PLATFORM | k_unit_module_synth,  // target platform and module for this unit
    .api = UNIT_API_VERSION,                               // logue sdk API version against which unit was built
    .dev_id = 0x656B7544U,                                        // developer identifier
    .unit_id = 0x73736554U,                                       // Id for this unit, should be unique within the scope of a given dev_id
    .version = 0x00000001U,                                // This unit's version: major.minor.patch (major<<16 minor<<8 patch).
    .name = "Tesseract",                                       // Name for this unit, will be displayed on device
    .num_presets = 0,                                      // Number of internal presets this unit has
    .num_params = PARAM_COUNT,                                       // Number of parameters for this unit, max 24
    .params = {
        // Format: min, max, center, default, type, fractional, frac. type, <reserved>, name

        // See common/runtime.h for type enum and unit_param_t structure

// Position X Y Z W
        {0, (63 << 9), 0, 0, k_unit_param_type_none, 9, k_unit_param_frac_mode_fixed, 0, {"Position X"}},
        {0, (63 << 9), 0, 0, k_unit_param_type_none, 9, k_unit_param_frac_mode_fixed, 0, {"Position Y"}},
        {0, (63 << 9), 0, 0, k_unit_param_type_none, 9, k_unit_param_frac_mode_fixed, 0, {"Position Z"}},
        {0, (63 << 9), 0, 0, k_unit_param_type_none, 9, k_unit_param_frac_mode_fixed, 0, {"Position W"}},
// LFO rate X Y Z W
        {0, 10000, 0, 0, k_unit_param_type_hertz, 3, k_unit_param_frac_mode_decimal, 0, {"LFO Rate X"}},
        {0, 10000, 0, 0, k_unit_param_type_hertz, 3, k_unit_param_frac_mode_decimal, 0, {"LFO Rate Y"}},
        {0, 10000, 0, 0, k_unit_param_type_hertz, 3, k_unit_param_frac_mode_decimal, 0, {"LFO Rate Z"}},
        {0, 10000, 0, 0, k_unit_param_type_hertz, 3, k_unit_param_frac_mode_decimal, 0, {"LFO Rate W"}},
// LFO depth X Y Z W
        {-(63 << 9), (63 << 9), 0, 0, k_unit_param_type_none, 9, k_unit_param_frac_mode_fixed, 0, {"LFO Depth X"}},
        {-(63 << 9), (63 << 9), 0, 0, k_unit_param_type_none, 9, k_unit_param_frac_mode_fixed, 0, {"LFO Depth Y"}},
        {-(63 << 9), (63 << 9), 0, 0, k_unit_param_type_none, 9, k_unit_param_frac_mode_fixed, 0, {"LFO Depth Z"}},
        {-(63 << 9), (63 << 9), 0, 0, k_unit_param_type_none, 9, k_unit_param_frac_mode_fixed, 0, {"LFO Depth W"}},
// LFO mode X Y Z W
        {0, 48, 0, 0, k_unit_param_type_strings, 0, 0, 0, {"LFO Mode X"}},
        {0, 48, 0, 0, k_unit_param_type_strings, 0, 0, 0, {"LFO Mode Y"}},
        {0, 48, 0, 0, k_unit_param_type_strings, 0, 0, 0, {"LFO Mode Z"}},
        {0, 48, 0, 0, k_unit_param_type_strings, 0, 0, 0, {"LFO Mode W"}},
// Dimension X Y Z W
        {1, 4096, 1, 1, k_unit_param_type_none, 0, k_unit_param_frac_mode_fixed, 0, {"Dimension X"}},
        {1, 4096, 1, 1, k_unit_param_type_none, 0, k_unit_param_frac_mode_fixed, 0, {"Dimension Y"}},
        {1, 4096, 1, 1, k_unit_param_type_none, 0, k_unit_param_frac_mode_fixed, 0, {"Dimension Z"}},
        {1, 4096, 1, 1, k_unit_param_type_none, 0, k_unit_param_frac_mode_fixed, 0, {"Dimension W"}},
// Wave bank sample length offset
        {0, 127, 0, 0, k_unit_param_type_none, 0, k_unit_param_frac_mode_fixed, 0, {"Wave Bank"}},
        {0, 127, 0, 0, k_unit_param_type_none, 0, k_unit_param_frac_mode_fixed, 0, {"Wave Sample"}},
        {0, 12, 0, 8, k_unit_param_type_strings, 0, 0, 0, {"Wave Size"}},
        {0, 0x7FFF, 0, 0, k_unit_param_type_none, 0, k_unit_param_frac_mode_fixed, 0, {"Wave Offset"}}
}};
