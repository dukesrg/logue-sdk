/*
 *  File: header.c
 *
 *  FM64 Synth unit header.
 *
 *
 *  2022-2023 (c) Oleg Burdaev
 *  mailto: dukesrg@gmail.com
 */

#include "unit.h"
const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_synth,
    .api = UNIT_API_VERSION,
    .dev_id = 0x656B7544U,
    .unit_id = 0x34364D46U,
    .version = 0x00020004U,
    .name = "FM64",
    .num_presets = 0,
    .num_params = 24,
    .params = {
        {0, 127, 0, 60, k_unit_param_type_midi_note, 0, k_unit_param_frac_mode_fixed, 0, {"Note"}},
        {0, 32767, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Voice"}},
        {0, 423, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Mode"}},
        {-100, 100, 0, 0, k_unit_param_type_cents, 0, k_unit_param_frac_mode_fixed, 0, {"Detune"}},

        {0, 85, 0,0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"Alg."}},
        {0, 32767, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"C.Wave"}},
        {0, 32767, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"M.Wave"}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},

        {-112, 128, 0, 0, k_unit_param_type_oct, 4, k_unit_param_frac_mode_fixed, 0, {"FB1 offs"}},
        {0, 36, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"FB1 path"}},
        {-112, 128, 0, 0, k_unit_param_type_oct, 4, k_unit_param_frac_mode_fixed, 0, {"FB2 offs"}},
        {0, 36, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"FB2 path"}},

        {-396, 396, 0, 0, k_unit_param_type_oct, 2, k_unit_param_frac_mode_fixed, 0, {"C.Level"}},
        {-396, 396, 0, 0, k_unit_param_type_oct, 2, k_unit_param_frac_mode_fixed, 0, {"M.Level"}},
        {-396, 396, 0, 0, k_unit_param_type_oct, 2, k_unit_param_frac_mode_fixed, 0, {"C.Rate"}},
        {-396, 396, 0, 0, k_unit_param_type_oct, 2, k_unit_param_frac_mode_fixed, 0, {"M.Rate"}},

        {-396, 396, 0, 0, k_unit_param_type_oct, 2, k_unit_param_frac_mode_fixed, 0, {"C.KLS"}},
        {-396, 396, 0, 0, k_unit_param_type_oct, 2, k_unit_param_frac_mode_fixed, 0, {"M.KLS"}},
        {-112, 112, 0, 0, k_unit_param_type_oct, 4, k_unit_param_frac_mode_fixed, 0, {"C.KRS"}},
        {-112, 112, 0, 0, k_unit_param_type_oct, 4, k_unit_param_frac_mode_fixed, 0, {"M.KRS"}},

        {-112, 112, 0, 0, k_unit_param_type_oct, 4, k_unit_param_frac_mode_fixed, 0, {"C.KVS"}},
        {-112, 112, 0, 0, k_unit_param_type_oct, 4, k_unit_param_frac_mode_fixed, 0, {"M.KVS"}},
        {-100, 100, 0, 0, k_unit_param_type_cents, 0, k_unit_param_frac_mode_fixed, 0, {"C.Det."}},
        {-100, 100, 0, 0, k_unit_param_type_cents, 0, k_unit_param_frac_mode_fixed, 0, {"M.Det."}}
    }
};
