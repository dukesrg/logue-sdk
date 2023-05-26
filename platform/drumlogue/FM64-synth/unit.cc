/*
 *  File: unit.cc
 *
 *  FM64 Synth unit.
 *
 *
 *  2022-2023 (c) Oleg Burdaev
 *  mailto: dukesrg@gmail.com
 */

#include "unit.h"

#include <cstddef>
#include <cstdint>

#include <arm_neon.h>

#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>

#include "attributes.h"
#include "fastpow.h"
#include "arm.h"
#include "wavetable.h"

#define TWEAK_ALG //use reserved bits for extended algorithms count support
#define TWEAK_WF //use reserved bits for extended waveforms count support
#include "fm64.h"

#define POLYPHONY 4
#define MAX_CHORD 4

#define MAX_SAMPLES_LOAD 256 //maximum number of user bank samples to accomodate as voice banks and wavetables, each

#ifndef M_LN10
  #define M_LN10 2.30258509299404568402f
#endif

#define clipminmax(a,b,c) (((b)>(c))?(c):((b)<(a))?(a):(b))
#define clipmin(a,b) ((b)<(a)?(a):(b))
#define clipmax(b,c) ((b)>(c)?(c):(b))

#define PITCH_BEND_CENTER 8192
#define PITCH_BEND_SENSITIVITY .0001220703125f  // 24/8192
#define SAMPLERATE_RECIP 2.0833333e-5f  // 1/48000
#define OCTAVE_RECIP .083333333f        // 1/12
#define NOTE_FREQ_OFFSET -150.232645f  // 12 * log2(440/48000) - 69


//#define OP6 //6-operator support
//#define OP4 //4-operator support
//#define WF32 //all 8 DX11 waveforms from PCM32 wavebank
//#define WF16 //all 8 DX11 waveforms from PCM16 wavebank
//#define WF8 //all 8 DX11 waveforms runtime generated from half-sine
//#define WF4 //4 first DX11 waveforms runtime generated from half-sine
//#define WF2 //2 first DX11 waveforms runtime generated from half-sine
//#define WFROM //logue SDK wave banks A-F
//#define OPSIX //enable KORG Opsix extensions
//#define SY77 //enable SY77 extensions
#define FEEDBACK_COUNT 2
#define WFBITS 7

//#define PEG //pitch EG enable (~530-600 bytes)
//#define PEG_RATE_LUT //PEG Rate from LUT close to DX7, instead of approximated function (~44-176 bytes)
#define FINE_TUNE //16-bit precision for cents/detune
//#define KIT_MODE //key tracking to voice (- ~112 bytes)
#define SPLIT_ZONES 3
//#define MOD16 //16-bit mod matrix processing

#ifdef WAVE_PINCH
  #define WAVE_PINCH_PARAMS , s_wavewidth[i * 2], s_wavewidth[i * 2 + 1]
#else
  #define WAVE_PINCH_PARAMS
#endif

//  #define EGLUT //use precalculated EG LUT, saves ~140 bytes of code
//#define USE_Q31
#if defined(EGLUT)
  #include "eglut.h"
  #define EG_LUT_SHR 20
#elif defined(EGLUT11)
  #include "eglut11.h"
  #define EG_LUT_SHR 19
#elif defined(EGLUT12)
  #include "eglut12.h"
  #define EG_LUT_SHR 18
#elif defined(EGLUT13)
  #include "eglut13.h"
  #define EG_LUT_SHR 17
#endif

#if defined(EGLUTX15)
//  #define param_eglut(a,b) (ldrsh_lsl((int32_t)eg_lut, usat_asr(31, q31add(a,b), (EG_LUT_SHR + 1)), 1) << 16)
#elif defined(EGLUTX16)
//  #define param_eglut(a,b) (ldrh_lsl((int32_t)eg_lut, usat_asr(31, q31add(a,b), (EG_LUT_SHR + 1)), 1) << 15)
#else
  #define param_eglut(a,b) (ldr_lsl((int32_t)eg_lut, usat_asr(31, q31add(a,b), (EG_LUT_SHR + 1)), 2))
#endif

//#define FEEDBACK_RECIP 0x00FFFFFF // <1/128 - pre-multiplied by 2 for simplified Q31 multiply by always positive
//#define FEEDBACK_RECIPF .00390625f // 1/256 - pre-multiplied by 2 for simplified Q31 multiply by always positive
//#define FEEDBACK_RECIP_LOG2 (-8) // 1/256 - pre-multiplied by 2 for simplified Q31 multiply by always positive
#define FEEDBACK_RECIP_LOG2 (-9.f) // 1/512
#define LEVEL_SCALE_FACTOR 0x01000000 // -0.7525749892dB/96dB === 1/128
//#define DX7_SAMPLING_FREQ 49096.545017211284821233588006932f // 1/20.368032usec
//#define DX7_TO_LOGUE_FREQ 0.977665536f // 48000/49096.545
//-.0325870980969347836053763275142f // log2(DX7_TO_LOGUE_FREQ)
#define EG_FREQ_CORRECT .0325870980969347836053763275142f // log2(1/DX7_TO_LOGUE_FREQ)

#define DX7_RATE_EXP_FACTOR .16f // ? 16/99 = .16(16)
#define DX11_RATE_EXP_FACTOR .505f
#define DX11_RELEASE_RATE_EXP_FACTOR 1.04f
//#define DX7_ATTACK_RATE_FACTOR 5.0200803e-7f // 1/(41.5*48000)
//#define DX7_DECAY_RATE_FACTOR -5.5778670e-8f // -1/(9*41.5*48000)
//#define DX7_ATTACK_RATE1_FACTOR 5.0261359e-7f // 1/(41.45*48000)
//#define DX7_DECAY_RATE1_FACTOR -8.3768932e-8f // -1/(6*41.45*48000)
#define DX7_ATTACK_RATE_FACTOR 3.3507573e-7f // 1/(1.5*41.45*48000)
#define DX7_DECAY_RATE_FACTOR -5.5845955e-8f // -1/(1.5*6*41.45*48000)
#define DX7_RATE1_FACTOR 1.5f
//#define DX7_ATTACK_RATE_FACTOR 4.8773035424164220513138759143079e-7f // 1/(2^21 * DX7_TO_LOGUE_FREQ) = 1/(2^(21 - EG_FREQ_CORRECT)
//#define DX7_ATTACK_RATE_FACTOR 20.9674129f
//#define DX7_ATTACK_RATE_FACTOR (21.f - EG_FREQ_CORRECT)
// 2^24 samples @49k = 2^24 / 49k seconds = 2^24 * 48k / (48k * 49k) seconds = 2^24 * 48K / 49K samples @ 48K
//#define DX7_DECAY_RATE_FACTOR -6.0966294280205275641423448928849e-8f // -1/(2^24 * DX7_TO_LOGUE_FREQ)
//#define DX7_DECAY_RATE_FACTOR 23.9674129f
//#define DX7_DECAY_RATE_FACTOR (24.f - EG_FREQ_CORRECT)
//#define DX7_HOLD_RATE_FACTOR .51142234392928421688784987507221f // 1/(2^1 * DX7_TO_LOGUE_FREQ)
//#define DX7_HOLD_RATE_FACTOR 0.9674129f
//#define DX7_HOLD_RATE_FACTOR (1.f - EG_FREQ_CORRECT)
//#define RATE_SCALING_FACTOR .061421131f
//#define RATE_SCALING_FACTOR .041666667f
//#define RATE_SCALING_FACTOR .065040650f // 1/24 * 64/41
#define RATE_SCALING_FACTOR .445291664f // reversed from measures for current curve function
#define DX7_RATE_SCALING_FACTOR .142857143f // 1/7
#define DX11_RATE_SCALING_FACTOR .333333333f // 1/3

//#define DX7_LEVEL_SCALE_FACTOR 0.0267740885f // 109.(6)/4096
//#define DX7_LEVEL_SCALE_FACTOR 0.0222222222f // 1/45
#define DX7_LEVEL_SCALE_FACTOR 0.0200686664f
#define DX11_LEVEL_SCALE_FACTOR 0.0149253731f // 1/(103-36) C1...G6
//#define LEVEL_SCALE_FACTORF 0.0078740157f // 1/127
#define LEVEL_SCALE_FACTORF 0.0078125f // 1/128
#define LEVEL_SCALE_FACTOR_DB 0.0103810253f // 1/96dB
#define DX11_TO_DX7_LEVEL_SCALE_FACTOR 6.6f // 99/15
#define DX11_MAX_LEVEL 15

//#define FREQ_FACTOR .08860606f // (9.772 - 1)/99
//#define PEG_SCALE 0x00600000 // 48/128 * 256 * 65536
#define PEG_SCALE 0x00000060 // 48/128 * 256
#define PEG_RATE_SCALE 196.38618f; // ~ 192 >> 24 semitones per sample at 49096.545

//#define MI_SCALE_FACTOR 0x7A92BE8B // 3.830413123f >> 2
#define MI_SCALE_FACTOR 3.83041312f // 2 ^ (2 - 1/16)
//#define FINE_TUNE_FACTOR 65536.f

enum {
  param_gate_note = 0U,
  param_dxvoice_idx,
  param_mode,
  param_detune,
  param_algorithm,
  param_waveform_carriers,
  param_waveform_modulators,
  param_7,
  param_feedback1_offset,
  param_feedback1_route,
  param_feedback2_offset,
  param_feedback2_route,
  param_level_offset_carriers,
  param_level_offset_modulators,
  param_rate_offset_carriers,
  param_rate_offset_modulators,
  param_kls_offset_carriers,
  param_kls_offset_modulators,
  param_krs_offset_carriers,
  param_krs_offset_modulators,
  param_kvs_offset_carriers,
  param_kvs_offset_modulators,
  param_detune_offset_carriers,
  param_detune_offset_modulators,
};

enum {
  mode_poly = 0U,
  mode_duo,
  mode_unison,
  mode_chord,
};

enum {
  state_running = 0,
  state_noteon = 1,
  state_noteoff = 2
};

  static const dx_voice_t *dx7_init_voice_ptr = (dx_voice_t *)(const uint8_t[]){
    0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00, 0x02,
    0x00, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00,
    0x02, 0x00, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x00,
    0x00, 0x02, 0x00, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38,
    0x00, 0x00, 0x02, 0x00, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x38, 0x00, 0x00, 0x02, 0x00, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x38, 0x00, 0x63, 0x02, 0x00, 0x63, 0x63, 0x63, 0x63, 0x32, 0x32, 0x32, 0x32, 0x00, 0x08,
    0x23, 0x00, 0x00, 0x00, 0x31, 0x18, 0x49, 0x4E, 0x49, 0x54, 0x20, 0x56, 0x4F, 0x49, 0x43, 0x45
  };

  static int32_t sParams[PARAM_COUNT];

  static struct {
    const dx_voice_t * dxvoice_ptr;
    uint32_t count;
  } DxVoiceBanks[MAX_SAMPLES_LOAD] = {dx7_init_voice_ptr, 1};

  static wavetable_t Wavetables;

  static struct {
    uint32_t state;
    uint32_t note;
    float pitch;
    float detune_sens;
    float velocity;
    uint32_t algorithm_idx;
    uint32_t dxvoice_idx;
//    dx_voice_t * dxvoice_ptr;
    uint32_t opi;
    int32_t transpose;
    int32_t zone_transpose;
    float level_scale_factor;
  } SynVoices[POLYPHONY] = {};

  static struct chord_t {
    int32_t notes[MAX_CHORD - 1];
    const char *name;
  } sChords[] = {
    {{7}, "5th"},
    {{1, 6}, "sus#4"},
    {{2, 7}, "sus2"},
    {{3, 6}, "dim"},
    {{3, 7}, "m"},
    {{4, 7}, "Maj"},
    {{4, 8}, "aug"},
    {{4, 10}, "it6"},
    {{5, 7}, "sus4"},
    {{3, 6, 9}, "dim7"},
    {{3, 6, 10}, "m7b5"},
    {{3, 6, 1}, "Maj7b5"},
    {{3, 7, 9}, "m6"},
    {{3, 7, 10}, "m7"},
    {{3, 7, 11}, "mMaj7"},
    {{4, 6, 10}, "7b5"},
    {{4, 7, 9}, "Maj6"},
    {{4, 7, 10}, "7"},
    {{4, 7, 11}, "Maj7"},
    {{4, 8, 10}, "aug7"},
    {{4, 8, 11}, "Maj7#5"},
    {{5, 7, 10}, "7sus4"}
  };

  static float sPitchBend;
  static uint32_t gate_note;
  static float sDetune;
  static int32_t sChordNotes[MAX_CHORD - 1];

  static uint8_t s_algorithm[POLYPHONY][OPERATOR_COUNT] = {0};

/* Modulation matrix mode
1 - original 2x4x6 with integrated feedback
2 - original 2x4x6 with external feedback
3 - reduced 3x2x6
4 - transposed 3x2x6
*/
#define MODMATRIX 2

#if MODMATRIX == 1
  static float s_opval[POLYPHONY][OPERATOR_COUNT + FEEDBACK_COUNT * 2];
#else
  static float s_opval[POLYPHONY][OPERATOR_COUNT];
#endif
#if MODMATRIX == 1 || MODMATRIX == 2
  static float32x4x2_t vModMatrix[POLYPHONY][OPERATOR_COUNT];
#elif MODMATRIX == 3
  static float32x2x3_t vModMatrix[OPERATOR_COUNT][POLYPHONY];
#elif MODMATRIX == 4
  static float32x2x3_t vModMatrix[POLYPHONY][OPERATOR_COUNT];
#endif
  static float32x2_t vFeedbackLevel[POLYPHONY];
  static uint8x8_t vFeedbackSource[POLYPHONY];
#if MODMATRIX != 1  
  static float32x2_t vFeedbackOutput[POLYPHONY];
  static float32x2_t vFeedbackScale[POLYPHONY];
#endif
  static float32x2_t vFeedbackShadow[POLYPHONY];
  static float32x2x3_t vCompensation[POLYPHONY];

  static int8_t s_algorithm_offset = 0;
  static uint8_t s_algorithm_select = 0;
  static uint8_t s_split_point[SPLIT_ZONES - 1] = {0};
  static int8_t s_zone_transpose[SPLIT_ZONES] = {0};
  static int8_t s_zone_voice_shift[SPLIT_ZONES] = {0};
#ifndef KIT_MODE
  static uint8_t s_kit_voice = 0;
  static int8_t s_voice[SPLIT_ZONES] = {0};
#endif
  static float s_level_offset[OPERATOR_COUNT + 3] = {0};
  static float s_kls_offset[OPERATOR_COUNT + 3] = {0};
  static float s_kvs_offset[OPERATOR_COUNT + 3] = {0};
  static float s_egrate_offset[OPERATOR_COUNT + 3] = {0};
  static float s_krs_offset[OPERATOR_COUNT + 3] = {0};
  static float s_detune_offset[OPERATOR_COUNT + 3] = {0};
  static int8_t s_level_scale[OPERATOR_COUNT + 3] = {0};
//  static int8_t s_kls_scale[OPERATOR_COUNT + 3] = {0};
  static int8_t s_kvs_scale[OPERATOR_COUNT + 3] = {0};
//  static int8_t s_egrate_scale[OPERATOR_COUNT + 3] = {0};
//  static int8_t s_krs_scale[OPERATOR_COUNT + 3] = {0};
  static int8_t s_detune_scale[OPERATOR_COUNT + 3] = {0};
#ifdef WAVE_PINCH
  static int8_t s_waveform_pinch_offset[OPERATOR_COUNT + 3] = {0};
  static int8_t s_waveform_pinch[OPERATOR_COUNT + 3] = {0};
#endif
  static float s_feedback_offset[FEEDBACK_COUNT] = {0.f};
#if FEEDBACK_COUNT == 2
//  static float s_feedback_scale[FEEDBACK_COUNT] = {1.f, 1.f};
#else
//  static float s_feedback_scale[FEEDBACK_COUNT] = {1.f};
#endif
  static uint32_t s_feedback_route[FEEDBACK_COUNT] = {0};
  static float s_feedback_level[POLYPHONY][FEEDBACK_COUNT] = {0.f};

  static int8_t s_left_depth[POLYPHONY][OPERATOR_COUNT];
  static int8_t s_right_depth[POLYPHONY][OPERATOR_COUNT];
  static uint8_t s_pitchfreq[POLYPHONY][OPERATOR_COUNT];
  static uint8_t s_egstage[POLYPHONY][OPERATOR_COUNT];
  static uint8_t s_kvs[POLYPHONY][OPERATOR_COUNT];
  static uint8_t s_break_point[POLYPHONY][OPERATOR_COUNT];
  static uint8_t s_left_curve[POLYPHONY][OPERATOR_COUNT];
  static uint8_t s_right_curve[POLYPHONY][OPERATOR_COUNT];
  static int8_t s_detune[POLYPHONY][OPERATOR_COUNT];
  static uint32_t s_sample_cnt[POLYPHONY];
  static uint32_t s_eg_sample_cnt[POLYPHONY][OPERATOR_COUNT][EG_STAGE_COUNT * 2];

  static float s_op_level[POLYPHONY][OPERATOR_COUNT];
  static float s_op_rate_scale[POLYPHONY][OPERATOR_COUNT];

  static uint8_t s_op_waveform[POLYPHONY][OPERATOR_COUNT];
  static wave_t s_waveform[POLYPHONY][OPERATOR_COUNT];
  static int32_t s_waveform_cm[2];
  static const float *s_wave_sample_ptr[POLYPHONY][OPERATOR_COUNT];
  static float s_wave_size[POLYPHONY][OPERATOR_COUNT];
  static uint32_t s_wave_size_mask[POLYPHONY][OPERATOR_COUNT];
  static uint32_t s_wave_size_exp[POLYPHONY][OPERATOR_COUNT];
  static uint32_t s_wave_size_frac[POLYPHONY][OPERATOR_COUNT];
  
  static float s_egrate[POLYPHONY][OPERATOR_COUNT][EG_STAGE_COUNT];
  static q31_t s_egsrate[POLYPHONY][OPERATOR_COUNT][EG_STAGE_COUNT * 2];
  static float s_egsrate_recip[POLYPHONY][OPERATOR_COUNT][2];
  static q31_t s_eglevel[POLYPHONY][OPERATOR_COUNT][EG_STAGE_COUNT];
  static q31_t s_egval[POLYPHONY][OPERATOR_COUNT];
  static q31_t s_oplevel[POLYPHONY][OPERATOR_COUNT];
  static q31_t s_outlevel[POLYPHONY][OPERATOR_COUNT];
#ifdef OP6
  static float s_klslevel[OPERATOR_COUNT] = {LEVEL_SCALE_FACTOR_DB, LEVEL_SCALE_FACTOR_DB, LEVEL_SCALE_FACTOR_DB, LEVEL_SCALE_FACTOR_DB, LEVEL_SCALE_FACTOR_DB, LEVEL_SCALE_FACTOR_DB};
  static float s_krslevel[OPERATOR_COUNT] = {RATE_SCALING_FACTOR, RATE_SCALING_FACTOR, RATE_SCALING_FACTOR, RATE_SCALING_FACTOR, RATE_SCALING_FACTOR, RATE_SCALING_FACTOR};
  static float s_egratelevel[OPERATOR_COUNT] = {1.f, 1.f, 1.f, 1.f, 1.f, 1.f};
#ifdef WAVE_PINCH
  static q31_t s_wavewidth[OPERATOR_COUNT * 2] = {0};
#endif
#else
  static float s_klslevel[OPERATOR_COUNT] = {LEVEL_SCALE_FACTOR_DB, LEVEL_SCALE_FACTOR_DB, LEVEL_SCALE_FACTOR_DB, LEVEL_SCALE_FACTOR_DB};
  static float s_krslevel[OPERATOR_COUNT] = {RATE_SCALING_FACTOR, RATE_SCALING_FACTOR, RATE_SCALING_FACTOR, RATE_SCALING_FACTOR};
  static float s_egratelevel[OPERATOR_COUNT] = {1.f, 1.f, 1.f, 1.f};
#ifdef WAVE_PINCH
  static q31_t s_wavewidth[OPERATOR_COUNT * 2] = {0};
#endif
#endif
  static float s_klsoffset[OPERATOR_COUNT] = {0};
  static float s_egrateoffset[OPERATOR_COUNT] = {0};
  static float s_krsoffset[OPERATOR_COUNT] = {0};
  static q31_t s_level_scaling[POLYPHONY][OPERATOR_COUNT];
  static float s_kvslevel[POLYPHONY][OPERATOR_COUNT];
  static float s_velocitylevel[POLYPHONY][OPERATOR_COUNT];

  static uint32_t s_feedback_src[POLYPHONY][FEEDBACK_COUNT];
  static uint32_t s_feedback_dst[POLYPHONY][FEEDBACK_COUNT];
  static uint32_t s_feedback_src_alg[POLYPHONY][FEEDBACK_COUNT];
  static uint32_t s_feedback_dst_alg[POLYPHONY][FEEDBACK_COUNT];

  static int32_t s_pegrate[POLYPHONY][PEG_STAGE_COUNT + 1];
  static int32_t s_peglevel[POLYPHONY][PEG_STAGE_COUNT];
  static uint32_t s_peg_sample_cnt[POLYPHONY][PEG_STAGE_COUNT];
  static int32_t s_pegval[POLYPHONY];
  static float s_pegrate_releaserecip[POLYPHONY];
  static uint8_t s_pegstage[POLYPHONY];
  static uint8_t s_peg_stage_start[POLYPHONY];

  static float s_oppitch[POLYPHONY][OPERATOR_COUNT];

#define UQ32_PHASE
#ifdef UQ32_PHASE
  static uint32x2x3_t vPhase[POLYPHONY];
#else
  static float32x2x3_t vPhase[POLYPHONY];
#endif

  fast_inline const dx_voice_t * getDxVoice(uint32_t index) {
    uint32_t voicebank_idx = 0;
    while (voicebank_idx < sizeof(DxVoiceBanks)/sizeof(DxVoiceBanks[0]) && index >= DxVoiceBanks[voicebank_idx].count && DxVoiceBanks[voicebank_idx].count > 0)
      index -= DxVoiceBanks[voicebank_idx++].count;
    if (voicebank_idx < sizeof(DxVoiceBanks)/sizeof(DxVoiceBanks[0]) && DxVoiceBanks[voicebank_idx].count > 0)
      return &DxVoiceBanks[voicebank_idx].dxvoice_ptr[index];
    return nullptr;
  }

  fast_inline float paramScale(int8_t *param, uint32_t synvoice_idx, uint32_t opidx) {
    return .000001f * (param[opidx] + 100) * (param[((s_algorithm[synvoice_idx][opidx] & ALG_OUT_MASK) >> 7) + OPERATOR_COUNT] + 100) * (param[OPERATOR_COUNT + 2] + 100);
  }

  fast_inline int16_t paramOffset(int8_t *param, uint32_t synvoice_idx, uint32_t opidx) {
    return param[opidx] + param[((s_algorithm[synvoice_idx][opidx] & ALG_OUT_MASK) >> 7) + OPERATOR_COUNT] + param[OPERATOR_COUNT + 2];
  }

  fast_inline float paramOffset(float *param, uint32_t synvoice_idx, uint32_t opidx) {
    return param[opidx] + param[((s_algorithm[synvoice_idx][opidx] & ALG_OUT_MASK) >> 7) + OPERATOR_COUNT] + param[OPERATOR_COUNT + 2];
  }

  fast_inline void setOpLevel(uint32_t synvoice_idx, uint32_t opidx) {
// make it non-negative and apply -96dB to further fit EG level
    s_oplevel[synvoice_idx][opidx] = q31sub((usat_lsl(31, q31add(s_level_scaling[synvoice_idx][opidx], f32_to_q31(s_velocitylevel[synvoice_idx][opidx])), 0)), 0x7F000000);
  }

  fast_inline void setOutLevel(uint32_t synvoice_idx) {
    for (uint32_t i = 0; i < OPERATOR_COUNT; i++)
// saturate Out Level to 0dB offset of Q31
      s_outlevel[synvoice_idx][i] = q31add(f32_to_q31(scale_level(clipminmax(0.f, s_op_level[synvoice_idx][i] + paramOffset(s_level_offset, synvoice_idx, i), 99.f)) * paramScale(s_level_scale, synvoice_idx, i) * LEVEL_SCALE_FACTORF), 0x00FFFFFF);
  }

  fast_inline void setKvsLevel(uint32_t synvoice_idx) {
    for (uint32_t i = 0; i < OPERATOR_COUNT; i++)
      s_kvslevel[synvoice_idx][i] = clipminmax(0.f, s_kvs[synvoice_idx][i] + paramOffset(s_kvs_offset, synvoice_idx, i), 7.f) * paramScale(s_kvs_scale, synvoice_idx, i);
  }

  fast_inline void setVelocityLevel(uint32_t synvoice_idx) {
    for (uint32_t i = 0; i < OPERATOR_COUNT; i++) { 
// Velocity * KVS
      s_velocitylevel[synvoice_idx][i] = SynVoices[synvoice_idx].velocity * s_kvslevel[synvoice_idx][i];
      setOpLevel(synvoice_idx, i);
    }
  }

fast_inline void setWaveform(uint32_t synvoice_idx) {
  int32_t waveform_idx;
  for (uint32_t i = 0; i < OPERATOR_COUNT; i++) {
    waveform_idx = s_waveform_cm[(s_algorithm[synvoice_idx][i] & ALG_OUT_MASK) ? 0 : 1];
    if (waveform_idx == 0)
      waveform_idx = s_op_waveform[synvoice_idx][i];
    else
      waveform_idx--;
    s_waveform[synvoice_idx][i] = Wavetables.getWave(waveform_idx);
    s_wave_sample_ptr[synvoice_idx][i] = s_waveform[synvoice_idx][i].sample_ptr;
    s_wave_size[synvoice_idx][i] = s_waveform[synvoice_idx][i].size;
    s_wave_size_mask[synvoice_idx][i] = s_waveform[synvoice_idx][i].size_mask;
    s_wave_size_exp[synvoice_idx][i] = s_waveform[synvoice_idx][i].size_exp;
    s_wave_size_frac[synvoice_idx][i] = s_waveform[synvoice_idx][i].size_exp - 32;
  }
}

#ifdef WAVE_PINCH
void setWaveformPinch() {
  uint32_t value;
  for (uint32_t i = 0; i < OPERATOR_COUNT; i++) {
    value = clipminmaxi32(1, 100 - (s_waveform_pinch[i] + paramOffset(s_waveform_pinch_offset, i)), 100);
    s_wavewidth[i * 2] = 0x0147AE14 * value; // 1/100 * witdh
    s_wavewidth[i * 2 + 1] = 0x64000000 / value; // (100 >> 7) / width
  }
}
#endif

  fast_inline void setFeedback(uint32_t synvoice_idx, uint32_t idx) {
//    s_feedback[synvoice_idx][idx] = fastpow2(clipminmax(0.f, s_feedback_level[synvoice_idx][idx] + s_feedback_offset[idx], 8.f) + FEEDBACK_RECIP_LOG2);
    vFeedbackLevel[synvoice_idx][idx] = fastpow2(clipminmax(0.f, s_feedback_level[synvoice_idx][idx] + s_feedback_offset[idx], 8.f) + FEEDBACK_RECIP_LOG2);
  }

  fast_inline void setFeedbackRoute(uint32_t synvoice_idx, uint32_t idx) {
    if (s_feedback_route[idx] == 0) {
      s_feedback_src[synvoice_idx][idx] = s_feedback_src_alg[synvoice_idx][idx];
      s_feedback_dst[synvoice_idx][idx] = s_feedback_dst_alg[synvoice_idx][idx];
    } else {
      s_feedback_src[synvoice_idx][idx] = OPERATOR_COUNT - 1 - (s_feedback_route[idx] - 1) / 6;
      s_feedback_dst[synvoice_idx][idx] = OPERATOR_COUNT - 1 - (s_feedback_route[idx] - 1) % 6;
    }
#if MODMATRIX != 1
  if (s_feedback_src[synvoice_idx][idx] == OPERATOR_COUNT || s_feedback_dst[synvoice_idx][idx] == OPERATOR_COUNT) {
    s_feedback_src[synvoice_idx][idx] = 0;
    s_feedback_dst[synvoice_idx][idx] = 0;
    vFeedbackScale[synvoice_idx][idx] = 0.f;
  } else {
    vFeedbackScale[synvoice_idx][idx] = MI_SCALE_FACTOR;    
  }
#endif
    for (uint32_t i = 0; i < OPERATOR_COUNT; i++) {
      if (i == s_feedback_dst[synvoice_idx][idx])
        s_algorithm[synvoice_idx][i] |= ALG_FBK_MASK << idx;
      else
        s_algorithm[synvoice_idx][i] &= ~(ALG_FBK_MASK << idx);
        
//__asm__ volatile ("nop\n nop\n nop\n");
/*
      vAlgorithm[i].val[0][0] = (s_algorithm[i] & 0x01) ? 0xFFFFFFFF : 0;
      vAlgorithm[i].val[0][1] = (s_algorithm[i] & 0x02) ? 0xFFFFFFFF : 0;
      vAlgorithm[i].val[0][2] = (s_algorithm[i] & 0x04) ? 0xFFFFFFFF : 0;
      vAlgorithm[i].val[0][3] = (s_algorithm[i] & 0x08) ? 0xFFFFFFFF : 0;
      vAlgorithm[i].val[1][0] = (s_algorithm[i] & 0x10) ? 0xFFFFFFFF : 0;
      vAlgorithm[i].val[1][1] = 0;      
      vAlgorithm[i].val[1][2] = (s_algorithm[i] & 0x20) ? 0xFFFFFFFF : 0;
      vAlgorithm[i].val[1][3] = (s_algorithm[i] & 0x40) ? 0xFFFFFFFF : 0;
*/
/*
        int16x8_t tmp = vmovl_s8(vneg_s8(  
          vcnt_s8(vand_s8(
            vdup_n_s8(s_algorithm[i]),
            vcreate_s8(0x4020001008040201)
          ))
        ));
        vAlgorithm[i] = {{
          vreinterpretq_u32_s32(vmovl_s16(vget_low_s16(tmp))),
          vreinterpretq_u32_s32(vmovl_s16(vget_high_s16(tmp)))
        }};
*/
        uint16x8_t tmp = vmovl_u8(  
          vcnt_u8(vand_u8(
            vdup_n_u8(s_algorithm[synvoice_idx][i]),
#if MODMATRIX == 1
            vcreate_u8(0x4020001008040201)
#else
            vcreate_u8(0x0000001008040201)
#endif
          ))
        );
#if MODMATRIX == 1 || MODMATRIX == 2
        vModMatrix[synvoice_idx][i] = {{
          vcvtq_f32_u32(vmovl_u16(vget_low_u16(tmp))) * MI_SCALE_FACTOR,
          vcvtq_f32_u32(vmovl_u16(vget_high_u16(tmp))) * MI_SCALE_FACTOR
        }};
#elif MODMATRIX == 3
        float32x4_t tmp2 = vcvtq_f32_u32(vmovl_u16(vget_low_u16(tmp))) * MI_SCALE_FACTOR;
        vModMatrix[i][synvoice_idx] = {{
          vget_low_f32(tmp2),
          vget_high_f32(tmp2),
          vget_low_f32(vcvtq_f32_u32(vmovl_u16(vget_high_u16(tmp))) * MI_SCALE_FACTOR)
        }};
#elif MODMATRIX == 4
        float32x4_t tmp2 = vcvtq_f32_u32(vmovl_u16(vget_low_u16(tmp))) * MI_SCALE_FACTOR;
        ((float*)&vModMatrix[synvoice_idx][0])[i] = tmp2[0];
        ((float*)&vModMatrix[synvoice_idx][1])[i] = tmp2[1];
        ((float*)&vModMatrix[synvoice_idx][2])[i] = tmp2[2];
        ((float*)&vModMatrix[synvoice_idx][3])[i] = tmp2[3];
        tmp2 = vcvtq_f32_u32(vmovl_u16(vget_high_u16(tmp))) * MI_SCALE_FACTOR;
        ((float*)&vModMatrix[synvoice_idx][4])[i] = tmp2[0];
        ((float*)&vModMatrix[synvoice_idx][5])[i] = tmp2[1];
#endif
        vFeedbackSource[synvoice_idx] = vreinterpret_u8_u32(0x03020100 + vld1_u32(s_feedback_src[synvoice_idx]) * 4);
    }
  }

  fast_inline void setAlgorithm(uint32_t synvoice_idx) {
    int32_t comp = 0;
    s_feedback_dst_alg[synvoice_idx][0] = OPERATOR_COUNT;
    s_feedback_dst_alg[synvoice_idx][1] = OPERATOR_COUNT;
    for (uint32_t i = 0; i < OPERATOR_COUNT; i++) {
#ifndef MOD16
      s_algorithm[synvoice_idx][i] = dx7_algorithm[clipminmax(0, (int32_t)(s_algorithm_select == 0 ? SynVoices[synvoice_idx].algorithm_idx : s_algorithm_select - 1) + s_algorithm_offset, ALGORITHM_COUNT - 1)][i];
#endif
#ifdef CUSTOM_ALGORITHM_COUNT
        if (algidx < ALGORITHM_COUNT) {
#endif
      for (uint32_t fbidx = 0; fbidx < FEEDBACK_COUNT; fbidx++) {
        if (s_algorithm[synvoice_idx][i] & (ALG_FBK_MASK << fbidx)) {
          s_feedback_src_alg[synvoice_idx][fbidx] = s_algorithm[synvoice_idx][i] & (ALG_FBK_MASK - 1);
          s_feedback_dst_alg[synvoice_idx][fbidx] = i;
#ifndef MOD16
          s_algorithm[synvoice_idx][i] &= ~(ALG_FBK_MASK - 1);
#endif
        }
      }
      if (s_algorithm[synvoice_idx][i] & ALG_OUT_MASK)
        comp++;
#ifdef CUSTOM_ALGORITHM_COUNT
        } else {
          if (custom_algorithm[algidx - ALGORITHM_COUNT][i][OPERATOR_COUNT] != 0)
            comp++;
        }
#endif

    }
    setFeedbackRoute(synvoice_idx, 0);
    setFeedbackRoute(synvoice_idx, 1);
    static const float compensation[6] = {
      1.f,
      .5f,
      .333333333f,
      .25f,
      .2f,
      .166666667f,
    };
    float s_comp[OPERATOR_COUNT];
    for (uint32_t i = 0; i < OPERATOR_COUNT; i++) {
      s_comp[i] = 0.f;
#ifdef CUSTOM_ALGORITHM_COUNT
      if (algidx < ALGORITHM_COUNT) {
#endif
      if (s_algorithm[synvoice_idx][i] & ALG_OUT_MASK)
        s_comp[i] = compensation[comp - 1];
#ifdef WAVE_PINCH
      s_waveform_pinch[i] = 0;
#endif
#ifdef CUSTOM_ALGORITHM_COUNT
      } else {
        if (custom_algorithm[algidx - ALGORITHM_COUNT][i][OPERATOR_COUNT] != 0)
          s_comp[i] = compensation[comp - 1];
#ifdef WAVE_PINCH
        s_waveform_pinch[i] = custom_algorithm[algidx - ALGORITHM_COUNT][i][OPERATOR_COUNT + 1];
#endif
      }
#endif
#ifdef WAVE_PINCH
      setWaveformPinch();
#endif
    }
    for (uint32_t synvoice_idx = 0; synvoice_idx < POLYPHONY; synvoice_idx++)    
      setWaveform(synvoice_idx);

    vCompensation[synvoice_idx].val[0][0] = s_comp[0];
    vCompensation[synvoice_idx].val[0][1] = s_comp[1];
    vCompensation[synvoice_idx].val[1][0] = s_comp[2];
    vCompensation[synvoice_idx].val[1][1] = s_comp[3];
    vCompensation[synvoice_idx].val[2][0] = s_comp[4];
    vCompensation[synvoice_idx].val[2][1] = s_comp[5];      
  }

  fast_inline void initVoice(uint32_t synvoice_idx) {
    const dx_voice_t * dxvoice_ptr = getDxVoice(SynVoices[synvoice_idx].dxvoice_idx);
    if (dxvoice_ptr == nullptr)
      dxvoice_ptr = dx7_init_voice_ptr;
    if (dxvoice_ptr->dx7.vnam[0] != 0) {
#ifdef OP6
        const dx7_voice_t * voice = &dxvoice_ptr->dx7;
    SynVoices[synvoice_idx].opi = voice->opi;
    SynVoices[synvoice_idx].algorithm_idx = voice->als;
    SynVoices[synvoice_idx].transpose = voice->trnp - TRANSPOSE_CENTER;

    s_feedback_level[synvoice_idx][0] = voice->fbl;

    s_peg_stage_start[synvoice_idx] = PEG_STAGE_COUNT - DX7_PEG_STAGE_COUNT;
    for (uint32_t i = s_peg_stage_start[synvoice_idx]; i < PEG_STAGE_COUNT; i++) {
      s_peglevel[synvoice_idx][i] = scale_pitch_level(voice->pl[i - s_peg_stage_start[synvoice_idx]]) * PEG_SCALE;
      s_pegrate[synvoice_idx][i] = scale_pitch_rate(voice->pr[i - s_peg_stage_start[synvoice_idx]]) * PEG_RATE_SCALE;
    }

    for (uint32_t i = 0; i < OPERATOR_COUNT; i++) {
      s_pitchfreq[synvoice_idx][i] = !voice->op[i].pm;
      s_detune[synvoice_idx][i] = (voice->op[i].pd - DX7_DETUNE_CENTER) * 3;
#ifdef TWEAK_WF
      s_op_waveform[synvoice_idx][i] = voice->op[i].osw & ((1 << WFBITS) - 1);
#else
      s_op_waveform[i] = 0;
#endif
//      s_phase[i] = 0;

//todo: check dx7 D1/D2/R rates
      for (uint32_t j = 0; j < EG_STAGE_COUNT; j++) {
        s_egrate[synvoice_idx][i][j] = voice->op[i].r[j];
        s_eglevel[synvoice_idx][i][j] = scale_level(voice->op[i].l[j]) * LEVEL_SCALE_FACTOR;
      }

      if (s_pitchfreq[synvoice_idx][i])
        s_oppitch[synvoice_idx][i] = (voice->op[i].pc == 0 ? .5f : voice->op[i].pc) * (1.f + voice->op[i].pf * .01f);
      else
        s_oppitch[synvoice_idx][i] = fastexp(M_LN10 * ((voice->op[i].pc & 3) + voice->op[i].pf * .01f)) * SAMPLERATE_RECIP;

      s_kvs[synvoice_idx][i] = voice->op[i].ts;
      s_op_rate_scale[synvoice_idx][i] = voice->op[i].rs * DX7_RATE_SCALING_FACTOR;
      s_op_level[synvoice_idx][i] = voice->op[i].tl;
      s_break_point[synvoice_idx][i] = voice->op[i].bp + NOTE_A_1;
//fold negative/position curves into curve depth sign
      s_left_depth[synvoice_idx][i] = voice->op[i].ld;
      s_right_depth[synvoice_idx][i] = voice->op[i].rd;
      if (voice->op[i].lc < 2) {
        s_left_curve[synvoice_idx][i] = voice->op[i].lc;
      } else {
        s_left_curve[synvoice_idx][i] = 5 - voice->op[i].lc;
      }
      if (voice->op[i].rc < 2) {
        s_right_curve[synvoice_idx][i] = voice->op[i].rc;
      } else {
        s_right_curve[synvoice_idx][i] = 5 - voice->op[i].rc;
      }
    }
#ifdef UQ32_PHASE
    vPhase[synvoice_idx] = {vdup_n_u32(0), vdup_n_u32(0), vdup_n_u32(0)};
#else
    vPhase[synvoice_idx] = {vdup_n_f32(0.f), vdup_n_f32(0.f), vdup_n_f32(0.f)};
#endif
    SynVoices[synvoice_idx].level_scale_factor = DX7_LEVEL_SCALE_FACTOR;
#endif
      } else {
//        dx11_voice_t * voice = &dxvoice_ptr.dx11;

      }

      setAlgorithm(synvoice_idx);
      setOutLevel(synvoice_idx);
      setKvsLevel(synvoice_idx);
      setVelocityLevel(synvoice_idx);
      for (uint32_t i = 0; i < FEEDBACK_COUNT; i++)
        setFeedback(synvoice_idx, i);
#if MODMATRIX == 1        
      for (uint32_t i = 0; i < OPERATOR_COUNT + FEEDBACK_COUNT * 2; i++)
#else
      for (uint32_t i = 0; i < OPERATOR_COUNT; i++)
#endif
        s_opval[synvoice_idx][i] = 0.f;
      for (uint32_t i = 0; i < OPERATOR_COUNT; i++) {
        s_eg_sample_cnt[synvoice_idx][i][EG_STAGE_COUNT - 1] = 0xFFFFFFFF;
        s_egsrate[synvoice_idx][i][EG_STAGE_COUNT - 1] = 0;
        s_egstage[synvoice_idx][i] = EG_STAGE_COUNT - 1;
        s_egval[synvoice_idx][i] = 0;
      }

      uint32_t samples = 0;
      int32_t dl;
      for (uint32_t i = s_peg_stage_start[synvoice_idx]; i < PEG_STAGE_COUNT - 1; i++) {
        dl = (s_peglevel[synvoice_idx][i] - s_peglevel[synvoice_idx][i != s_peg_stage_start[synvoice_idx] ? i - 1 : PEG_STAGE_COUNT - 1]);
        if (dl < 0)
          s_pegrate[synvoice_idx][i] = -s_pegrate[synvoice_idx][i];
        samples += dl / s_pegrate[synvoice_idx][i];
        s_peg_sample_cnt[synvoice_idx][i] = samples;
      }
      s_pegrate[synvoice_idx][PEG_STAGE_COUNT] = s_pegrate[synvoice_idx][PEG_STAGE_COUNT - 1];
      s_pegrate_releaserecip[synvoice_idx] = 1.f / s_pegrate[synvoice_idx][PEG_STAGE_COUNT];
      s_pegrate[synvoice_idx][PEG_STAGE_COUNT - 1] = 0;
      s_peg_sample_cnt[synvoice_idx][PEG_STAGE_COUNT - 1] = 0xFFFFFFFF;
      s_pegstage[synvoice_idx] = PEG_STAGE_COUNT - 1;
      s_pegval[synvoice_idx] = 0;

  return;
 }

  fast_inline q31_t calc_rate(uint32_t synvoice_idx, uint32_t i, uint32_t j, float rate_factor, int32_t note) {
    if (j == 0)
      rate_factor *= DX7_RATE1_FACTOR;
    float rscale = (note - NOTE_A_1) * clipminmax(0.f, s_op_rate_scale[synvoice_idx][i] + s_krsoffset[i], 7.f) * s_krslevel[i];
    float rate = clipminmax(0.f, s_egrate[synvoice_idx][i][j] + s_egrateoffset[i], 99.f) * s_egratelevel[i];
    return f32_to_q31(rate_factor * fastpow2(DX7_RATE_EXP_FACTOR * (rate + rscale)));
  }

#ifdef NOINTERPOLATE
  fast_inline float32x4_t wave(const float **sample_ptr, int32x4_t size_exp, int32x4_t size_frac, uint32x4_t size_mask, uint32x4_t x) {
    const uint32x4_t x0 = vshlq_u32(x, size_frac);
    const float32x4_t res = {sample_ptr[0][x0[0]], sample_ptr[1][x0[1]], sample_ptr[2][x0[2]], sample_ptr[3][x0[3]]};
    return res;
  }  

  fast_inline float32x4_t wave(const float **sample_ptr, float32x4_t size, uint32x4_t size_mask, float32x4_t x) {
    const uint32x4_t offs = vcvtq_s32_f32(x * size) & size_mask;
    const float32x4_t res = {sample_ptr[0][offs[0]], sample_ptr[1][offs[1]], sample_ptr[2][offs[2]], sample_ptr[3][offs[3]]};
    return res;
  }  

  fast_inline float32x2_t wave(const float **sample_ptr, float32x2_t size, uint32x2_t size_mask, float32x2_t x) {
//__asm__ volatile ("nop\n nop\n nop\n");
//    const uint32x2_t offs = ((int32x2_t)(x * size)) & size_mask;
    const uint32x2_t offs = vcvt_s32_f32(x * size) & size_mask;
    const float32x2_t res = {sample_ptr[0][offs[0]], sample_ptr[1][offs[1]]};
//__asm__ volatile ("nop\n nop\n nop\n");
    return res;
  }  

  fast_inline float wave(wave_t *w, float x) {
    const float res = w->sample_ptr[(int32_t)(x * w->size) & w->size_mask];
    return res;
  }  
#else

  fast_inline float32x4_t wave(const float **sample_ptr, int32x4_t size_exp, int32x4_t size_frac, uint32x4_t size_mask, uint32x4_t x) {
    const uint32x4_t x0 = vshlq_u32(x, size_frac);
    const uint32x4_t x1 = (x0 + 1) & size_mask;
    const float32x4_t w0 = {sample_ptr[0][x0[0]], sample_ptr[1][x0[1]], sample_ptr[2][x0[2]], sample_ptr[3][x0[3]]};
    const float32x4_t w1 = {sample_ptr[0][x1[0]], sample_ptr[1][x1[1]], sample_ptr[2][x1[2]], sample_ptr[3][x1[3]]};
    const float32x4_t res = w0 + vcvtq_n_f32_u32(vshlq_u32(x, size_exp), 32) * (w1 - w0);
    return res;
  }  

  fast_inline float32x4_t wave(const float **sample_ptr, float32x4_t size, uint32x4_t size_mask, float32x4_t x) {
    x -= vcvtq_f32_s32(vcvtq_s32_f32(x));
    x += 1.f;
    x -= vcvtq_f32_s32(vcvtq_s32_f32(x));
    const float32x4_t x0f = x * size;
    const uint32x4_t x0i = vcvtq_u32_f32(x0f);
    const uint32x4_t x0 = x0i & size_mask;
    const uint32x4_t x1 = (x0i + vdupq_n_u32(1)) & size_mask;
    const float32x4_t w0 = {sample_ptr[0][x0[0]], sample_ptr[1][x0[1]], sample_ptr[2][x0[2]], sample_ptr[3][x0[3]]};
    const float32x4_t w1 = {sample_ptr[0][x1[0]], sample_ptr[1][x1[1]], sample_ptr[2][x1[2]], sample_ptr[3][x1[3]]};
    const float32x4_t res = w0 + (x0f - vcvtq_f32_u32(x0i)) * (w1 - w0);
    return res;
  }  
  fast_inline float wave(wave_t *w, float x) {
//__asm__ volatile ("nop\n nop\n nop\n");
    x -= (int32_t)x;
    if (x < 0.f)
      x += 1.f;
    float x0f = x * w->size;
    const int32_t x0i = x0f;
    const int32_t x0 = x0i & w->size_mask;
    const int32_t x1 = (x0 + 1) & w->size_mask;

    const float res = w->sample_ptr[x0] + (x0f - x0i) * (w->sample_ptr[x1] - w->sample_ptr[x0]);
//__asm__ volatile ("nop\n nop\n nop\n");
    return res;
  }  
#endif
  fast_inline float32x2_t wave2x(wave_t *w, float32x2_t x) {
    int32x2_t xx = vcvt_s32_f32(x * w->size) & w->size_mask;
    return (float32x2_t){w->sample_ptr[xx[0]], w->sample_ptr[xx[1]]};
  }  

  fast_inline float wave_q31(wave_t *w, q31_t x) {
//    uint32_t x0p = ubfx(x, 31 - w->size_exp, w->size_exp);
    uint32_t x0p = ubfx(x, 31 - 8, 8);
    uint32_t x0 = x0p;
    uint32_t x1 = (x0 + 1) & 0xff;
    const q31_t fr = (x << 8) & 0x7FFFFFFF;
    return w->sample_ptr[x0] + q31_to_f32(fr) * (w->sample_ptr[x1] - w->sample_ptr[x0]);
  }  

  fast_inline void getChordNotes(int32_t * notes, int32_t value) {
    value += (value - 1) / 7 + (((value - 8) / 49) << 3);
    for (uint32_t i = 0; i < MAX_CHORD - 1; i++) {
      notes[i] = ((value >> (MAX_CHORD - 2 - i) * 3) & 0x07);
      if (i > 0 && notes[i] > 0)
        notes[i] += notes[i - 1];
    }
  }

  fast_inline void note_on(uint32_t note, float pitch, float detune_sens, float velocity) {
    uint32_t synvoice_idx = POLYPHONY;
    uint32_t sample_cnt = 0;

    for (uint32_t i = 0; i < POLYPHONY; i++) {
      if (SynVoices[i].note == note && SynVoices[i].pitch == pitch && SynVoices[i].detune_sens == detune_sens) {
        synvoice_idx = i;
        break;
      }
    }
/*
    if (synvoice_idx == POLYPHONY)
    for (uint32_t i = 0; i < POLYPHONY; i++) {
      uint32_t j;
      for (j = 0; j < OPERATOR_COUNT && s_egstage[i][j] == (EG_STAGE_COUNT - 1); j++);
      if (j == OPERATOR_COUNT && SynVoices[i].note != note) {
        synvoice_idx = i;
      }
    }

*/
    if (synvoice_idx == POLYPHONY)
    for (uint32_t i = 0; i < POLYPHONY; i++) {
//      if (s_sample_cnt[i] > sample_cnt  && SynVoices[i].note != note) {// && (SynVoices[synvoice_idx].state & state_noteon) == 0) {
      if (s_sample_cnt[i] > sample_cnt) {// && (SynVoices[synvoice_idx].state & state_noteon) == 0) {
        sample_cnt = s_sample_cnt[i];
        synvoice_idx = i;
      }
    }

    if (synvoice_idx == POLYPHONY) {
//      if (nextvoice >= POLYPHONY)
//        nextvoice = 0;
//      synvoice_idx = nextvoice++;
      return;
    }

    s_sample_cnt[synvoice_idx] = 0;   

    SynVoices[synvoice_idx].state |= state_noteon;
    SynVoices[synvoice_idx].note = note;
    SynVoices[synvoice_idx].pitch = pitch;
    SynVoices[synvoice_idx].detune_sens = detune_sens;
    SynVoices[synvoice_idx].velocity = (fastpow(velocity, .27f) * 60.f - 208.f) * .0625f * LEVEL_SCALE_FACTOR_DB;

    if ((uint32_t)sParams[param_dxvoice_idx] != SynVoices[synvoice_idx].dxvoice_idx) {
      SynVoices[synvoice_idx].dxvoice_idx = sParams[param_dxvoice_idx];
      initVoice(synvoice_idx);
    } else {
      setVelocityLevel(synvoice_idx);
    }

//    sNotePhaseIncrement = (float)UINT_MAX * fastpow2((sNote + sPitchBend + NOTE_FREQ_OFFSET) * OCTAVE_RECIP);  // f = 2^((note - A4)/12), A4 = #69 = 440Hz
//    s_velocity = (fastpow((float)velocity, .27f) * 60.f - 208.f) * .0625f * LEVEL_SCALE_FACTOR_DB;
//    setVelocityLevel();

    float rate_factor;
    int32_t dl, dp, curve = 0, voice;
    float depth = 0.f;
    uint32_t zone;
    for (zone = 0; zone < (SPLIT_ZONES - 1) && note < s_split_point[zone]; zone++);
    SynVoices[synvoice_idx].zone_transpose = s_zone_transpose[zone];
#ifndef KIT_MODE
    voice = s_voice[zone];
//    s_kit_voice = (voice == 0);
    if (voice > 0)
      voice--;
    if (s_kit_voice) {
#endif
      voice = note;
      note = KIT_CENTER;
#ifndef KIT_MODE
    }
  #endif
    note += SynVoices[synvoice_idx].zone_transpose;
    voice += s_zone_voice_shift[zone];
//    initVoice(voice);
    uint32_t samples;
    for (uint32_t i = 0; i < OPERATOR_COUNT; i++) {
      samples = 0;
      for (uint32_t j = 0; j < EG_STAGE_COUNT - 1; j++) {
        dl = s_eglevel[synvoice_idx][i][j] - s_eglevel[synvoice_idx][i][j ? (j - 1) : (EG_STAGE_COUNT - 1)];
        if (dl != 0) {
          if (dl < 0) {
            rate_factor = DX7_DECAY_RATE_FACTOR;
          } else {
            rate_factor = DX7_ATTACK_RATE_FACTOR;
          }
//        s_egsrate[synvoice_idx][i][j + EG_STAGE_COUNT] = calc_rate(i, j, rate_factor, s_attack_rate_exp_factor, note);
          s_egsrate[synvoice_idx][i][j + EG_STAGE_COUNT] = calc_rate(synvoice_idx, i, j, rate_factor, note);
          samples += dl / s_egsrate[synvoice_idx][i][j + EG_STAGE_COUNT];
        } else {
          s_egsrate[synvoice_idx][i][j + EG_STAGE_COUNT] = 0;
        }
        s_eg_sample_cnt[synvoice_idx][i][j + EG_STAGE_COUNT] = samples;
      }
      dp = note - s_break_point[synvoice_idx][i];
//    depth = paramOffset(s_kls_offset, i);
      depth = s_klsoffset[i];
      if (dp < 0) {
         depth += s_left_depth[synvoice_idx][i];
         curve = s_left_curve[synvoice_idx][i];
         dp = - dp;
      } else if (dp > 0) {
        depth += s_right_depth[synvoice_idx][i];
        curve = s_right_curve[synvoice_idx][i];
      }
      if (curve < 2)
        depth = - depth;
// saturate Out level with KLS and adjust 0dB level to fit positive Velocity
//    s_level_scaling[i] = q31sub(q31add(s_outlevel[i], f32_to_q31(clipminmaxf(-99, depth, 99) * paramScale(s_kls_scale, i) * ((curve & 0x01) ? ((POW2F(dp * .083333333f) - 1.f) * .015625f) : (s_level_scale_factor * dp)) * LEVEL_SCALE_FACTOR_DB)), 0x07000000);
      s_level_scaling[synvoice_idx][i] = q31sub(q31add(s_outlevel[synvoice_idx][i], f32_to_q31(clipminmax(-99.f, depth, 99.f) * s_klslevel[i] * ((curve & 0x01) ? ((fastpow2(dp * .083333333f) - 1.f) * .015625f) : (SynVoices[synvoice_idx].level_scale_factor * dp)))), 0x07000000);
//o      s_level_scaling[i] = q31sub(s_outlevel[i], 0x07000000);
//    setOpLevel(i);
    }
    SynVoices[synvoice_idx].zone_transpose += SynVoices[synvoice_idx].transpose;
  }

  fast_inline void noteOn(uint8_t note, uint8_t velocity) {
    uint32_t max_unison = POLYPHONY;
    uint32_t chordnote;
    float detune_sens;
    switch (sParams[param_mode]) {
      case mode_duo:
        max_unison = 2;
      case mode_unison:
        for (int32_t i = max_unison - 1; i >= 0; i--) {
          if (sParams[param_mode] == mode_duo || sParams[param_mode] == mode_unison) {
            detune_sens = i;
          } else {
            detune_sens = 1.f;
          }
          note_on(note, note, i, velocity);
        }
        break;
      default:
        for (uint32_t i = 0; i < MAX_CHORD - 1; i++) {
          if (sParams[param_mode] < (int32_t)mode_chord) {
            chordnote = 0;
          } else if (sParams[param_mode] < (int32_t)(mode_chord + sizeof(sChords)/sizeof(sChords[0]))) {
            chordnote = sChords[sParams[param_mode] - mode_chord].notes[i];
          } else {
            chordnote = sChordNotes[i];
          }
          if (sParams[param_mode] == mode_duo || sParams[param_mode] == mode_unison) {
            detune_sens = i;
          } else {
            detune_sens = 1.f;
          }
          if (sParams[param_mode] < (int32_t)mode_chord || chordnote != 0) {
            note_on(note, note + chordnote, detune_sens, velocity);
          }
        }
      case mode_poly:
        note_on(note, note, 1.f, velocity);
        break;
    }
  }

  fast_inline void noteOff(uint8_t note) {
    uint32_t synvoice_idx;
    for (synvoice_idx = 0; synvoice_idx < POLYPHONY; synvoice_idx++) {
      if (SynVoices[synvoice_idx].note == note) {
        for (uint32_t i = 0; i < OPERATOR_COUNT; i++) {
          s_egsrate[synvoice_idx][i][EG_STAGE_COUNT - 1] = calc_rate(synvoice_idx, i, EG_STAGE_COUNT - 1, DX7_DECAY_RATE_FACTOR, note);
          s_egsrate[synvoice_idx][i][EG_STAGE_COUNT * 2 - 1] = calc_rate(synvoice_idx, i, EG_STAGE_COUNT - 1, DX7_ATTACK_RATE_FACTOR, note);
          s_egsrate_recip[synvoice_idx][i][0] = 1.f / s_egsrate[synvoice_idx][i][EG_STAGE_COUNT - 1];
          s_egsrate_recip[synvoice_idx][i][1] = 1.f / s_egsrate[synvoice_idx][i][EG_STAGE_COUNT * 2 - 1];
        }
        SynVoices[synvoice_idx].state |= state_noteoff;
      }
    }
  }  

  __unit_callback int8_t unit_init(const unit_runtime_desc_t * desc) {
    if (!desc)
      return k_unit_err_undef;
    if (desc->target != unit_header.target)
      return k_unit_err_target;
    if (!UNIT_API_IS_COMPAT(desc->api))
      return k_unit_err_api_version;
    if (desc->samplerate != 48000)
      return k_unit_err_samplerate;
    if (desc->output_channels != 2)
      return k_unit_err_geometry;

    Wavetables.init(desc, 0x65766157); // 'Wave'

    uint32_t bankcount = desc->get_num_sample_banks();
    uint32_t voicebank_idx = 0;
    for (uint32_t bank_idx = 0; bank_idx < bankcount; bank_idx++) {
      uint32_t samplecount = desc->get_num_samples_for_bank(bank_idx);
      for (uint32_t sample_idx = 0; sample_idx < samplecount; sample_idx++) {
        const sample_wrapper_t * sample = desc->get_sample(bank_idx, sample_idx);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
        if (sample == nullptr || sample->sample_ptr == nullptr || sample->channels > 1 || *(uint32_t *)sample->name != unit_header.unit_id)  // 'FM64'
#pragma GCC diagnostic pop
          continue;
        switch (((uint32_t *)sample->name)[1]) {
          case 0x6B6E6142:  // 'Bank'
            DxVoiceBanks[voicebank_idx].dxvoice_ptr = (const dx_voice_t *)sample->sample_ptr;
            DxVoiceBanks[voicebank_idx].count = sample->frames * sample->channels * sizeof(sample->sample_ptr[0]) / sizeof(dx_voice_t);
            voicebank_idx++;
            break;
          default:
            break;
        }
      }
    }
    for (uint32_t i = 0; i < POLYPHONY; i++) {
      SynVoices[i].dxvoice_idx = -1;
    }
    return k_unit_err_none;
  }

__unit_callback void unit_teardown() {
}

__unit_callback void unit_reset() {
}

__unit_callback void unit_resume() {
}

__unit_callback void unit_suspend() {
}

__unit_callback void unit_render(const float * in, float * out, uint32_t frames) {
    (void)in;
    float * __restrict out_p = out;
    const float * out_e = out_p + (frames << 1);  // assuming stereo output
/*
    if (sWave == nullptr) {
      for (; out_p != out_e; out_p += 2) {
        vst1_f32(out_p, vdup_n_f32(0.f));
      }
      return;
    }
*/
#ifdef UQ32_PHASE
    uq32x2x3_t vOpw0[POLYPHONY];
#else
    float32x2x3_t vOpw0[POLYPHONY];
#endif
    for (uint32_t synvoice_idx = 0; synvoice_idx < POLYPHONY; synvoice_idx++) {
      if (SynVoices[synvoice_idx].state) {
        if (SynVoices[synvoice_idx].state == state_noteon) {
          for (uint32_t i = 0; i < OPERATOR_COUNT; i++) {
            for (uint32_t j = 0; j < EG_STAGE_COUNT - 1; j++) {
              s_egsrate[synvoice_idx][i][j] = s_egsrate[synvoice_idx][i][j + EG_STAGE_COUNT];
              s_eg_sample_cnt[synvoice_idx][i][j] = s_eg_sample_cnt[synvoice_idx][i][j + EG_STAGE_COUNT];
            }
            s_egstage[synvoice_idx][i] = 0;
            if (SynVoices[synvoice_idx].opi) {
//              s_phase[i] = 0;
#ifdef UQ32_PHASE
              vPhase[synvoice_idx] = {vdup_n_u32(0), vdup_n_u32(0), vdup_n_u32(0)};
#else
              vPhase[synvoice_idx] = {vdup_n_f32(0.f), vdup_n_f32(0.f), vdup_n_f32(0.f)};
#endif
            }
            // todo: to reset or not to reset - that is the question (stick with the operator phase init)
            s_opval[synvoice_idx][i] = 0.f;
            s_egval[synvoice_idx][i] = s_eglevel[synvoice_idx][i][EG_STAGE_COUNT - 1];
            //      setLevel();
            // make it non-negative and apply -96dB to further fit EG level
            //      s_oplevel[i] = q31sub((usat_lsl(31, q31add(s_level_scaling[i], s_velocitylevel[i]), 0)), 0x7F000000);
            setOpLevel(synvoice_idx, i);
          }
          s_sample_cnt[synvoice_idx] = 0;

          s_pegval[synvoice_idx] = s_peglevel[synvoice_idx][PEG_STAGE_COUNT - 1];
          s_pegstage[synvoice_idx] = s_peg_stage_start[synvoice_idx];

          SynVoices[synvoice_idx].state &= ~state_noteon;
        } else {
          int32_t dl;
          uint32_t samples;
          for (uint32_t i = 0; i < OPERATOR_COUNT; i++) {
            samples = s_sample_cnt[synvoice_idx];
            dl = s_eglevel[synvoice_idx][i][EG_STAGE_COUNT - 1] - s_egval[synvoice_idx][i];
            if (dl != 0) {
              if (dl < 0) {
                samples += dl * s_egsrate_recip[synvoice_idx][i][0];
              } else {
                s_egsrate[synvoice_idx][i][EG_STAGE_COUNT - 1] = s_egsrate[synvoice_idx][i][EG_STAGE_COUNT * 2 - 1];
                samples += dl * s_egsrate_recip[synvoice_idx][i][1];
              }
            } else {
              s_egsrate[synvoice_idx][i][EG_STAGE_COUNT - 1] = 0;
            }
            s_eg_sample_cnt[synvoice_idx][i][EG_STAGE_COUNT - 1] = samples;
            s_egstage[synvoice_idx][i] = EG_STAGE_COUNT - 1;
          }

          dl = s_peglevel[synvoice_idx][PEG_STAGE_COUNT - 1] - s_pegval[synvoice_idx];
          if (dl < 0) {
            s_pegrate[synvoice_idx][PEG_STAGE_COUNT - 1] = -s_pegrate[synvoice_idx][PEG_STAGE_COUNT];
            s_peg_sample_cnt[synvoice_idx][PEG_STAGE_COUNT - 1] = s_sample_cnt[synvoice_idx] - dl * s_pegrate_releaserecip[synvoice_idx];
          } else {
            s_pegrate[synvoice_idx][PEG_STAGE_COUNT - 1] = s_pegrate[synvoice_idx][PEG_STAGE_COUNT];
            s_peg_sample_cnt[synvoice_idx][PEG_STAGE_COUNT - 1] = s_sample_cnt[synvoice_idx] + dl * s_pegrate_releaserecip[synvoice_idx];
          }
          s_pegstage[synvoice_idx] = PEG_STAGE_COUNT - 1;

          SynVoices[synvoice_idx].state &= ~(state_noteoff | state_noteon);
        }
      }

//  q31_t osc_out, modw0;
//  float osc_out;
//  float modw0;
  float pitch = sPitchBend;
  if (s_kit_voice)
    pitch = KIT_CENTER;
  pitch += s_pegval[synvoice_idx];
  float basew0;
  float opw0[OPERATOR_COUNT];
  for (uint32_t i = 0; i < OPERATOR_COUNT; i++) {
    if (s_pitchfreq[synvoice_idx][i]) {
#ifdef FINE_TUNE
//      uint32_t p;
//      p = pitch + (s_detune[synvoice_idx][i] + paramOffset(s_detune_offset, i) * 2.56f) * paramScale(s_detune_scale, i) * FINE_TUNE_FACTOR;
//      uint8_t note = clipmini32(0, (p >> 24) + s_zone_transposed);
//      basew0 = f32_to_pitch(clipmaxf(linintf((p & 0xFFFFFF) * 5.9604645e-8f, osc_notehzf(note), osc_notehzf(note + 1)), k_note_max_hz) * SAMPLERATE_RECIP);
//sNotePhaseIncrement = (float)UINT_MAX * fastpow2((sNote + sPitchBend + NOTE_FREQ_OFFSET) * OCTAVE_RECIP);  // f = 2^((note - A4)/12), A4 = #69 = 440Hz
      basew0 = fastpow2((SynVoices[synvoice_idx].pitch + SynVoices[synvoice_idx].detune_sens * sDetune + SynVoices[synvoice_idx].zone_transpose + pitch + (s_detune[synvoice_idx][i] + paramOffset(s_detune_offset, synvoice_idx, i)) * paramScale(s_detune_scale, synvoice_idx, i) * .00390625f + NOTE_FREQ_OFFSET) * OCTAVE_RECIP);  // f = 2^((note - A4)/12), A4 = #69 = 440Hz
#else
//      basew0 = f32_to_pitch(osc_w0f_for_note(((pitch + s_detune[synvoice_idx][i]) >> 8) + s_transpose, (pitch + s_detune[synvoice_idx][i]) & 0xFF));
#endif
      opw0[i] = s_oppitch[synvoice_idx][i] * basew0;
    } else
      opw0[i] = s_oppitch[synvoice_idx][i];
/*
    int32_t p = s_oppitch[i] + s_detune[synvoice_idx][i];
    if (s_pitchfreq[synvoice_idx][i])
        p += pitch;
    p = usat(p, 16);
    opw0[i] = f32_to_q31(osc_w0f_for_note(p >> 8, p & 0xFF));
*/
  }
#ifdef UQ32_PHASE
  vOpw0[synvoice_idx].val[0][0] = f32_to_uq32(opw0[0]);
  vOpw0[synvoice_idx].val[0][1] = f32_to_uq32(opw0[1]);
  vOpw0[synvoice_idx].val[1][0] = f32_to_uq32(opw0[2]);
  vOpw0[synvoice_idx].val[1][1] = f32_to_uq32(opw0[3]);
  vOpw0[synvoice_idx].val[2][0] = f32_to_uq32(opw0[4]);
  vOpw0[synvoice_idx].val[2][1] = f32_to_uq32(opw0[5]);
#else
  vOpw0[synvoice_idx].val[0][0] = opw0[0];
  vOpw0[synvoice_idx].val[0][1] = opw0[1];
  vOpw0[synvoice_idx].val[1][0] = opw0[2];
  vOpw0[synvoice_idx].val[1][1] = opw0[3];
  vOpw0[synvoice_idx].val[2][0] = opw0[4];
  vOpw0[synvoice_idx].val[2][1] = opw0[5];
#endif
    }

    for (; out_p != out_e; out_p += 2) {
    float32x2_t voice_out = {0.f};
//    for (uint32_t synvoice_idx = 0; synvoice_idx < POLYPHONY; synvoice_idx++) {
    for (uint32_t synvoice_idx = 0; synvoice_idx < POLYPHONY; synvoice_idx += 2) {

//float32x2x3_t w0;
//__asm__ volatile ( "nop\nnop\nnop\nnop\nnop\n");
//    float32x4x3_t w0 = *(float32x4x3_t *)&vPhase[synvoice_idx];
//    float32x4x3_t w0 = *(float32x4x3_t *)&vPhase[synvoice_idx];
//    float32x4x3_t w0 = {
//      *(float32x4_t *)&vPhase[synvoice_idx].val[0],
//      *(float32x4_t *)&vPhase[synvoice_idx].val[2],
//      *(float32x4_t *)&vPhase[synvoice_idx+1].val[1]
//    };

/*
    float32x2x3_t w0 = vPhase[synvoice_idx];
    float32x2x3_t vPhaseNext = {
      w0.val[0] + vOpw0[synvoice_idx].val[0],
      w0.val[1] + vOpw0[synvoice_idx].val[1],
      w0.val[2] + vOpw0[synvoice_idx].val[2]
    };
    vPhaseNext.val[0] -= vcvt_f32_u32(vcvt_u32_f32(vPhaseNext.val[0]));
    vPhaseNext.val[1] -= vcvt_f32_u32(vcvt_u32_f32(vPhaseNext.val[1]));
    vPhaseNext.val[2] -= vcvt_f32_u32(vcvt_u32_f32(vPhaseNext.val[2]));
    vPhase[synvoice_idx] = vPhaseNext;
*/

/*
    *(float32x4x3_t *)&vPhase[synvoice_idx] = {
      vP.val[0] - vcvtq_f32_u32(vcvtq_u32_f32(vP.val[0])),
      vP.val[1] - vcvtq_f32_u32(vcvtq_u32_f32(vP.val[1])),
      vP.val[2] - vcvtq_f32_u32(vcvtq_u32_f32(vP.val[2]))
    };
*/
/*
    float32x4_t tmp;
    float32x2_t tmp1, tmp2, tmp3, tmp4;
    float32x4x3_t w0 = {
      vld1q_f32((float32_t *)&vPhase[synvoice_idx].val[0]),
      vld1q_f32((float32_t *)&vPhase[synvoice_idx].val[2]),
      vld1q_f32((float32_t *)&vPhase[synvoice_idx+1].val[1])
    };
*/

//    w0.val[0] = vPhase[synvoice_idx].val[0];
//    w0.val[1] = vPhase[synvoice_idx].val[1];
//    tmp = vcombine_f32(vPhase[synvoice_idx].val[0], vPhase[synvoice_idx].val[1]) + vcombine_f32(vOpw0[synvoice_idx].val[0], vOpw0[synvoice_idx].val[1]);

/*
    float32x4x3_t w0 = vld3q_f32((float32_t *)vPhase[synvoice_idx].val);
    float32x4x3_t vPhaseNext = vld3q_f32((float32_t *)vOpw0[synvoice_idx].val);
    vPhaseNext.val[0] += w0.val[0];
    vPhaseNext.val[1] += w0.val[1];
    vPhaseNext.val[2] += w0.val[2];
    vPhaseNext.val[0] -= vcvtq_f32_u32(vcvtq_u32_f32(vPhaseNext.val[0])),
    vPhaseNext.val[1] -= vcvtq_f32_u32(vcvtq_u32_f32(vPhaseNext.val[1])),
    vPhaseNext.val[2] -= vcvtq_f32_u32(vcvtq_u32_f32(vPhaseNext.val[2])),
    vst3q_f32((float32_t *)vPhase[synvoice_idx].val, vPhaseNext);
*/
//    float32x4x3_t x = vld1q_f32_x3((float32_t *)&vOpw0[synvoice_idx].val[0]);
/*
    float32x4x3_t vP = {
      w0.val[0] + vld1q_f32((float32_t *)&vOpw0[synvoice_idx].val[0]),
      w0.val[1] + vld1q_f32((float32_t *)&vOpw0[synvoice_idx].val[2]),
      w0.val[2] + vld1q_f32((float32_t *)&vOpw0[synvoice_idx+1].val[1])
    };
    *(float32x4x3_t *)&vPhase[synvoice_idx] = {
      vP.val[0] - vcvtq_f32_u32(vcvtq_u32_f32(vP.val[0])),
      vP.val[1] - vcvtq_f32_u32(vcvtq_u32_f32(vP.val[1])),
      vP.val[2] - vcvtq_f32_u32(vcvtq_u32_f32(vP.val[2]))
    };
*/

//    *(float32x4_t *)&vPhase[synvoice_idx].val[0] + *(float32x4_t *)&vOpw0[synvoice_idx].val[0];
//    tmp -= vcvtq_f32_u32(vcvtq_u32_f32(tmp));
//    vPhase[synvoice_idx].val[0] = vget_low_f32(tmp);
//    vPhase[synvoice_idx].val[1] = vget_high_f32(tmp);
//    vst1q_f32((float32_t *)vPhase[synvoice_idx].val, tmp);

//__asm__ volatile ( "nop\nnop\nnop\nnop\nnop\n");


//__asm__ volatile ( "nop\nnop\nnop\nnop\nnop\n");
//float32x4x2_t t = *(float32x4x2_t *)&vModMatrix[synvoice_idx][0].val[0];
//float32x4x2_t o = *(float32x4x2_t *)&s_opval[synvoice_idx][0];
//tmp = t.val[0] * o.val[0] + t.val[1] * o.val[1];

/*
    float32x4_t tmp, o1,o2,o3,o4,o5,o6;
    float32x2_t tmp1, tmp2;

    float32x2_t w0, w1, w2;
    float32x2_t vPhaseNext1, vPhaseNext2, vPhaseNext3;
    int32x2_t vPhaseNext1i, vPhaseNext2i, vPhaseNext3i;
//    float32x2_t vWaves;
    int32x2_t vEgLut1, vEgLut2, vEgLut3;
//    float32x2_t vEgOut;
  
//__asm__ volatile ( "nop\nnop\nnop\nnop\n");
    w0 = vPhase[synvoice_idx].val[0];
    w1 = vPhase[synvoice_idx].val[1];
    w2 = vPhase[synvoice_idx].val[2];
    vPhaseNext1 = w0 + vOpw0[synvoice_idx].val[0];
    vPhaseNext2 = w1 + vOpw0[synvoice_idx].val[1];
    vPhaseNext3 = w2 + vOpw0[synvoice_idx].val[2];
    vPhaseNext1i = vcvt_s32_f32(vPhaseNext1);
    vPhaseNext2i = vcvt_s32_f32(vPhaseNext2);
    vPhaseNext3i = vcvt_s32_f32(vPhaseNext3);
    vPhaseNext1 -= vcvt_f32_s32(vPhaseNext1i);
    vPhaseNext2 -= vcvt_f32_s32(vPhaseNext2i);
    vPhaseNext3 -= vcvt_f32_s32(vPhaseNext3i);
    vPhase[synvoice_idx].val[0] = vPhaseNext1;
    vPhase[synvoice_idx].val[1] = vPhaseNext2;
    vPhase[synvoice_idx].val[2] = vPhaseNext3;
//__asm__ volatile ( "nop\nnop\nnop\nnop\n");
      o1 = vModMatrix[synvoice_idx][0].val[0] * vld1q_f32(&s_opval[synvoice_idx][0]) + vModMatrix[synvoice_idx][0].val[1] * vld1q_f32(&s_opval[synvoice_idx][4]);
      o2 = vModMatrix[synvoice_idx][1].val[0] * vld1q_f32(&s_opval[synvoice_idx][0]) + vModMatrix[synvoice_idx][1].val[1] * vld1q_f32(&s_opval[synvoice_idx][4]);
      o3 = vModMatrix[synvoice_idx][2].val[0] * vld1q_f32(&s_opval[synvoice_idx][0]) + vModMatrix[synvoice_idx][2].val[1] * vld1q_f32(&s_opval[synvoice_idx][4]);
      o4 = vModMatrix[synvoice_idx][3].val[0] * vld1q_f32(&s_opval[synvoice_idx][0]) + vModMatrix[synvoice_idx][3].val[1] * vld1q_f32(&s_opval[synvoice_idx][4]);
      o5 = vModMatrix[synvoice_idx][4].val[0] * vld1q_f32(&s_opval[synvoice_idx][0]) + vModMatrix[synvoice_idx][4].val[1] * vld1q_f32(&s_opval[synvoice_idx][4]);
      o6 = vModMatrix[synvoice_idx][5].val[0] * vld1q_f32(&s_opval[synvoice_idx][0]) + vModMatrix[synvoice_idx][5].val[1] * vld1q_f32(&s_opval[synvoice_idx][4]);
//      w0.val[0] += vpadd_f32(tmp1, tmp2);
    w0 += vpadd_f32(vget_low_f32(o1) + vget_high_f32(o1), vget_low_f32(o2) + vget_high_f32(o2));
    w1 += vpadd_f32(vget_low_f32(o3) + vget_high_f32(o3), vget_low_f32(o4) + vget_high_f32(o4));
    w2 += vpadd_f32(vget_low_f32(o5) + vget_high_f32(o5), vget_low_f32(o6) + vget_high_f32(o6));
//    vWaves = {wave(sWave, w0[0], wave(sWave, w0[1])};    
    vEgLut1 = vshr_n_s32(vmax_s32(vqadd_s32(*(int32x2_t *)&s_egval[synvoice_idx][0], *(int32x2_t *)&s_oplevel[synvoice_idx][0]), vdup_n_s32(0)), EG_LUT_SHR + 1);
    vEgLut2 = vshr_n_s32(vmax_s32(vqadd_s32(*(int32x2_t *)&s_egval[synvoice_idx][2], *(int32x2_t *)&s_oplevel[synvoice_idx][2]), vdup_n_s32(0)), EG_LUT_SHR + 1);
    vEgLut3 = vshr_n_s32(vmax_s32(vqadd_s32(*(int32x2_t *)&s_egval[synvoice_idx][4], *(int32x2_t *)&s_oplevel[synvoice_idx][4]), vdup_n_s32(0)), EG_LUT_SHR + 1);

//    w0 = (float32x2_t){wave(&s_waveform[synvoice_idx][0], w0[0]), wave(&s_waveform[synvoice_idx][1], w0[1])};
//    w1 = (float32x2_t){wave(&s_waveform[synvoice_idx][2], w1[0]), wave(&s_waveform[synvoice_idx][3], w1[1])};
//    w2 = (float32x2_t){wave(&s_waveform[synvoice_idx][4], w2[0]), wave(&s_waveform[synvoice_idx][5], w2[1])};

//__asm__ volatile ( "nop\nnop\nnop\nnop\n");

    w0 = wave(&s_wave_sample_ptr[synvoice_idx][0], *(float32x2_t *)&s_wave_size[synvoice_idx][0], *(uint32x2_t *)&s_wave_size_mask[synvoice_idx][0], w0);
    w1 = wave(&s_wave_sample_ptr[synvoice_idx][2], *(float32x2_t *)&s_wave_size[synvoice_idx][2], *(uint32x2_t *)&s_wave_size_mask[synvoice_idx][2], w1);
    w2 = wave(&s_wave_sample_ptr[synvoice_idx][4], *(float32x2_t *)&s_wave_size[synvoice_idx][4], *(uint32x2_t *)&s_wave_size_mask[synvoice_idx][4], w2);

    *(float32x2_t *)&s_opval[synvoice_idx][0] = w0 * (float32x2_t){eg_lut[vEgLut1[0]], eg_lut[vEgLut1[1]]};
    *(float32x2_t *)&s_opval[synvoice_idx][2] = w1 * (float32x2_t){eg_lut[vEgLut2[0]], eg_lut[vEgLut2[1]]};
    *(float32x2_t *)&s_opval[synvoice_idx][4] = w2 * (float32x2_t){eg_lut[vEgLut3[0]], eg_lut[vEgLut3[1]]};
*/

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#ifdef UQ32_PHASE
    uq32x4x3_t vPhaseNext = *(uq32x4x3_t *)&vPhase[synvoice_idx];
    float32x4x3_t w0 = vcvtq_n_f32_uq32(vPhaseNext);
    *(uq32x4x3_t *)&vPhase[synvoice_idx] = vPhaseNext + *(uq32x4x3_t *)&vOpw0[synvoice_idx];
#else
    float32x4x3_t w0 = *(float32x4x3_t *)&vPhase[synvoice_idx];
    float32x4x3_t vPhaseNext = w0 + *(float32x4x3_t *)&vOpw0[synvoice_idx];  
    *(float32x4x3_t *)&vPhase[synvoice_idx] = vPhaseNext - vcvtq_f32_s32(vcvtq_s32_f32(vPhaseNext));
#endif
#pragma GCC diagnostic pop

    float32x4_t tmp;
    float32x2_t tmp2;
    int32x4x3_t vEgLut;

#if MODMATRIX == 1 || MODMATRIX == 2
    float32x4_t o1, o2, o3, o4, o5, o6, o7, o8, o9, o10, o11, o12;
    o1 = vModMatrix[synvoice_idx][0].val[0] * vld1q_f32(&s_opval[synvoice_idx][0]) + vModMatrix[synvoice_idx][0].val[1] * vld1q_f32(&s_opval[synvoice_idx][4]);
    o2 = vModMatrix[synvoice_idx][1].val[0] * vld1q_f32(&s_opval[synvoice_idx][0]) + vModMatrix[synvoice_idx][1].val[1] * vld1q_f32(&s_opval[synvoice_idx][4]);
    o3 = vModMatrix[synvoice_idx][2].val[0] * vld1q_f32(&s_opval[synvoice_idx][0]) + vModMatrix[synvoice_idx][2].val[1] * vld1q_f32(&s_opval[synvoice_idx][4]);
    o4 = vModMatrix[synvoice_idx][3].val[0] * vld1q_f32(&s_opval[synvoice_idx][0]) + vModMatrix[synvoice_idx][3].val[1] * vld1q_f32(&s_opval[synvoice_idx][4]);
    o5 = vModMatrix[synvoice_idx][4].val[0] * vld1q_f32(&s_opval[synvoice_idx][0]) + vModMatrix[synvoice_idx][4].val[1] * vld1q_f32(&s_opval[synvoice_idx][4]);
    o6 = vModMatrix[synvoice_idx][5].val[0] * vld1q_f32(&s_opval[synvoice_idx][0]) + vModMatrix[synvoice_idx][5].val[1] * vld1q_f32(&s_opval[synvoice_idx][4]);
    o7 = vModMatrix[synvoice_idx + 1][0].val[0] * vld1q_f32(&s_opval[synvoice_idx + 1][0]) + vModMatrix[synvoice_idx + 1][0].val[1] * vld1q_f32(&s_opval[synvoice_idx + 1][4]);
    o8 = vModMatrix[synvoice_idx + 1][1].val[0] * vld1q_f32(&s_opval[synvoice_idx + 1][0]) + vModMatrix[synvoice_idx + 1][1].val[1] * vld1q_f32(&s_opval[synvoice_idx + 1][4]);
    o9 = vModMatrix[synvoice_idx + 1][2].val[0] * vld1q_f32(&s_opval[synvoice_idx + 1][0]) + vModMatrix[synvoice_idx + 1][2].val[1] * vld1q_f32(&s_opval[synvoice_idx + 1][4]);
    o10 = vModMatrix[synvoice_idx + 1][3].val[0] * vld1q_f32(&s_opval[synvoice_idx + 1][0]) + vModMatrix[synvoice_idx + 1][3].val[1] * vld1q_f32(&s_opval[synvoice_idx + 1][4]);
    o11 = vModMatrix[synvoice_idx + 1][4].val[0] * vld1q_f32(&s_opval[synvoice_idx + 1][0]) + vModMatrix[synvoice_idx + 1][4].val[1] * vld1q_f32(&s_opval[synvoice_idx + 1][4]);
    o12 = vModMatrix[synvoice_idx + 1][5].val[0] * vld1q_f32(&s_opval[synvoice_idx + 1][0]) + vModMatrix[synvoice_idx + 1][5].val[1] * vld1q_f32(&s_opval[synvoice_idx + 1][4]);
    w0.val[0] += vcombine_f32(vpadd_f32(vget_low_f32(o1) + vget_high_f32(o1), vget_low_f32(o2) + vget_high_f32(o2)), vpadd_f32(vget_low_f32(o3) + vget_high_f32(o3), vget_low_f32(o4) + vget_high_f32(o4)));
    w0.val[1] += vcombine_f32(vpadd_f32(vget_low_f32(o5) + vget_high_f32(o5), vget_low_f32(o6) + vget_high_f32(o6)), vpadd_f32(vget_low_f32(o7) + vget_high_f32(o7), vget_low_f32(o8) + vget_high_f32(o8)));
    w0.val[2] += vcombine_f32(vpadd_f32(vget_low_f32(o9) + vget_high_f32(o9), vget_low_f32(o10) + vget_high_f32(o10)), vpadd_f32(vget_low_f32(o11) + vget_high_f32(o11), vget_low_f32(o12) + vget_high_f32(o12)));
#elif MODMATRIX == 3
    float32x4_t o1, o2, o3, o4, o5, o6, o7, o8, o9, o10, o11, o12;
    float32x4_t o13, o14, o15, o16, o17, o18;
    o1 = *(float32x4_t *)&vModMatrix[0][synvoice_idx].val[0] * *(float32x4_t *)&s_opval[synvoice_idx][0];
    o2 = *(float32x4_t *)&vModMatrix[0][synvoice_idx].val[2] * *(float32x4_t *)&s_opval[synvoice_idx][4];
    o3 = *(float32x4_t *)&vModMatrix[0][synvoice_idx + 1].val[1] * *(float32x4_t *)&s_opval[synvoice_idx + 1][2];
    o4 = *(float32x4_t *)&vModMatrix[1][synvoice_idx].val[0] * *(float32x4_t *)&s_opval[synvoice_idx][0];
    o5 = *(float32x4_t *)&vModMatrix[1][synvoice_idx].val[2] * *(float32x4_t *)&s_opval[synvoice_idx][4];
    o6 = *(float32x4_t *)&vModMatrix[1][synvoice_idx + 1].val[1] * *(float32x4_t *)&s_opval[synvoice_idx + 1][2];
    o7 = *(float32x4_t *)&vModMatrix[2][synvoice_idx].val[0] * *(float32x4_t *)&s_opval[synvoice_idx][0];
    o8 = *(float32x4_t *)&vModMatrix[2][synvoice_idx].val[2] * *(float32x4_t *)&s_opval[synvoice_idx][4];
    o9 = *(float32x4_t *)&vModMatrix[2][synvoice_idx + 1].val[1] * *(float32x4_t *)&s_opval[synvoice_idx + 1][2];
    o10 = *(float32x4_t *)&vModMatrix[3][synvoice_idx].val[0] * *(float32x4_t *)&s_opval[synvoice_idx][0];
    o11 = *(float32x4_t *)&vModMatrix[3][synvoice_idx].val[2] * *(float32x4_t *)&s_opval[synvoice_idx][4];
    o12 = *(float32x4_t *)&vModMatrix[3][synvoice_idx + 1].val[1] * *(float32x4_t *)&s_opval[synvoice_idx + 1][2];
    o13 = *(float32x4_t *)&vModMatrix[4][synvoice_idx].val[0] * *(float32x4_t *)&s_opval[synvoice_idx][0];
    o14 = *(float32x4_t *)&vModMatrix[4][synvoice_idx].val[2] * *(float32x4_t *)&s_opval[synvoice_idx][4];
    o15 = *(float32x4_t *)&vModMatrix[4][synvoice_idx + 1].val[1] * *(float32x4_t *)&s_opval[synvoice_idx + 1][2];
    o16 = *(float32x4_t *)&vModMatrix[5][synvoice_idx].val[0] * *(float32x4_t *)&s_opval[synvoice_idx][0];
    o17 = *(float32x4_t *)&vModMatrix[5][synvoice_idx].val[2] * *(float32x4_t *)&s_opval[synvoice_idx][4];
    o18 = *(float32x4_t *)&vModMatrix[5][synvoice_idx + 1].val[1] * *(float32x4_t *)&s_opval[synvoice_idx + 1][2];

    o2 += vcombine_f32(vpadd_f32(vget_low_f32(o1), vget_high_f32(o1)), vpadd_f32(vget_low_f32(o3), vget_high_f32(o3)));
    o5 += vcombine_f32(vpadd_f32(vget_low_f32(o4), vget_high_f32(o4)), vpadd_f32(vget_low_f32(o6), vget_high_f32(o6)));
    o8 += vcombine_f32(vpadd_f32(vget_low_f32(o7), vget_high_f32(o7)), vpadd_f32(vget_low_f32(o9), vget_high_f32(o9)));
    o11 += vcombine_f32(vpadd_f32(vget_low_f32(o10), vget_high_f32(o10)), vpadd_f32(vget_low_f32(o12), vget_high_f32(o12)));
    o14 += vcombine_f32(vpadd_f32(vget_low_f32(o13), vget_high_f32(o13)), vpadd_f32(vget_low_f32(o15), vget_high_f32(o15)));
    o17 += vcombine_f32(vpadd_f32(vget_low_f32(o16), vget_high_f32(o16)), vpadd_f32(vget_low_f32(o18), vget_high_f32(o18)));

    w0.val[0] += vcombine_f32(vpadd_f32(vget_low_f32(o2), vget_high_f32(o2)), vpadd_f32(vget_low_f32(o5), vget_high_f32(o5)));
    w0.val[1] += vcombine_f32(vpadd_f32(vget_low_f32(o8), vget_high_f32(o8)), vpadd_f32(vget_low_f32(o11), vget_high_f32(o11)));
    w0.val[2] += vcombine_f32(vpadd_f32(vget_low_f32(o14), vget_high_f32(o14)), vpadd_f32(vget_low_f32(o17), vget_high_f32(o17)));
    /*
          float32x4x2_t t1 = vtrnq_f32(o1, o3);
          float32x4x2_t t2 = vtrnq_f32(o4, o6);
          o1 = t1.val[0] + t1.val[1];
          o4 = t2.val[0] + t2.val[1];
          w0.val[0] += vcombine_f32(vpadd_f32(vget_low_f32(o2), vget_high_f32(o2)), vpadd_f32(vget_low_f32(o5), vget_high_f32(o5))) +
            vcombine_f32(vget_low_f32(o1), vget_low_f32(o4)) +
            vcombine_f32(vget_high_f32(o1), vget_high_f32(o4));
    */

#elif MODMATRIX == 4
//__asm__ volatile ( "nop\nnop\nnop\nnop\n");
    float32x4_t t1, t2;
    t1 = vld1q_dup_f32(&s_opval[synvoice_idx][0]);
    t2 = vld1q_dup_f32(&s_opval[synvoice_idx + 1][0]);
    w0.val[0] += *(float32x4_t *)&vModMatrix[synvoice_idx][0].val[0] * t1;
    w0.val[2] += *(float32x4_t *)&vModMatrix[synvoice_idx + 1][0].val[1] * t2;
    w0.val[1] += *(float32x4_t *)&vModMatrix[synvoice_idx][0].val[2] * vextq_f32(t1, t2, 2);

    t1 = vld1q_dup_f32(&s_opval[synvoice_idx][1]);
    t2 = vld1q_dup_f32(&s_opval[synvoice_idx + 1][1]);
    w0.val[0] += *(float32x4_t *)&vModMatrix[synvoice_idx][1].val[0] * t1;
    w0.val[2] += *(float32x4_t *)&vModMatrix[synvoice_idx + 1][1].val[1] * t2;
    w0.val[1] += *(float32x4_t *)&vModMatrix[synvoice_idx][1].val[2] * vextq_f32(t1, t2, 2);

    t1 = vld1q_dup_f32(&s_opval[synvoice_idx][2]);
    t2 = vld1q_dup_f32(&s_opval[synvoice_idx + 1][2]);
    w0.val[0] += *(float32x4_t *)&vModMatrix[synvoice_idx][2].val[0] * t1;
    w0.val[2] += *(float32x4_t *)&vModMatrix[synvoice_idx + 1][2].val[1] * t2;
    w0.val[1] += *(float32x4_t *)&vModMatrix[synvoice_idx][2].val[2] * vextq_f32(t1, t2, 2);

    t1 = vld1q_dup_f32(&s_opval[synvoice_idx][3]);
    t2 = vld1q_dup_f32(&s_opval[synvoice_idx + 1][3]);
    w0.val[0] += *(float32x4_t *)&vModMatrix[synvoice_idx][3].val[0] * t1;
    w0.val[2] += *(float32x4_t *)&vModMatrix[synvoice_idx + 1][3].val[1] * t2;
    w0.val[1] += *(float32x4_t *)&vModMatrix[synvoice_idx][3].val[2] * vextq_f32(t1, t2, 2);

    t1 = vld1q_dup_f32(&s_opval[synvoice_idx][4]);
    t2 = vld1q_dup_f32(&s_opval[synvoice_idx + 1][4]);
    w0.val[0] += *(float32x4_t *)&vModMatrix[synvoice_idx][4].val[0] * t1;
    w0.val[2] += *(float32x4_t *)&vModMatrix[synvoice_idx + 1][4].val[1] * t2;
    w0.val[1] += *(float32x4_t *)&vModMatrix[synvoice_idx][4].val[2] * vextq_f32(t1, t2, 2);

    t1 = vld1q_dup_f32(&s_opval[synvoice_idx][5]);
    t2 = vld1q_dup_f32(&s_opval[synvoice_idx + 1][5]);
    w0.val[0] += *(float32x4_t *)&vModMatrix[synvoice_idx][5].val[0] * t1;
    w0.val[2] += *(float32x4_t *)&vModMatrix[synvoice_idx + 1][5].val[1] * t2;
    w0.val[1] += *(float32x4_t *)&vModMatrix[synvoice_idx][5].val[2] * vextq_f32(t1, t2, 2);
#endif
    //__asm__ volatile ( "nop\nnop\nnop\nnop\n");

#if MODMATRIX != 1
    ((float*)&w0.val[0])[s_feedback_dst[synvoice_idx][0]] += vFeedbackOutput[synvoice_idx][0];
    ((float*)&w0.val[0])[s_feedback_dst[synvoice_idx][1]] += vFeedbackOutput[synvoice_idx][1];
    ((float*)&w0.val[1][2])[s_feedback_dst[synvoice_idx+1][0]] += vFeedbackOutput[synvoice_idx+1][0];
    ((float*)&w0.val[1][2])[s_feedback_dst[synvoice_idx+1][1]] += vFeedbackOutput[synvoice_idx+1][1];
#endif

    vEgLut.val[0] = vshrq_n_s32(vmaxq_s32(vqaddq_s32(*(int32x4_t *)&s_egval[synvoice_idx][0], *(int32x4_t *)&s_oplevel[synvoice_idx][0]), vdupq_n_s32(0)), EG_LUT_SHR + 1);
    vEgLut.val[1] = vshrq_n_s32(vmaxq_s32(vqaddq_s32(*(int32x4_t *)&s_egval[synvoice_idx][4], *(int32x4_t *)&s_oplevel[synvoice_idx][4]), vdupq_n_s32(0)), EG_LUT_SHR + 1);
    vEgLut.val[2] = vshrq_n_s32(vmaxq_s32(vqaddq_s32(*(int32x4_t *)&s_egval[synvoice_idx+1][2], *(int32x4_t *)&s_oplevel[synvoice_idx+1][2]), vdupq_n_s32(0)), EG_LUT_SHR + 1);

    w0.val[0] = wave(&s_wave_sample_ptr[synvoice_idx][0], *(float32x4_t *)&s_wave_size[synvoice_idx][0], *(uint32x4_t *)&s_wave_size_mask[synvoice_idx][0], w0.val[0]);
    w0.val[1] = wave(&s_wave_sample_ptr[synvoice_idx][4], *(float32x4_t *)&s_wave_size[synvoice_idx][4], *(uint32x4_t *)&s_wave_size_mask[synvoice_idx][4], w0.val[1]);
    w0.val[2] = wave(&s_wave_sample_ptr[synvoice_idx+1][2], *(float32x4_t *)&s_wave_size[synvoice_idx+1][2], *(uint32x4_t *)&s_wave_size_mask[synvoice_idx+1][2], w0.val[2]);

    *(float32x4_t *)&s_opval[synvoice_idx][0] = w0.val[0] * (float32x4_t){eg_lut[vEgLut.val[0][0]], eg_lut[vEgLut.val[0][1]], eg_lut[vEgLut.val[0][2]], eg_lut[vEgLut.val[0][3]]};
#if MODMATRIX == 1
    tmp = w0.val[1] * (float32x4_t){eg_lut[vEgLut.val[1][0]], eg_lut[vEgLut.val[1][1]], eg_lut[vEgLut.val[1][2]], eg_lut[vEgLut.val[1][3]]};
    *(float32x2_t *)&s_opval[synvoice_idx][4] = vget_low_f32(tmp);
    *(float32x2_t *)&s_opval[synvoice_idx+1][0] = vget_high_f32(tmp);
#else
    *(float32x4_t *)&s_opval[synvoice_idx][4] = w0.val[1] * (float32x4_t){eg_lut[vEgLut.val[1][0]], eg_lut[vEgLut.val[1][1]], eg_lut[vEgLut.val[1][2]], eg_lut[vEgLut.val[1][3]]};
#endif
    *(float32x4_t *)&s_opval[synvoice_idx+1][2] = w0.val[2] * (float32x4_t){eg_lut[vEgLut.val[2][0]], eg_lut[vEgLut.val[2][1]], eg_lut[vEgLut.val[2][2]], eg_lut[vEgLut.val[2][3]]};

/*
    float32x4x2_t m12 = vtrnq_f32(
      *(float32x4_t *)&vModMatrix[synvoice_idx][0].val[0] * *(float32x4_t *)&s_opval[synvoice_idx][0] + *(float32x4_t *)&vModMatrix[synvoice_idx][0].val[1] * *(float32x4_t *)&s_opval[synvoice_idx][4],
      *(float32x4_t *)&vModMatrix[synvoice_idx][1].val[0] * *(float32x4_t *)&s_opval[synvoice_idx][0] + *(float32x4_t *)&vModMatrix[synvoice_idx][1].val[1] * *(float32x4_t *)&s_opval[synvoice_idx][4]
    );
    float32x4x2_t m34 = vtrnq_f32(
      *(float32x4_t *)&vModMatrix[synvoice_idx][2].val[0] * *(float32x4_t *)&s_opval[synvoice_idx][0] + *(float32x4_t *)&vModMatrix[synvoice_idx][2].val[1] * *(float32x4_t *)&s_opval[synvoice_idx][4],
      *(float32x4_t *)&vModMatrix[synvoice_idx][3].val[0] * *(float32x4_t *)&s_opval[synvoice_idx][0] + *(float32x4_t *)&vModMatrix[synvoice_idx][3].val[1] * *(float32x4_t *)&s_opval[synvoice_idx][4]
    );
    float32x4x2_t m1234 = vuzpq_f32(m12.val[0] + m12.val[1], m34.val[0] + m34.val[1]);
    w0.val[0] += m1234.val[0] + m1234.val[1];

    float32x4x2_t m56 = vtrnq_f32(
      *(float32x4_t *)&vModMatrix[synvoice_idx][4].val[0] * *(float32x4_t *)&s_opval[synvoice_idx][0] + *(float32x4_t *)&vModMatrix[synvoice_idx][4].val[1] * *(float32x4_t *)&s_opval[synvoice_idx][4],
      *(float32x4_t *)&vModMatrix[synvoice_idx][5].val[0] * *(float32x4_t *)&s_opval[synvoice_idx][0] + *(float32x4_t *)&vModMatrix[synvoice_idx][5].val[1] * *(float32x4_t *)&s_opval[synvoice_idx][4]
    );
    float32x4x2_t m78 = vtrnq_f32(
      *(float32x4_t *)&vModMatrix[synvoice_idx+1][0].val[0] * *(float32x4_t *)&s_opval[synvoice_idx+1][0] + *(float32x4_t *)&vModMatrix[synvoice_idx+1][0].val[1] * *(float32x4_t *)&s_opval[synvoice_idx+1][4],
      *(float32x4_t *)&vModMatrix[synvoice_idx+1][1].val[0] * *(float32x4_t *)&s_opval[synvoice_idx+1][0] + *(float32x4_t *)&vModMatrix[synvoice_idx+1][1].val[1] * *(float32x4_t *)&s_opval[synvoice_idx+1][4]
    );
    float32x4x2_t m5678 = vuzpq_f32(m56.val[0] + m56.val[1], m78.val[0] + m78.val[1]);
    w0.val[1] += m5678.val[0] + m5678.val[1];

    float32x4x2_t m9A = vtrnq_f32(
      *(float32x4_t *)&vModMatrix[synvoice_idx+1][2].val[0] * *(float32x4_t *)&s_opval[synvoice_idx+1][0] + *(float32x4_t *)&vModMatrix[synvoice_idx+1][2].val[1] * *(float32x4_t *)&s_opval[synvoice_idx+1][4],
      *(float32x4_t *)&vModMatrix[synvoice_idx+1][3].val[0] * *(float32x4_t *)&s_opval[synvoice_idx+1][0] + *(float32x4_t *)&vModMatrix[synvoice_idx+1][3].val[1] * *(float32x4_t *)&s_opval[synvoice_idx+1][4]
    );
    float32x4x2_t mBC = vtrnq_f32(
      *(float32x4_t *)&vModMatrix[synvoice_idx+1][4].val[0] * *(float32x4_t *)&s_opval[synvoice_idx+1][0] + *(float32x4_t *)&vModMatrix[synvoice_idx+1][4].val[1] * *(float32x4_t *)&s_opval[synvoice_idx+1][4],
      *(float32x4_t *)&vModMatrix[synvoice_idx+1][5].val[0] * *(float32x4_t *)&s_opval[synvoice_idx+1][0] + *(float32x4_t *)&vModMatrix[synvoice_idx+1][5].val[1] * *(float32x4_t *)&s_opval[synvoice_idx+1][4]
    );
    float32x4x2_t m9ABC = vuzpq_f32(m9A.val[0] + m9A.val[1], mBC.val[0] + mBC.val[1]);
    w0.val[2] += m9ABC.val[0] + m9ABC.val[1];
*/

/*
      s_opval[synvoice_idx][0] = wave(sWave, w0.val[0][0]) * eg_lut[usat_asr(31, q31add(s_egval[synvoice_idx][0], s_oplevel[synvoice_idx][0]), (EG_LUT_SHR + 1))];
      s_opval[synvoice_idx][1] = wave(sWave, w0.val[0][1]) * eg_lut[usat_asr(31, q31add(s_egval[synvoice_idx][1], s_oplevel[synvoice_idx][1]), (EG_LUT_SHR + 1))];
      s_opval[synvoice_idx][2] = wave(sWave, w0.val[1][0]) * eg_lut[usat_asr(31, q31add(s_egval[synvoice_idx][2], s_oplevel[synvoice_idx][2]), (EG_LUT_SHR + 1))];
      s_opval[synvoice_idx][3] = wave(sWave, w0.val[1][1]) * eg_lut[usat_asr(31, q31add(s_egval[synvoice_idx][3], s_oplevel[synvoice_idx][3]), (EG_LUT_SHR + 1))];
*/
/*
      int32x2x3_t vEgVal = *(int32x2x3_t *)s_egval[synvoice_idx];
      int32x2x3_t vOpLevel = *(int32x2x3_t *)s_oplevel[synvoice_idx];
      int32x2x3_t vEgLut = {
      vshr_n_s32(
        vmax_s32(
          vqadd_s32(vEgVal.val[0], vOpLevel.val[0]),
          vdup_n_s32(0)
        ),
        EG_LUT_SHR + 1
      ),
      vshr_n_s32(
        vmax_s32(
          vqadd_s32(vEgVal.val[1], vOpLevel.val[1]),
          vdup_n_s32(0)
        ),
        EG_LUT_SHR + 1
      ),
      vshr_n_s32(
        vmax_s32(
          vqadd_s32(vEgVal.val[2], vOpLevel.val[2]),
          vdup_n_s32(0)
        ),
        EG_LUT_SHR + 1
      )
      };
*/
/*
    int32x4x3_t vEgVal = {
      vld1q_s32((int32_t *)&s_egval[synvoice_idx][0]),
      vld1q_s32((int32_t *)&s_egval[synvoice_idx][4]),
      vld1q_s32((int32_t *)&s_egval[synvoice_idx+1][2])
    };
    int32x4x3_t vOpLevel = {
      vld1q_s32((int32_t *)&s_oplevel[synvoice_idx][0]),
      vld1q_s32((int32_t *)&s_oplevel[synvoice_idx][4]),
      vld1q_s32((int32_t *)&s_oplevel[synvoice_idx+1][2])
    };

    int32x4x3_t vEgLut = {
      vshrq_n_s32(vmaxq_s32(vqaddq_s32(vEgVal.val[0], vOpLevel.val[0]), vdupq_n_s32(0)), EG_LUT_SHR + 1),
      vshrq_n_s32(vmaxq_s32(vqaddq_s32(vEgVal.val[1], vOpLevel.val[1]), vdupq_n_s32(0)), EG_LUT_SHR + 1),
      vshrq_n_s32(vmaxq_s32(vqaddq_s32(vEgVal.val[2], vOpLevel.val[2]), vdupq_n_s32(0)), EG_LUT_SHR + 1)
    };
*/

/*
      s_opval[synvoice_idx][0] = wave(sWave, w0.val[0][0]) * eg_lut[vEgLut.val[0][0]];
      s_opval[synvoice_idx][1] = wave(sWave, w0.val[0][1]) * eg_lut[vEgLut.val[0][1]];
      s_opval[synvoice_idx][2] = wave(sWave, w0.val[1][0]) * eg_lut[vEgLut.val[1][0]];
      s_opval[synvoice_idx][3] = wave(sWave, w0.val[1][1]) * eg_lut[vEgLut.val[1][1]];
*/

/*    w0.val[2] = vPhase[synvoice_idx].val[2];
    vPhase[synvoice_idx].val[2] += vOpw0[synvoice_idx].val[2];
    vPhase[synvoice_idx].val[2] -= vcvt_f32_u32(vcvt_u32_f32(vPhase[synvoice_idx].val[2]));
*/

//      s_opval[synvoice_idx][4] = wave(sWave, w0.val[2][0]) * eg_lut[usat_asr(31, q31add(s_egval[synvoice_idx][4], s_oplevel[synvoice_idx][4]), (EG_LUT_SHR + 1))];
//      s_opval[synvoice_idx][5] = wave(sWave, w0.val[2][1]) * eg_lut[usat_asr(31, q31add(s_egval[synvoice_idx][5], s_oplevel[synvoice_idx][5]), (EG_LUT_SHR + 1))];
//__asm__ volatile ( "nop\nnop\nnop\nnop\n");
//      s_opval[synvoice_idx][4] = wave(sWave, w0.val[2][0]) * eg_lut[vEgLut.val[2][0]];
//      s_opval[synvoice_idx][5] = wave(sWave, w0.val[2][1]) * eg_lut[vEgLut.val[2][1]];
//__asm__ volatile ( "nop\nnop\nnop\nnop\n");

/*
      float32x2x3_t vEgOut = {
        eg_lut[vEgLut.val[0][0]],
        eg_lut[vEgLut.val[0][1]],
        eg_lut[vEgLut.val[1][0]],
        eg_lut[vEgLut.val[1][1]],
        eg_lut[vEgLut.val[2][0]],
        eg_lut[vEgLut.val[2][1]]
      };

      float32x2x3_t vWaves = {
        wave(sWave, w0.val[0][0]),
        wave(sWave, w0.val[0][1]),
        wave(sWave, w0.val[1][0]),
        wave(sWave, w0.val[1][1]),
        wave(sWave, w0.val[2][0]),
        wave(sWave, w0.val[2][1]),
      };

      vst1_f32(&s_opval[synvoice_idx][0], vWaves.val[0] * vEgOut.val[0]);
      vst1_f32(&s_opval[synvoice_idx][2], vWaves.val[1] * vEgOut.val[1]);
      vst1_f32(&s_opval[synvoice_idx][4], vWaves.val[2] * vEgOut.val[2]);
*/

/*
      float32x4x3_t vEgOut = {
        eg_lut[vEgLut.val[0][0]],
        eg_lut[vEgLut.val[0][1]],
        eg_lut[vEgLut.val[0][2]],
        eg_lut[vEgLut.val[0][3]],
        eg_lut[vEgLut.val[1][0]],
        eg_lut[vEgLut.val[1][1]],
        eg_lut[vEgLut.val[1][2]],
        eg_lut[vEgLut.val[1][3]],
        eg_lut[vEgLut.val[2][0]],
        eg_lut[vEgLut.val[2][1]],
        eg_lut[vEgLut.val[2][2]],
        eg_lut[vEgLut.val[2][3]]
      };

      float32x4x3_t vWaves = {
        wave(sWave, w0.val[0][0]),
        wave(sWave, w0.val[0][1]),
        wave(sWave, w0.val[0][2]),
        wave(sWave, w0.val[0][3]),
        wave(sWave, w0.val[1][0]),
        wave(sWave, w0.val[1][1]),
        wave(sWave, w0.val[1][2]),
        wave(sWave, w0.val[1][3]),
        wave(sWave, w0.val[2][0]),
        wave(sWave, w0.val[2][1]),
        wave(sWave, w0.val[2][2]),
        wave(sWave, w0.val[2][3])
      };

      vWaves.val[0] *= vEgOut.val[0];
      vWaves.val[1] *= vEgOut.val[1];
      vWaves.val[2] *= vEgOut.val[2];

      vst1q_f32(&s_opval[synvoice_idx][0], vWaves.val[0]);
      vst1_f32(&s_opval[synvoice_idx][4], vget_low_f32(vWaves.val[1]));
      vst1_f32(&s_opval[synvoice_idx+1][0], vget_high_f32(vWaves.val[1]));
      vst1q_f32(&s_opval[synvoice_idx+1][2], vWaves.val[2]);
*/

//__asm__ volatile ( "nop\nnop\nnop\nnop\n");
//    static uint32x2_t vFeedbackSrc = vcreate_u8(0x0706050403020100);
//    float32x2_t vFeedbackTmp;
//    vFeedbackTmp = vFeedbackShadow[synvoice_idx];
//    vFeedbackShadow[synvoice_idx] = vreinterpret_f32_u8(vtbl3_u8(vOpval, vreinterpret_u8_u32(vFeedbackSrc))) * vFeedback[synvoice_idx];
//  uint8x8x3_t o  = vld1_u8_x3((uint8_t *)s_opval[synvoice_idx]);
//    vFeedbackShadow[synvoice_idx] = vreinterpret_f32_u8(vtbl3_u8((uint8x8x3_t){
//      vreinterpret_u8_f32(vld1_f32(&s_opval[synvoice_idx][0])),
//      vreinterpret_u8_f32(vld1_f32(&s_opval[synvoice_idx][2])),
//      vreinterpret_u8_f32(vld1_f32(&s_opval[synvoice_idx][4]))
//    }, vFeedbackSource[synvoice_idx])) * vFeedbackLevel[synvoice_idx];
//__asm__ volatile ( "nop\nnop\nnop\nnop\n");
//#pragma GCC diagnostic push
//#pragma GCC diagnostic ignored "-Wstrict-aliasing"
//    vFeedbackShadow[synvoice_idx] = vreinterpret_f32_u8(vtbl3_u8(*(uint8x8x3_t *)s_opval[synvoice_idx], vFeedbackSource[synvoice_idx])) * vFeedbackLevel[synvoice_idx];
//#pragma GCC diagnostic pop
//    vst1_f32(&s_opval[synvoice_idx][OPERATOR_COUNT], vFeedbackTmp + vFeedbackShadow[synvoice_idx]);
//__asm__ volatile ( "nop\nnop\nnop\nnop\n");

//    for (uint32_t i = 0; i < OPERATOR_COUNT; i++) {
//      modw0 = 0.f;

//        __asm__ volatile ( "nop\nnop\nnop\n");
//      tmp = vbslq_f32(vAlgorithm[i].val[0], vld1q_f32(&s_opval[0]), vdupq_n_f32(0.f)) + vbslq_f32(vAlgorithm[i].val[1], vld1q_f32(&s_opval[4]), vdupq_n_f32(0.f));
///      tmp = vModMatrix[i].val[0] * vld1q_f32(&s_opval[0]) + vModMatrix[i].val[1] * vld1q_f32(&s_opval[4]);
///      tmp2 = vget_low_f32(tmp) + vget_high_f32(tmp);
///      modw0 = vpadd_f32(tmp2, tmp2)[0];

//        __asm__ volatile ( "nop\nnop\nnop\n");
/*
      if ((s_algorithm[i] & 0x40) != 0)
        modw0 += s_opval[7];

      if ((s_algorithm[i] & 0x20) != 0)
        modw0 += s_opval[6];

      for (uint32_t j = 0; j < i; j++)
        if (((s_algorithm[i] >> j) & 0x01) != 0)
          modw0 += s_opval[j];
*/
/*
      if (((s_algorithm[i] >> 0) & 1) != 0)
        modw0 += s_opval[0];
      if (((s_algorithm[i] >> 1) & 1) != 0)
        modw0 += s_opval[1];
      if (((s_algorithm[i] >> 2) & 1) != 0)
        modw0 += s_opval[2];
      if (((s_algorithm[i] >> 3) & 1) != 0)
        modw0 += s_opval[3];
      if (((s_algorithm[i] >> 4) & 1) != 0)
        modw0 += s_opval[4];
*/
//      modw0 = modw0 * MI_SCALE_FACTOR + s_phase[i];
///      modw0 += s_phase[i];
///      s_phase[i] += opw0[i];
///      s_phase[i] -= (uint32_t)s_phase[i];

//      s_opval[i] = smmul(OSC_FUNC(modw0), param_eglut(s_egval[i], s_oplevel[i])) << 1;
//      osc_out += smmul(s_opval[i], s_comp[i]) << 1;
//      s_opval[i] = wave(sWave, modw0) * eg_lut[usat_asr(31, q31add(s_egval[i], s_oplevel[i]), (EG_LUT_SHR + 1))];
//      s_opval[i] = wave(sWave, modw0) * q31_to_f32(param_eglut(s_egval[i], s_oplevel[i]));
///      s_opval[i] = wave(sWave, modw0) * eg_lut[usat_asr(31, q31add(s_egval[i], s_oplevel[i]), (EG_LUT_SHR + 1))];
//      s_opval[i] = wave_noli(sWave, modw0) * eg_lut[usat_asr(31, q31add(s_egval[i], s_oplevel[i]), (EG_LUT_SHR + 1))];
//      s_opval[i] = wave_q31(sWave, f32_to_q31(modw0 - (int32_t)modw0)) * q31_to_f32(param_eglut(s_egval[i], s_oplevel[i]));

//      osc_out += s_opval[i] * s_comp[i];
 
//      if ( s_sample_cnt[synvoice_idx] < s_eg_sample_cnt[synvoice_idx][i][s_egstage[synvoice_idx][i]] ) {
//        s_egval[synvoice_idx][i] = q31add(s_egval[synvoice_idx][i], s_egsrate[synvoice_idx][i][s_egstage[synvoice_idx][i]]);
//      } else {
//        s_egval[synvoice_idx][i] = s_eglevel[synvoice_idx][i][s_egstage[synvoice_idx][i]];
//        if (s_egstage[synvoice_idx][i] < EG_STAGE_COUNT - 2)
//          s_egstage[synvoice_idx][i]++;
//      }
//    }

//__asm__ volatile ( "nop\nnop\nnop\nnop\nnop\n");
//    float32x4_t t = vcombine_f32(vPhase.val[0], vPhase.val[1]) + vcombine_f32(vOpw0.val[0], vOpw0.val[1]);
//    t -= vcvtq_f32_u32(vcvtq_u32_f32(t));
//    vPhase.val[0] = vget_low_f32(t);
//    vPhase.val[1] = vget_high_f32(t);
//    vPhase.val[2] += vOpw0.val[2];
//    vPhase.val[2] -= vcvt_f32_u32(vcvt_u32_f32(vPhase.val[2]));
//__asm__ volatile ( "nop\nnop\nnop\nnop\nnop\n");
/*
    s_opval[synvoice_idx][OPERATOR_COUNT] = s_opval[synvoice_idx][OPERATOR_COUNT + FEEDBACK_COUNT];
//    s_opval[synvoice_idx][OPERATOR_COUNT + FEEDBACK_COUNT] = s_opval[synvoice_idx][s_feedback_src[synvoice_idx][0]] * s_feedback[synvoice_idx][0];
    s_opval[synvoice_idx][OPERATOR_COUNT + FEEDBACK_COUNT] = s_opval[synvoice_idx][s_feedback_src[synvoice_idx][0]] * vFeedback[synvoice_idx][0];
    s_opval[synvoice_idx][OPERATOR_COUNT] += s_opval[synvoice_idx][OPERATOR_COUNT + FEEDBACK_COUNT];
#if FEEDBACK_COUNT == 2
    s_opval[synvoice_idx][OPERATOR_COUNT + 1] = s_opval[synvoice_idx][OPERATOR_COUNT + FEEDBACK_COUNT + 1];
//    s_opval[synvoice_idx][OPERATOR_COUNT + FEEDBACK_COUNT] = s_opval[synvoice_idx][s_feedback_src[synvoice_idx][0]] * s_feedback[synvoice_idx][1];
    s_opval[synvoice_idx][OPERATOR_COUNT + FEEDBACK_COUNT] = s_opval[synvoice_idx][s_feedback_src[synvoice_idx][0]] * vFeedback[synvoice_idx][1];
    s_opval[synvoice_idx][OPERATOR_COUNT + 1] += s_opval[synvoice_idx][OPERATOR_COUNT + FEEDBACK_COUNT + 1];
#endif
*/

//      vst1_f32(out_p, vdup_n_f32(osc_out));
//__asm__ volatile ( "nop\nnop\nnop\nnop\n");

//__asm__ volatile ( "nop\nnop\nnop\nnop\n");
//    float32x2x3_t vOpVal = *(float32x2x3_t *);
/*
    float32x2_t vFeedbackTmp = vFeedbackShadow[synvoice_idx];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
    vFeedbackShadow[synvoice_idx] = vreinterpret_f32_u8(vtbl3_u8(*(uint8x8x3_t*)s_opval[synvoice_idx], vFeedbackSource[synvoice_idx])) * vFeedbackLevel[synvoice_idx];
#pragma GCC diagnostic pop
    vst1_f32(&s_opval[synvoice_idx][OPERATOR_COUNT], vFeedbackTmp + vFeedbackShadow[synvoice_idx]);
*/

//__asm__ volatile ( "nop\nnop\nnop\nnop\n");
    float32x4_t vFeedbackTmp = *(float32x4_t *)&vFeedbackShadow[synvoice_idx];
#if MODMATRIX == 1
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
    *(float32x4_t *)&vFeedbackShadow[synvoice_idx] = vreinterpretq_f32_u8(vcombine_u8(
      vtbl3_u8(*(uint8x8x3_t*)s_opval[synvoice_idx], vFeedbackSource[synvoice_idx]),
      vtbl3_u8(*(uint8x8x3_t*)s_opval[synvoice_idx+1], vFeedbackSource[synvoice_idx+1])
    )) * *(float32x4_t *)&vFeedbackLevel[synvoice_idx];
#pragma GCC diagnostic pop
    *(float32x4_t *)&vFeedbackTmp += *(float32x4_t *)&vFeedbackShadow[synvoice_idx];
    vst1_f32(&s_opval[synvoice_idx][OPERATOR_COUNT], vget_low_f32(vFeedbackTmp));
    vst1_f32(&s_opval[synvoice_idx+1][OPERATOR_COUNT], vget_high_f32(vFeedbackTmp));
#else
    *(float32x4_t *)&vFeedbackShadow[synvoice_idx] = (float32x4_t){
      s_opval[synvoice_idx][s_feedback_src[synvoice_idx][0]],
      s_opval[synvoice_idx][s_feedback_src[synvoice_idx][1]],
      s_opval[synvoice_idx+1][s_feedback_src[synvoice_idx+1][0]],
      s_opval[synvoice_idx+1][s_feedback_src[synvoice_idx+1][1]]
    } * *(float32x4_t *)&vFeedbackScale[synvoice_idx] * *(float32x4_t *)&vFeedbackLevel[synvoice_idx];
    *(float32x4_t *)&vFeedbackOutput[synvoice_idx] = vFeedbackTmp + *(float32x4_t *)&vFeedbackShadow[synvoice_idx];
#endif

/*
      tmp = vcombine_f32(vCompensation[synvoice_idx].val[0], vCompensation[synvoice_idx].val[1]) * vld1q_f32(&s_opval[synvoice_idx][0]);
      tmp2 = vget_low_f32(tmp) + vget_high_f32(tmp) + vCompensation[synvoice_idx].val[2] * vld1_f32(&s_opval[synvoice_idx][4]);
*/
      tmp = *(float32x4_t *)&vCompensation[synvoice_idx].val[0] * *(float32x4_t *)&s_opval[synvoice_idx][0] +
#if MODMATRIX == 1
      *(float32x4_t *)&vCompensation[synvoice_idx].val[2] * vcombine_f32(*(float32x2_t *)&s_opval[synvoice_idx][4], *(float32x2_t *)&s_opval[synvoice_idx+1][0]) + 
#else
      *(float32x4_t *)&vCompensation[synvoice_idx].val[2] * *(float32x4_t *)&s_opval[synvoice_idx][4] + 
#endif
      *(float32x4_t *)&vCompensation[synvoice_idx+1].val[1] * *(float32x4_t *)&s_opval[synvoice_idx+1][2];
      tmp2 = vget_low_f32(tmp) + vget_high_f32(tmp);

      voice_out += vpadd_f32(tmp2, tmp2);

//__asm__ volatile ("nop\n nop\n nop\n");
    for (uint32_t i = 0; i < OPERATOR_COUNT; i++) {
      if ( s_sample_cnt[synvoice_idx] < s_eg_sample_cnt[synvoice_idx][i][s_egstage[synvoice_idx][i]] ) {
        s_egval[synvoice_idx][i] = q31add(s_egval[synvoice_idx][i], s_egsrate[synvoice_idx][i][s_egstage[synvoice_idx][i]]);
      } else {
        s_egval[synvoice_idx][i] = s_eglevel[synvoice_idx][i][s_egstage[synvoice_idx][i]];
        if (s_egstage[synvoice_idx][i] < EG_STAGE_COUNT - 2)
          s_egstage[synvoice_idx][i]++;
      }

      if ( s_sample_cnt[synvoice_idx+1] < s_eg_sample_cnt[synvoice_idx+1][i][s_egstage[synvoice_idx+1][i]] ) {
        s_egval[synvoice_idx+1][i] = q31add(s_egval[synvoice_idx+1][i], s_egsrate[synvoice_idx+1][i][s_egstage[synvoice_idx+1][i]]);
      } else {
        s_egval[synvoice_idx+1][i] = s_eglevel[synvoice_idx+1][i][s_egstage[synvoice_idx+1][i]];
        if (s_egstage[synvoice_idx+1][i] < EG_STAGE_COUNT - 2)
          s_egstage[synvoice_idx+1][i]++;
      }
    }
//__asm__ volatile ("nop\n nop\n nop\n");

    if (
      s_sample_cnt[synvoice_idx] < s_peg_sample_cnt[synvoice_idx][s_pegstage[synvoice_idx]]
    ) {
      s_pegval[synvoice_idx] += s_pegrate[synvoice_idx][s_pegstage[synvoice_idx]];
    } else {
      s_pegval[synvoice_idx] = s_peglevel[synvoice_idx][s_pegstage[synvoice_idx]];
      if (s_pegstage[synvoice_idx] < PEG_STAGE_COUNT - 2)
        s_pegstage[synvoice_idx]++;
    }

    if (
      s_sample_cnt[synvoice_idx+1] < s_peg_sample_cnt[synvoice_idx+1][s_pegstage[synvoice_idx+1]]
    ) {
      s_pegval[synvoice_idx+1] += s_pegrate[synvoice_idx+1][s_pegstage[synvoice_idx+1]];
    } else {
      s_pegval[synvoice_idx+1] = s_peglevel[synvoice_idx+1][s_pegstage[synvoice_idx+1]];
      if (s_pegstage[synvoice_idx+1] < PEG_STAGE_COUNT - 2)
        s_pegstage[synvoice_idx+1]++;
    }

//    s_sample_cnt[synvoice_idx]++;
    (*(uint32x2_t*)&s_sample_cnt[synvoice_idx])++; 
  }
      vst1_f32(out_p, voice_out);  
    }    
}

__unit_callback void unit_set_param_value(uint8_t id, int32_t value) {
    sParams[id] = value;    
    switch (id) {
      case param_gate_note:
        break;
      case param_dxvoice_idx:
//        initVoice(sParams[param_dxvoice_idx]);
        break;
      case param_mode:
        if (value >= (int32_t)(mode_chord + sizeof(sChords)/sizeof(sChords[0]))) {
          getChordNotes(sChordNotes, value - mode_chord - sizeof(sChords)/sizeof(sChords[0]) + 1);
        }
        break;
      case param_detune:
        sDetune = value * .01f;
        break;
      case param_algorithm:
        s_algorithm_select = value;
        for (uint32_t synvoice_idx = 0; synvoice_idx < POLYPHONY; synvoice_idx++)
          setAlgorithm(synvoice_idx);
        break;
      case param_waveform_carriers:
      case param_waveform_modulators:
        s_waveform_cm[id - param_waveform_carriers] = value;
        for (uint32_t synvoice_idx = 0; synvoice_idx < POLYPHONY; synvoice_idx++)
          setWaveform(synvoice_idx);
        break;
      case param_feedback1_offset:
        id = 0;
        goto set_feedback;
      case param_feedback2_offset:
        id = 1;
  set_feedback:
        s_feedback_offset[id] = value * .0625f; // 1/16
        for (uint32_t synvoice_idx = 0; synvoice_idx < POLYPHONY; synvoice_idx++)
          setFeedback(synvoice_idx, id);
        break;
      case param_feedback1_route:
        id = 0;
        goto set_feedback_route;
      case param_feedback2_route:
        id = 1;
set_feedback_route:
        s_feedback_route[id] = value;
        for (uint32_t synvoice_idx = 0; synvoice_idx < POLYPHONY; synvoice_idx++)
          setFeedbackRoute(synvoice_idx, id);
        break;
        case param_level_offset_carriers:
        case param_level_offset_modulators:
          s_level_offset[param_level_offset_modulators - id + 6] = value * .25f;
          for (uint32_t synvoice_idx = 0; synvoice_idx < POLYPHONY; synvoice_idx++)
            setOutLevel(synvoice_idx);
          break;
        case param_rate_offset_carriers:
        case param_rate_offset_modulators:
          s_egrate_offset[param_rate_offset_modulators - id + 6] = value * .25f;
          for (uint32_t synvoice_idx = 0; synvoice_idx < POLYPHONY; synvoice_idx++)
            for (uint32_t i = 0; i < OPERATOR_COUNT; i++)
              s_egrateoffset[i] = paramOffset(s_egrate_offset, synvoice_idx, i);
          break;
        case param_kls_offset_carriers:
        case param_kls_offset_modulators:
          s_kls_offset[param_kls_offset_modulators - id + 6] = value * .25f;
          for (uint32_t synvoice_idx = 0; synvoice_idx < POLYPHONY; synvoice_idx++)
            for (uint32_t i = 0; i < OPERATOR_COUNT; i++)
              s_klsoffset[i] = paramOffset(s_kls_offset, synvoice_idx, i);
          break;
        case param_krs_offset_carriers:
        case param_krs_offset_modulators:
          s_krs_offset[param_krs_offset_modulators - id + 6] = value * .0625f;
          for (uint32_t synvoice_idx = 0; synvoice_idx < POLYPHONY; synvoice_idx++)
            for (uint32_t i = 0; i < OPERATOR_COUNT; i++)
              s_krsoffset[i] = paramOffset(s_krs_offset, synvoice_idx, i);
          break;
        case param_kvs_offset_carriers:
        case param_kvs_offset_modulators:
          s_kvs_offset[param_kvs_offset_modulators - id + 6] = value * .0625f;
          for (uint32_t synvoice_idx = 0; synvoice_idx < POLYPHONY; synvoice_idx++)
            setKvsLevel(synvoice_idx);
          break;
        case param_detune_offset_carriers:
        case param_detune_offset_modulators:
          s_detune_offset[param_detune_offset_modulators - id + 6] = value * 2.56f;
          break;
      default:
        break;
    }
}

__unit_callback int32_t unit_get_param_value(uint8_t id) {
    return sParams[id];
}

__unit_callback const char * unit_get_param_str_value(uint8_t id, int32_t value) {
    static char name[VOICE_NAME_SIZE + 1] = {0};
    static const char *vnam, *wnam;
    static const char *allowed = " ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!?#$%&'()*+,-.:;<=>@";
    static const char *pitchclass = "0123456789te";
    static char chord[10] = {0};
    int32_t chordnotes[MAX_CHORD - 1];
    const dx_voice_t *dxvoice_ptr;
    size_t len;
    switch (id) {
      case param_dxvoice_idx:
        if ((dxvoice_ptr = getDxVoice(value)) == nullptr)
          break;;
        vnam = dxvoice_ptr->dx7.vnam;
        if (*vnam == 0)
          vnam = &dxvoice_ptr->dx11.vnam[0];
        memcpy(name, vnam, VOICE_NAME_SIZE);
        len = strlen(name);
        for (size_t l = strspn(name, allowed); l < len; name[l] = '.', l = strspn(name, allowed)); //replace non-allowed characters
//        for (char *p = strrchr(name, ' '); p == &name[len - 1]; *p = 0, p = strrchr(name, ' '), len--); //remove trailing spaces
//        for (char *p = strstr(name, "  "); p != nullptr; strcpy(p, p + 1), p = strstr(name, "  ")); //squash multiple spaces
        len = strlen(name);
        for (uint32_t i = 1; i < len; i++) { //capitalize
          if (strchr(" -.", name[i - 1]) == nullptr) 
            name[i] = tolower(name[i]);
        }
        for (char *p = strstr(name, " "); p != nullptr; strcpy(p, p + 1), p = strstr(name, " ")); //remove spaces
        return name;
      case param_mode:
        if (value < mode_chord)
          return (const char*[]){"Poly", "Duo", "Unison"}[value];
        if (value < (int32_t)(mode_chord + sizeof(sChords)/sizeof(sChords[0])))
          return sChords[value - mode_chord].name;
        getChordNotes(chordnotes, value - mode_chord - sizeof(sChords)/sizeof(sChords[0]) + 1);
        if (chordnotes[1] == 0) {
          sprintf(chord, "{0,%c}", pitchclass[chordnotes[2]]);
        } else if (chordnotes[0] == 0) {
          sprintf(chord, "{0,%c,%c}", pitchclass[chordnotes[1]], pitchclass[chordnotes[2] % 12]);
        } else {
          sprintf(chord, "{0,%c,%c,%c}", pitchclass[chordnotes[0]], pitchclass[chordnotes[1] % 12], pitchclass[chordnotes[2] % 12]);
        }        
        return chord;
      case param_algorithm:
        if (value == 0)
          return "Retain";
        if (value <= 32)
          sprintf(name, "DX7.%d", value);
        else if (value <= 40)
          sprintf(name, "opsix.%d", value);
        else
          sprintf(name, "SY77.%d", value - 40);
        return name;
      case param_waveform_carriers:
      case param_waveform_modulators:
        if (value == 0)
          return "Retain";
        if ((wnam = Wavetables.getName(value - 1)) != nullptr)
          return wnam;
        break;
      case param_feedback1_route:
      case param_feedback2_route:
        if (value == 0)
          return "Retain";
        sprintf(name, "Op%d>Op%d", (value - 1) / 6 + 1, (value - 1) % 6 + 1);
        return name;
      default:
        break;
    }
    sprintf(name, "%d", value);
    return name;
}

__unit_callback const uint8_t * unit_get_param_bmp_value(uint8_t id, int32_t value) {
    (void)id;
    (void)value;
    return nullptr;
}

__unit_callback void unit_set_tempo(uint32_t tempo) {
    __attribute__((used)) static float ftempo = uq16_16_to_f32(tempo);
}

__unit_callback void unit_note_on(uint8_t note, uint8_t velocity) {
  noteOn(note, velocity);
}

__unit_callback void unit_note_off(uint8_t note) {
  noteOff(note);
}

__unit_callback void unit_gate_on(uint8_t velocity) {
  noteOn(gate_note = sParams[param_gate_note], velocity);
}

__unit_callback void unit_gate_off() {
  noteOff(gate_note);
}

__unit_callback void unit_all_note_off() {}

__unit_callback void unit_pitch_bend(uint16_t bend) {
  sPitchBend = ((int32_t)bend - PITCH_BEND_CENTER) * PITCH_BEND_SENSITIVITY;
//  sNotePhaseIncrement = (float)UINT_MAX * fastpow2((sNote + sPitchBend + NOTE_FREQ_OFFSET) * OCTAVE_RECIP);
}

__unit_callback void unit_channel_pressure(uint8_t pressure) {
  (void)pressure;
}

__unit_callback void unit_aftertouch(uint8_t note, uint8_t aftertouch) {
  (void)note;
  (void)aftertouch;
}

__unit_callback void unit_load_preset(uint8_t idx) {
  (void)idx;
}

__unit_callback uint8_t unit_get_preset_index() {
  return 0;
}

__unit_callback const char * unit_get_preset_name(uint8_t idx) {
  (void)idx;
  return nullptr;
}
