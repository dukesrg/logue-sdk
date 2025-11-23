/*
 *  File: vault.c
 *
 *  Lirbarian unit header for drumlogue and microKORG2
 *
 *  2025 (c) Oleg Burdaev
 *  mailto: dukesrg@gmail.com
 */

#include "logue_wrap.h"
#include "logue_perf.h"

#if defined(UNIT_TARGET_PLATFORM_MICROKORG2) && !defined(UNIT_TARGET_MODULE_OSC)
#include "FxDefines.h"
#define RESERVED kMk2FxParamModeIgnoreKnobStateAndModulation
#else
#define RESERVED 0
#endif

const __unit_header UNIT_HEADER_TYPE unit_header = {
    .header_size = sizeof(UNIT_HEADER_TYPE),
    .target = UNIT_HEADER_TARGET_VALUE,
    .api = UNIT_API_VERSION,
    .dev_id = 0x44756B65U,
    .unit_id = 0x544c4056U,
    .version = 0x00010000U,
    .name = UNIT_NAME,
    .num_presets = 0,
    .num_params = PARAM_COUNT,
    .params = {
        {0, BANK_COUNT * (GENRE_COUNT + 1) + 1, 0, 0, k_unit_param_type_strings, RESERVED, k_unit_param_frac_mode_fixed, 0, {"Bank"}},
#ifdef UNIT_TARGET_PLATFORM_DRUMLOGUE
        {0, 2, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, RESERVED, {"Content"}},
#endif
        {0, 127, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, RESERVED, {"File"}},
        {0, 127, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, RESERVED, {"Export"}},
        {0, 127, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, RESERVED, {"File"}},
#ifdef UNIT_TARGET_PLATFORM_DRUMLOGUE
        {0, 2, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, RESERVED, {"Content"}},
#endif
        {0, BANK_COUNT * (GENRE_COUNT + 1) + 1, 0, 0, k_unit_param_type_strings, RESERVED, k_unit_param_frac_mode_fixed, 0, {"Bank"}},
        {0, 127, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, RESERVED, {"Import"}},
        {0, 99, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, RESERVED, {"Set"}},
#ifdef PERFMON_ENABLE
        PERFMON_PARAM,
#else
        {0, 1, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, RESERVED, {"Format"}},
#endif
    }
};
