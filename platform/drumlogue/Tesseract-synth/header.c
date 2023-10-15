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
    .version = 0x00000201U,                                // This unit's version: major.minor.patch (major<<16 minor<<8 patch).
    .name = "Tesseract",                                       // Name for this unit, will be displayed on device
    .num_presets = 0,                                      // Number of internal presets this unit has
    .num_params = PARAM_COUNT,                                       // Number of parameters for this unit, max 24
    .params = {
        {0, 127, 0, 60, k_unit_param_type_midi_note, 0, k_unit_param_frac_mode_fixed, 0, {"Note"}},
        {0, 32767, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Wave"}},        
        {0, 624, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Type"}},
        {0, 80, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Overflow"}},
// Position X Y Z W
        {-128, 128, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Pos X"}},
        {-128, 128, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Pos Y"}},
        {-128, 128, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Pos Z"}},
        {-128, 128, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Pos W"}},
// LFO rate X Y Z W
        {0, 1280, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Rate X"}},
        {0, 1280, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Rate Y"}},
        {0, 1280, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Rate Z"}},
        {0, 1280, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Rate W"}},
// LFO depth X Y Z W
        {-128, 128, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Depth X"}},
        {-128, 128, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Depth Y"}},
        {-128, 128, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Depth Z"}},
        {-128, 128, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Depth W"}},
// LFO wave X Y Z W
        {0, 32767, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Form X"}}, 
        {0, 32767, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Form Y"}}, 
        {0, 32767, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Form Z"}}, 
        {0, 32767, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Form W"}}, 
// Dimension X Y Z W
        {1, 4096, 1, 1, k_unit_param_type_none, 0, k_unit_param_frac_mode_fixed, 0, {"Dim X"}},
        {1, 4096, 1, 1, k_unit_param_type_none, 0, k_unit_param_frac_mode_fixed, 0, {"Dim Y"}},
        {1, 4096, 1, 1, k_unit_param_type_none, 0, k_unit_param_frac_mode_fixed, 0, {"Dim Z"}},
        {1, 4096, 1, 1, k_unit_param_type_none, 0, k_unit_param_frac_mode_fixed, 0, {"Dim W"}},
}};
