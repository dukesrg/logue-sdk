/*
 *  File: header.c
 *
 *  loguePADS Synth header.
 *
 *
 *  2023-2024 (c) Oleg Burdaev
 *  mailto: dukesrg@gmail.com
 */

#include "unit.h"

#define STR(x) #x
#define IDX(x) STR(x)

#ifdef LAYER_XFADE_RATE_BITS
#define LAYER_XFADE_RATE_MAX ((1 << (LAYER_XFADE_RATE_BITS + 7)) - (1 << LAYER_XFADE_RATE_BITS))
#if LOGUEPAD == 2
#define LAYER_XFADE_RATE_MIN 0
#elif LOGUEPAD == 4
#define LAYER_XFADE_RATE_MIN (-(1 << (LAYER_XFADE_RATE_BITS + 7)))
#endif
#endif

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_synth,
    .api = UNIT_API_VERSION,
    .dev_id = 0x656B7544U,
    .version = 0x00000200U,
    .name = "loguePAD"IDX(LOGUEPAD),
    .num_presets = 0,
    .num_params = PARAM_COUNT,
#if LOGUEPAD == 2
    .unit_id = 0x3230504CU,
    .params = {
        {0, 127, 60, 60, k_unit_param_type_midi_note, 0, k_unit_param_frac_mode_fixed, 0, {"Note"}},
        {0, 6, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Group"}},
        {0, 3, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Mode 1"}},
        {0, 3, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Mode 2"}},
        {0, 384, 0, 1, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Sample 1"}},
        {0, 384, 0, 2, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Sample 1"}},
        {-6000, 6700, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Tune 1"}},
        {-6000, 6700, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Tune 2"}},
        {-1000, 240, 0, 0, k_unit_param_type_db, 1, k_unit_param_frac_mode_decimal, 0, {"Level 1"}},
        {-1000, 240, 0, 0, k_unit_param_type_db, 1, k_unit_param_frac_mode_decimal, 0, {"Level 2"}},
        {-3000, 3000, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Decay 1"}},
        {-3000, 3000, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Decay 2"}},
        {-999, 999, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Start 1"}},
        {-999, 999, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Start 2"}},
        {1, 1000, 0, 1000, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"End 1"}},
        {1, 1000, 0, 1000, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"End 2"}},
        {LAYER_XFADE_RATE_MIN, LAYER_XFADE_RATE_MAX, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Low N1"}},
        {LAYER_XFADE_RATE_MIN, LAYER_XFADE_RATE_MAX, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Low N2"}},
        {LAYER_XFADE_RATE_MIN, LAYER_XFADE_RATE_MAX, 0, LAYER_XFADE_RATE_MAX, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"High N1"}},
        {LAYER_XFADE_RATE_MIN, LAYER_XFADE_RATE_MAX, 0, LAYER_XFADE_RATE_MAX, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"High N2"}},
        {LAYER_XFADE_RATE_MIN, LAYER_XFADE_RATE_MAX, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Low V1"}},
        {LAYER_XFADE_RATE_MIN, LAYER_XFADE_RATE_MAX, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Low V2"}},
        {LAYER_XFADE_RATE_MIN, LAYER_XFADE_RATE_MAX, 0, LAYER_XFADE_RATE_MAX, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"High V1"}},
        {LAYER_XFADE_RATE_MIN, LAYER_XFADE_RATE_MAX, 0, LAYER_XFADE_RATE_MAX, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"High V2"}},
    }
#elif LOGUEPAD == 4
    .unit_id = 0x3430504CU,
    .params = {
        {0, 127, 60, 60, k_unit_param_type_midi_note, 0, k_unit_param_frac_mode_fixed, 0, {"Note"}},
        {0, 6, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Group"}},
        {0, 15, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Mode 1-2"}},
        {0, 15, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Mode 3-4"}},
        {0, 384, 0, 1, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Sample 1"}},
        {0, 384, 0, 2, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Sample 1"}},
        {0, 384, 0, 3, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Sample 3"}},
        {0, 384, 0, 4, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Sample 4"}},
        {-6000, 6700, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Tune 1"}},
        {-6000, 6700, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Tune 2"}},
        {-6000, 6700, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Tune 3"}},
        {-6000, 6700, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Tune 4"}},
        {-1000, 240, 0, 0, k_unit_param_type_db, 1, k_unit_param_frac_mode_decimal, 0, {"Level 1"}},
        {-1000, 240, 0, 0, k_unit_param_type_db, 1, k_unit_param_frac_mode_decimal, 0, {"Level 2"}},
        {-1000, 240, 0, 0, k_unit_param_type_db, 1, k_unit_param_frac_mode_decimal, 0, {"Level 3"}},
        {-1000, 240, 0, 0, k_unit_param_type_db, 1, k_unit_param_frac_mode_decimal, 0, {"Level 4"}},
        {LAYER_XFADE_RATE_MIN, LAYER_XFADE_RATE_MAX, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Low 1"}},
        {LAYER_XFADE_RATE_MIN, LAYER_XFADE_RATE_MAX, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Low 2"}},
        {LAYER_XFADE_RATE_MIN, LAYER_XFADE_RATE_MAX, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Low 3"}},
        {LAYER_XFADE_RATE_MIN, LAYER_XFADE_RATE_MAX, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Low 4"}},
        {LAYER_XFADE_RATE_MIN, LAYER_XFADE_RATE_MAX, 0, LAYER_XFADE_RATE_MAX, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"High 1"}},
        {LAYER_XFADE_RATE_MIN, LAYER_XFADE_RATE_MAX, 0, LAYER_XFADE_RATE_MAX, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"High 2"}},
        {LAYER_XFADE_RATE_MIN, LAYER_XFADE_RATE_MAX, 0, LAYER_XFADE_RATE_MAX, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"High 3"}},
        {LAYER_XFADE_RATE_MIN, LAYER_XFADE_RATE_MAX, 0, LAYER_XFADE_RATE_MAX, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"High 4"}},
    }
#elif LOGUEPAD == 8
    .unit_id = 0x3830504CU,
    .params = {
        {0, 127, 60, 60, k_unit_param_type_midi_note, 0, k_unit_param_frac_mode_fixed, 0, {"Note"}},
        {0, 1295, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Group"}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 63, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Mode 1-2"}},
        {0, 63, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Mode 3-4"}},
        {0, 63, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Mode 5-6"}},
        {0, 63, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Mode 7-8"}},
        {0, 384, 0, 1, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Sample 1"}},
        {0, 384, 0, 2, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Sample 2"}},
        {0, 384, 0, 3, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Sample 3"}},
        {0, 384, 0, 4, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Sample 4"}},
        {0, 384, 0, 5, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Sample 5"}},
        {0, 384, 0, 6, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Sample 6"}},
        {0, 384, 0, 7, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Sample 7"}},
        {0, 384, 0, 8, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Sample 8"}},
        {-6000, 6700, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Tune 1"}},
        {-6000, 6700, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Tune 2"}},
        {-6000, 6700, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Tune 3"}},
        {-6000, 6700, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Tune 4"}},
        {-6000, 6700, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Tune 5"}},
        {-6000, 6700, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Tune 6"}},
        {-6000, 6700, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Tune 7"}},
        {-6000, 6700, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Tune 8"}},
    }
#elif LOGUEPAD == 16
    .unit_id = 0x3631504CU,
    .params = {
        {0, 127, 60, 60, k_unit_param_type_midi_note, 0, k_unit_param_frac_mode_fixed, 0, {"Note"}},
        {0, 1295, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Group"}},
        {0, 63, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Mode 1-2"}},
        {0, 63, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Mode 3-4"}},
        {0, 511, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Mode 5-7"}},
        {0, 511, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Mode 8-10"}},
        {0, 511, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Mode 11-13"}},
        {0, 511, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Mode 14-16"}},
        {0, 384, 0, 1, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Sample 1"}},
        {0, 384, 0, 2, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Sample 2"}},
        {0, 384, 0, 3, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Sample 3"}},
        {0, 384, 0, 4, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Sample 4"}},
        {0, 384, 0, 5, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Sample 5"}},
        {0, 384, 0, 6, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Sample 6"}},
        {0, 384, 0, 7, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Sample 7"}},
        {0, 384, 0, 8, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Sample 8"}},
        {0, 384, 0, 9, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Sample 9"}},
        {0, 384, 0, 10, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Sample10"}},
        {0, 384, 0, 11, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Sample11"}},
        {0, 384, 0, 12, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Sample12"}},
        {0, 384, 0, 13, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Sample13"}},
        {0, 384, 0, 14, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Sample14"}},
        {0, 384, 0, 15, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Sample15"}},
        {0, 384, 0, 16, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Sample16"}},
    }
#endif
};
