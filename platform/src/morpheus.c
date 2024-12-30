/*
 *  File: morpheus.c
 *
 *  Morphing wavetable oscillator unit header
 *
 *  2024 (c) Oleg Burdaev
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
#ifdef UNIT_TARGET_PLATFORM_NTS1_MKII
        {0, 1023, 0, 0, k_unit_param_type_none, 0, k_unit_param_frac_mode_fixed, 0, {"LFO X"}},
        {0, 1023, 0, 0, k_unit_param_type_none, 0, k_unit_param_frac_mode_fixed, 0, {"LFO Y"}},
#endif
        {0, 1023, 0, 0, k_unit_param_type_none, 0, k_unit_param_frac_mode_fixed, 0, {"LFO X"}},
        {0, 1023, 0, 0, k_unit_param_type_none, 0, k_unit_param_frac_mode_fixed, 0, {"LFO Y"}},
        {0, LFO_MODE_COUNT - 1, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"LFO X mode"}},
        {0, LFO_MODE_COUNT, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"LFO Y mode"}},
        {0, LFO_WAVEFORM_COUNT - 1, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"LFO X wave"}},
        {0, LFO_WAVEFORM_COUNT - 1, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"LFO Y wave"}},
        {-100, 100, 0, 0, k_unit_param_type_none, 0, k_unit_param_frac_mode_fixed, 0, {"LFO X depth"}},
        {-100, 100, 0, 0, k_unit_param_type_none, 0, k_unit_param_frac_mode_fixed, 0, {"LFO Y depth"}}
    }
#ifdef UNIT_TARGET_PLATFORM_NTS3_KAOSS
    },
    .default_mappings = {
        {k_genericfx_param_assign_x, k_genericfx_curve_linear, k_genericfx_curve_unipolar, 0, 1023, 0},
        {k_genericfx_param_assign_y, k_genericfx_curve_linear, k_genericfx_curve_unipolar, 0, 1023, 0},
        {k_genericfx_param_assign_none, k_genericfx_curve_linear, k_genericfx_curve_unipolar, 0, LFO_MODE_COUNT - 1, 0},
        {k_genericfx_param_assign_none, k_genericfx_curve_linear, k_genericfx_curve_unipolar, 0, LFO_MODE_COUNT, 0},
        {k_genericfx_param_assign_none, k_genericfx_curve_linear, k_genericfx_curve_unipolar, 0, LFO_WAVEFORM_COUNT - 1, 0},
        {k_genericfx_param_assign_none, k_genericfx_curve_linear, k_genericfx_curve_unipolar, 0, LFO_WAVEFORM_COUNT - 1, 0},
        {k_genericfx_param_assign_none, k_genericfx_curve_linear, k_genericfx_curve_unipolar, -100, 100, 0},
        {k_genericfx_param_assign_none, k_genericfx_curve_linear, k_genericfx_curve_unipolar, -100, 100, 0}
    }
#endif
};
