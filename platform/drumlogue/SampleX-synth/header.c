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
    .unit_id = 0x58706D53U,                                       // Id for this unit, should be unique within the scope of a given dev_id
    .version = 0x00000101U,                                // This unit's version: major.minor.patch (major<<16 minor<<8 patch).
    .name = "SampleX",                                       // Name for this unit, will be displayed on device
    .num_presets = 0,                                      // Number of internal presets this unit has
    .num_params = PARAM_COUNT,                                       // Number of parameters for this unit, max 24
    .params = {
        {0, 127, 60, 60, k_unit_param_type_midi_note, 0, k_unit_param_frac_mode_fixed, 0, {"Note"}},
        {0, 2, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Track"}},
        {0, 15, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Playback"}},
        {0, 255, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Trigger"}},
// Sample
        {0, 384, 0, 129, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Sample 1"}},
        {0, 384, 0, 130, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Sample 1"}},
        {0, 384, 0, 131, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Sample 3"}},
        {0, 384, 0, 132, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Sample 4"}},
// Level
        {-1000, 240, 0, 0, k_unit_param_type_db, 1, k_unit_param_frac_mode_decimal, 0, {"Level 1"}},
        {-1000, 240, 0, 0, k_unit_param_type_db, 1, k_unit_param_frac_mode_decimal, 0, {"Level 2"}},
        {-1000, 240, 0, 0, k_unit_param_type_db, 1, k_unit_param_frac_mode_decimal, 0, {"Level 3"}},
        {-1000, 240, 0, 0, k_unit_param_type_db, 1, k_unit_param_frac_mode_decimal, 0, {"Level 4"}},
// Tune
        {-6000, 6700, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Tune 1"}},
        {-6000, 6700, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Tune 2"}},
        {-6000, 6700, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Tune 3"}},
        {-6000, 6700, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Tune 4"}},
// Low threshold
        {-128, 127, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Low 1"}},
        {-128, 127, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Low 2"}},
        {-128, 127, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Low 3"}},
        {-128, 127, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Low 4"}},
// High threshold
        {-128, 127, 0, 127, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"High 1"}},
        {-128, 127, 0, 127, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"High 2"}},
        {-128, 127, 0, 127, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"High 3"}},
        {-128, 127, 0, 127, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"High 4"}},
}};
