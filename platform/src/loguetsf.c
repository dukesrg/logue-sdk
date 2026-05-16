/*
 *  File: loguetsf.c
 *
 *  SoundFont unit header for drumlogue and microKORG2
 *
 *  2026 (c) Oleg Burdaev
 *  mailto: dukesrg@gmail.com
 */

#include "logue_wrap.h"
#include "logue_perf.h"

const __unit_header UNIT_HEADER_TYPE unit_header = {
    .header_size = sizeof(UNIT_HEADER_TYPE),
    .target = UNIT_HEADER_TARGET_VALUE,
    .api = UNIT_API_VERSION,
    .dev_id = 0x44756B65U,
    .unit_id = 0x4653544CU,
    .version = 0x00010001U,
    .name = UNIT_NAME,
    .num_presets = 0,
    .num_params = PARAM_COUNT,
    .params = {
        {0, 127, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"SF File"}},
        {0, 511, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Preset"}},
        {0, 127, 0, 0, k_unit_param_type_none, 0, k_unit_param_frac_mode_fixed, 0, {"Voices"}},
        {0, 1, 0, 0, k_unit_param_type_onoff, 0, k_unit_param_frac_mode_fixed, 0, {"Sustain"}},
#ifdef UNIT_TARGET_PLATFORM_MICROKORG2
        {0, 127, 0, 127, k_unit_param_type_none, 0, k_unit_param_frac_mode_fixed, 0, {"Velocity"}},
#elif defined(UNIT_TARGET_PLATFORM_DRUMLOGUE)
        {0, 127, 0, 60, k_unit_param_type_midi_note, 0, k_unit_param_frac_mode_fixed, 0, {"Note"}},
#endif
    }
};
