/*
 *  File: morpheus.c
 *
 *  Morphing wavetable oscillator unit header
 *
 *  2024-2025 (c) Oleg Burdaev
 *  mailto: dukesrg@gmail.com
 */

#include "logue_wrap.h"

const __unit_header UNIT_HEADER_TYPE unit_header = {
#ifdef UNIT_TARGET_PLATFORM_NTS3_KAOSS
    .common = {
#endif
    .header_size = sizeof(UNIT_HEADER_TYPE),
    .target = UNIT_HEADER_TARGET_VALUE,
    .api = UNIT_API_VERSION,
    .dev_id = 0x44756B65U,
    .unit_id = 0x4850524DU,
    .version = 0x00020000U,
    .name = UNIT_NAME,
    .num_params = PARAM_COUNT,
    .params = {
#ifdef UNIT_TARGET_MODULE_OSC
        {0, 1023, 0, 0, k_unit_param_type_none, 0, k_unit_param_frac_mode_fixed, 0, {"X Value"}},
        {0, 1023, 0, 0, k_unit_param_type_none, 0, k_unit_param_frac_mode_fixed, 0, {"Y Value"}},
#endif
#ifdef UNIT_TARGET_MODULE_OSC
        {0, 1023, 0, 0, k_unit_param_type_none, 0, k_unit_param_frac_mode_fixed, 0, {"X Value"}},
        {0, 1023, 0, 0, k_unit_param_type_none, 0, k_unit_param_frac_mode_fixed, 0, {"Y Value"}},
#endif
        {0, LFO_MODE_COUNT * LFO_MODE_COUNT - 1, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Modes"}},
        {0, 10, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Dimen."}},
        {0, LFO_WAVEFORM_COUNT - 1, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"X Wave"}},
        {0, LFO_WAVEFORM_COUNT - 1, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Y Wave"}},
        {-100, 100, 0, 0, k_unit_param_type_none, 0, k_unit_param_frac_mode_fixed, 0, {"X Depth"}},
        {-100, 100, 0, 0, k_unit_param_type_none, 0, k_unit_param_frac_mode_fixed, 0, {"Y Depth"}},
#ifndef UNIT_TARGET_MODULE_OSC
        {0, 1023, 0, 480, k_unit_param_type_midi_note, 0, k_unit_param_frac_mode_fixed, 0, {"Pitch"}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, k_unit_param_frac_mode_fixed, 0, {""}},
#endif
    }
#ifdef UNIT_TARGET_PLATFORM_NTS3_KAOSS
    },
    .default_mappings = {
        {k_genericfx_param_assign_none, k_genericfx_curve_linear, k_genericfx_curve_unipolar, 0, LFO_MODE_COUNT * LFO_MODE_COUNT - 1, 0},
        {k_genericfx_param_assign_none, k_genericfx_curve_linear, k_genericfx_curve_unipolar, 0, LFO_MODE_COUNT, 0},
        {k_genericfx_param_assign_none, k_genericfx_curve_linear, k_genericfx_curve_unipolar, 0, LFO_WAVEFORM_COUNT - 1, 0},
        {k_genericfx_param_assign_none, k_genericfx_curve_linear, k_genericfx_curve_unipolar, 0, LFO_WAVEFORM_COUNT - 1, 0},
        {k_genericfx_param_assign_none, k_genericfx_curve_linear, k_genericfx_curve_unipolar, -100, 100, 0},
        {k_genericfx_param_assign_none, k_genericfx_curve_linear, k_genericfx_curve_unipolar, -100, 100, 0},
        {k_genericfx_param_assign_depth, k_genericfx_curve_linear, k_genericfx_curve_unipolar, 0, 1023, 480},
        {k_genericfx_param_assign_none, k_genericfx_curve_linear, k_genericfx_curve_unipolar, 0, 0, 0},
    }
#endif
};
