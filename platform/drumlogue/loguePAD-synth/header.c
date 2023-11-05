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
    .unit_id = 0x4441506CU,                                       // Id for this unit, should be unique within the scope of a given dev_id
    .version = 0x00000001U,                                // This unit's version: major.minor.patch (major<<16 minor<<8 patch).
    .name = "loguePAD",                                       // Name for this unit, will be displayed on device
    .num_presets = 0,                                      // Number of internal presets this unit has
    .num_params = PARAM_COUNT,                                       // Number of parameters for this unit, max 24
    .params = {
        {0, 127, 60, 60, k_unit_param_type_midi_note, 0, k_unit_param_frac_mode_fixed, 0, {"Note"}},
//        {0, 3, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Zone 1"}},
//        {0, 16, 0, 8, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"< Split >"}},
//        {0, 3, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Zone 2"}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
// Playback
        {0, 255, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Pb 1-4"}},
        {0, 255, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Pb 5-8"}},
        {0, 255, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Pb 9-12"}},
        {0, 255, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Pb 13-16"}},
// Sample
        {0, 384, 0, 129, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Smpl 1"}},
        {0, 384, 0, 130, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Smpl 2"}},
        {0, 384, 0, 131, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Smpl 3"}},
        {0, 384, 0, 132, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Smpl 4"}},
        {0, 384, 0, 133, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Smpl 5"}},
        {0, 384, 0, 134, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Smpl 6"}},
        {0, 384, 0, 135, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Smpl 7"}},
        {0, 384, 0, 136, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Smpl 8"}},
        {0, 384, 0, 137, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Smpl 9"}},
        {0, 384, 0, 138, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Smpl 10"}},
        {0, 384, 0, 139, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Smpl 11"}},
        {0, 384, 0, 140, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Smpl 12"}},
        {0, 384, 0, 141, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Smpl 13"}},
        {0, 384, 0, 142, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Smpl 14"}},
        {0, 384, 0, 143, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Smpl 15"}},
        {0, 384, 0, 144, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Smpl 16"}},
}};
