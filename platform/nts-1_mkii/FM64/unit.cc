/*
 *  File: unit.cc
 *
 *  FM64 Oscillator unit.
 *
 *
 *  2020-2025 (c) Oleg Burdaev
 *  mailto: dukesrg@gmail.com
 */

#include "logue_wrap.h"

#include <cstddef>
#include <cstdint>

#include <arm_neon.h>

#include <ctype.h>
#include <limits.h>

#if defined(UNIT_TARGET_PLATFORM_DRUMLOGUE) || defined(UNIT_TARGET_PLATFORM_MICROKORG2)
#include <string.h>
#endif

//#include "cv_data.h"

#define POLYPHONY 1
#define MAX_CHORD 1

#define PITCH_BEND_CENTER 8192
//#define PITCH_BEND_SENSITIVITY .0001220703125f  // 24/8192
#define PITCH_BEND_SENSITIVITY 45056 // 2^24 * 22/8192 NTS-1 mkII have native runtime context PB sensitivity of 2 semitones.

//#define OP6 //6-operator support
//#define OP4 //4-operator support
//#define WF32 //all 8 DX11 waveforms from PCM32 wavebank
//#define WF16 //all 8 DX11 waveforms from PCM16 wavebank
//#define WF8 //all 8 DX11 waveforms runtime generated from half-sine
//#define WF4 //4 first DX11 waveforms runtime generated from half-sine
//#define WF2 //2 first DX11 waveforms runtime generated from half-sine
//#define WFROM //logue SDK wave banks A-F
//#define WFGEN //generated waveforms
//#define OPSIX //enable KORG Opsix extensions
//#define SY77 //enable SY77 extensions
#define TWEAK_ALG //use reserved bits for extended algorithms count support
#define TWEAK_WF //use reserved bits for extended waveforms count support

//#define SHAPE_LFO //map Shape LFO to parameters (~28-40 bytes)
//#define PEG //pitch EG enable (~530-600 bytes)
//#define PEG_RATE_LUT //PEG Rate from LUT close to DX7, instead of approximated function (~44-176 bytes)
#define FINE_TUNE //16-bit precision for cents/detune
//#define KIT_MODE //key tracking to voice (- ~112 bytes)
#define SPLIT_ZONES 3
//#define MOD16 //16-bit mod matrix processing
//#define ROLLOUT //Inner loop rollout

#ifdef MOD16
  #define FEEDBACK_COUNT 2 //second feedback is mandatory and 'free' for 16-bit mod matrix
#endif
#ifndef FEEDBACK_COUNT
  #define FEEDBACK_COUNT 1
#endif

#define FAST_POWF //native logue-sdk pow2f must be fixed, lost precesion (- ~6K bytes)
//#define FASTER_POWF //not very precise (- ~6.3K bytes)
#if defined(FAST_POWF)
  #define POWF(a,b) fastpowf(a,b)
  #define POW2F(a) fastpow2f(a)
#elif defined(FASTER_POWF)
  #define POWF(a,b) fasterpowf(a,b)
  #define POW2F(a) fasterpow2f(a)
#else
  #define POWF(a,b) powf(a,b)
  #define POW2F(a) powf(2.f,a)
#endif

#include "fm64.h"

#ifdef WAVE_PINCH
#define CUSTOM_PARAM_COUNT 152
#elif defined (WFBITS)
#define CUSTOM_PARAM_COUNT 143
#else
#define CUSTOM_PARAM_COUNT 130
#endif

#include "custom_param.h"
CUSTOM_PARAM_INIT(
#if defined(UNIT_TARGET_PLATFORM_NTS1_MKII)
    CUSTOM_PARAM_ID(1),
    CUSTOM_PARAM_ID(17),
#ifdef KIT_MODE
    CUSTOM_PARAM_ID(7),
    CUSTOM_PARAM_ID(10),
    CUSTOM_PARAM_ID(5),
    CUSTOM_PARAM_ID(8),
    CUSTOM_PARAM_ID(11),
    CUSTOM_PARAM_ID(6),
    CUSTOM_PARAM_ID(8),
    CUSTOM_PARAM_ID(12)
#else
    CUSTOM_PARAM_ID(2),
    CUSTOM_PARAM_ID(122),
    CUSTOM_PARAM_ID(77),
    CUSTOM_PARAM_ID(24),
    CUSTOM_PARAM_ID(22),
#ifdef WFGEN
    CUSTOM_PARAM_ID(136),
#else
    CUSTOM_PARAM_ID(25),
#endif
#ifdef WAVE_PINCH
    CUSTOM_PARAM_ID(145),
    CUSTOM_PARAM_ID(146)
#else
    CUSTOM_PARAM_ID(24),
    CUSTOM_PARAM_ID(7)
#endif
#endif
#elif defined(UNIT_TARGET_PLATFORM_NTS3_KAOSS)
    CUSTOM_PARAM_ID(1),
    CUSTOM_PARAM_ID(158),
    CUSTOM_PARAM_ID(2),
    CUSTOM_PARAM_ID(156),
    CUSTOM_PARAM_ID(157),
    CUSTOM_PARAM_ID(7),
    CUSTOM_PARAM_ID(13),
#elif defined(UNIT_TARGET_PLATFORM_MICROKORG2)
    CUSTOM_PARAM_ID(1),
    CUSTOM_PARAM_ID(17),
#ifdef KIT_MODE
    CUSTOM_PARAM_ID(7),
    CUSTOM_PARAM_ID(10),
    CUSTOM_PARAM_ID(5),
    CUSTOM_PARAM_ID(8),
    CUSTOM_PARAM_ID(11),
    CUSTOM_PARAM_ID(6),
    CUSTOM_PARAM_ID(8),
    CUSTOM_PARAM_ID(12)
#else
    CUSTOM_PARAM_ID(2),
    CUSTOM_PARAM_ID(122),
    CUSTOM_PARAM_ID(77),
    CUSTOM_PARAM_ID(24),
    CUSTOM_PARAM_ID(22),
#ifdef WFGEN
    CUSTOM_PARAM_ID(136),
#else
    CUSTOM_PARAM_ID(25),
#endif
#ifdef WAVE_PINCH
    CUSTOM_PARAM_ID(145),
    CUSTOM_PARAM_ID(146)
#else
    CUSTOM_PARAM_ID(24),
    CUSTOM_PARAM_ID(7)
#endif
#endif
#endif
);

#if defined(WFGEN)
  #define FORMAT_PCM16
  #define SAMPLE_COUNT 256
  #define SAMPLE_GUARD
  #define WAVEBANK_NO_HOOKS
  #define WAVE_COUNT OPERATOR_COUNT
  #define WAVE_COUNT_X OPERATOR_COUNT
  #define WAVE_COUNT_Y 1
  #include "wavebank.h"
  #define WFBITS 7
#elif defined(WFROM)
  #define WAVE_COUNT k_waves_all_cnt
  #define WFBITS 7
#elif defined(WF16x2)
  #include "waveforms16x2.h"
  #define WFBITS 4
#elif defined(WF32)
  #include "waveforms32.h"
  #define WFBITS 3
#elif defined(WF16)
  #include "waveforms16.h"
  #define WFBITS 3
#elif defined(WF8)
  #define OSC_SIN_Q15_LUT
  #include "waveforms.h"
  #define WAVE_COUNT 8
  #define WFBITS 3
#elif defined(WF4)
  #define OSC_SIN_Q15_LUT
  #include "waveforms.h"
  #define WAVE_COUNT 4
  #define WFBITS 2
#elif defined(WF2)
  #define OSC_SIN_Q15_LUT
  #include "waveforms.h"
  #define WAVE_COUNT 2
  #define WFBITS 1
#endif

#ifdef WFGEN
  #define WAVEFORM_COUNT 126
#else
  #define WAVEFORM_COUNT WAVE_COUNT
#endif

#ifdef WAVE_PINCH
  #define WAVE_PINCH_PARAMS , s_wavewidth[i * 2], s_wavewidth[i * 2 + 1]
#else
  #define WAVE_PINCH_PARAMS
#endif

#ifdef WFBITS
#if defined(WFROM)
  #define OSC_FUNC(a) osc_wave_scanf(s_waveform[i], a)
#elif defined(WFGEN)
  #define OSC_FUNC(a) osc_wavebank(a, i WAVE_PINCH_PARAMS)
#else
  #define OSC_FUNC(a) osc_wavebank(a, s_waveform[i] WAVE_PINCH_PARAMS)
#endif
#else
  #define OSC_FUNC(a) osc_sinq(a WAVE_PINCH_PARAMS)
  #if defined(WFSIN32)
    #define OSC_SIN_Q31_LUT //use pre-calculated Q31 LUT instead of converted from firmware float, saves ~96 bytes of code
  #elif defined(WFSIN16)
    #define OSC_SIN_Q15_LUT //use pre-calculated Q31 LUT instead of converted from firmware float, saves ~96 bytes of code
  #endif
  #define OSC_SIN_Q
#endif
#include "osc_apiq.h"

//  #define EGLUT //use precalculated EG LUT, saves ~140 bytes of code
#define USE_Q31
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
  #define param_eglut(a,b) (ldrsh_lsl((int32_t)eg_lut, usat_asr(31, q31add(a,b), (EG_LUT_SHR + 1)), 1) << 16)
#elif defined(EGLUTX16)
  #define param_eglut(a,b) (ldrh_lsl((int32_t)eg_lut, usat_asr(31, q31add(a,b), (EG_LUT_SHR + 1)), 1) << 15)
#else
  #define param_eglut(a,b) (ldr_lsl((int32_t)eg_lut, usat_asr(31, q31add(a,b), (EG_LUT_SHR + 1)), 2))
#endif

#ifdef USE_Q31_PITCH
  typedef q31_t pitch_t;
  #define f32_to_pitch(a) f32_to_q31(a)
  #define pitch_to_phase(a) (a)
  #define pitch_mul(a,b) q31mul(a,b)
#else
  typedef float_t pitch_t;
  #define f32_to_pitch(a) (a)
  #define pitch_to_phase(a) f32_to_q31(a)
  #define pitch_mul(a,b) ((a)*(b))
#endif
#define FEEDBACK_RECIP 0x00FFFFFF // <1/128 - pre-multiplied by 2 for simplified Q31 multiply by always positive
#define FEEDBACK_RECIPF .00390625f // 1/256 - pre-multiplied by 2 for simplified Q31 multiply by always positive
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
#define DX11_TO_DX7_LEVEL_SCALE_FACTOR 6.6f //99/15
#define DX11_MAX_LEVEL 15

//#define FREQ_FACTOR .08860606f // (9.772 - 1)/99
#define PEG_SCALE 0x00600000 // 48/128 * 256 * 65536
#define PEG_RATE_SCALE 196.38618f; // ~ 192 >> 24 semitones per sample at 49096.545

#ifdef MOD16
#ifdef CUSTOM_ALGORITHM_COUNT
#define CUSTOM_MI_SCALE_FACTOR 0x139CA3E //normalized for 0-100 range of opsix
//#define CUSTOM_OUT_SCALE_FACTOR 0x147AE14 //^
#endif
#define MI_SCALE_FACTOR 0x7A93
static const uint8_t *s_algorithm;
static q15_t s_opval[OPERATOR_COUNT + FEEDBACK_COUNT * 2];
static q15_t s_modmatrix[OPERATOR_COUNT][OPERATOR_COUNT + FEEDBACK_COUNT];
static q15_t s_comp[OPERATOR_COUNT];
static q15_t compensation[] = {
  0x7FFF,
  0x3FFF,
  0x2AAA,
  0x1FFF,
  0x1999,
  0x1555
};
#else
#define MI_SCALE_FACTOR 0x7A92BE8B // 3.830413123f >> 2
static uint8_t s_algorithm[OPERATOR_COUNT] = {0};
static q31_t s_opval[OPERATOR_COUNT + FEEDBACK_COUNT * 2];
static q31_t s_comp[OPERATOR_COUNT];
static q31_t compensation[] = {
  0x7FFFFFFF,
  0x3FFFFFFF,
  0x2AAAAAAA,
  0x1FFFFFFF,
  0x19999999,
  0x15555555
};
#endif

static uint8_t s_algorithm_idx;
static int8_t s_algorithm_offset = 0;
static uint8_t s_algorithm_select = 0;
#define FINE_TUNE_FACTOR 65536.f
static uint8_t s_split_point[SPLIT_ZONES - 1] = {0};
static int8_t s_zone_transpose[SPLIT_ZONES] = {0};
static int8_t s_zone_voice_shift[SPLIT_ZONES] = {0};
static int8_t s_zone_transposed = 0;
#ifndef KIT_MODE
//static uint8_t s_kit_voice = 0;
static int8_t s_voice[SPLIT_ZONES] = {0};
#endif
static int16_t s_level_offset[OPERATOR_COUNT + 3] = {0};
static int16_t s_kls_offset[OPERATOR_COUNT + 3] = {0};
static int16_t s_kvs_offset[OPERATOR_COUNT + 3] = {0};
static int16_t s_egrate_offset[OPERATOR_COUNT + 3] = {0};
static int16_t s_krs_offset[OPERATOR_COUNT + 3] = {0};
static int16_t s_detune_offset[OPERATOR_COUNT + 3] = {0};
#ifdef WAVE_PINCH
static int8_t s_waveform_pinch[OPERATOR_COUNT + 3] = {0};
#endif
#ifdef OP6
static int16_t s_level_scale[OPERATOR_COUNT + 3] = {512, 512, 512, 512, 512, 512, 512, 512, 512};
static int16_t s_kls_scale[OPERATOR_COUNT + 3] = {512, 512, 512, 512, 512, 512, 512, 512, 512};
static int16_t s_kvs_scale[OPERATOR_COUNT + 3] = {512, 512, 512, 512, 512, 512, 512, 512, 512};
static int16_t s_egrate_scale[OPERATOR_COUNT + 3] = {512, 512, 512, 512, 512, 512, 512, 512, 512};
static int16_t s_krs_scale[OPERATOR_COUNT + 3] = {512, 512, 512, 512, 512, 512, 512, 512, 512};
static int16_t s_detune_scale[OPERATOR_COUNT + 3] = {512, 512, 512, 512, 512, 512, 512, 512, 512};
#else
static int16_t s_level_scale[OPERATOR_COUNT + 3] = {512, 512, 512, 512, 512, 512, 512};
static int16_t s_kls_scale[OPERATOR_COUNT + 3] = {512, 512, 512, 512, 512, 512, 512};
static int16_t s_kvs_scale[OPERATOR_COUNT + 3] = {512, 512, 512, 512, 512, 512, 512};
static int16_t s_egrate_scale[OPERATOR_COUNT + 3] = {512, 512, 512, 512, 512, 512, 512};
static int16_t s_krs_scale[OPERATOR_COUNT + 3] = {512, 512, 512, 512, 512, 512, 512};
static int16_t s_detune_scale[OPERATOR_COUNT + 3] = {512, 512, 512, 512, 512, 512, 512};
#endif
static float s_feedback_offset[FEEDBACK_COUNT] = {0.f};
#if FEEDBACK_COUNT == 2
static float s_feedback_scale[FEEDBACK_COUNT] = {1.f, 1.f};
#else
static float s_feedback_scale[FEEDBACK_COUNT] = {1.f};
#endif
static uint8_t s_feedback_route[FEEDBACK_COUNT] = {0};
static uint8_t s_feedback_level[FEEDBACK_COUNT] = {0};
#ifdef WFBITS
#ifdef OP6
static int16_t s_waveform_offset[OPERATOR_COUNT + 3 + 4] = {0};
#else
static int16_t s_waveform_offset[OPERATOR_COUNT + 3 + 3] = {0};
#endif
#endif
static int8_t s_left_depth[OPERATOR_COUNT];
static int8_t s_right_depth[OPERATOR_COUNT];
static uint8_t s_pitchfreq[OPERATOR_COUNT];
static uint8_t s_egstage[OPERATOR_COUNT];
static uint8_t s_kvs[OPERATOR_COUNT];
static uint8_t s_break_point[OPERATOR_COUNT];
static uint8_t s_left_curve[OPERATOR_COUNT];
static uint8_t s_right_curve[OPERATOR_COUNT];
static uint8_t s_opi;
static int8_t s_detune[OPERATOR_COUNT];
static int8_t s_transpose;
static uint32_t s_sample_num;
static uint32_t s_sample_count[OPERATOR_COUNT][EG_STAGE_COUNT * 2];

static q31_t s_velocity = 0;
static int16_t s_velocity_raw = 1023;

static int8_t s_op_level[OPERATOR_COUNT];
static float s_op_rate_scale[OPERATOR_COUNT];
#ifdef WFBITS
static uint8_t s_op_waveform[OPERATOR_COUNT];
#ifdef WFROM
static const float * s_waveform[OPERATOR_COUNT];
#else
static uint32_t s_waveform[OPERATOR_COUNT];
#ifdef WFGEN
#ifdef OP6
static uint32_t s_waveform_current[OPERATOR_COUNT] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
#else
static uint32_t s_waveform_current[OPERATOR_COUNT] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
#endif
#endif
#endif
#endif
static uint8_t s_egrate[OPERATOR_COUNT][EG_STAGE_COUNT];
static q31_t s_egsrate[OPERATOR_COUNT][EG_STAGE_COUNT * 2];
static float s_egsrate_recip[OPERATOR_COUNT][2];
static q31_t s_eglevel[OPERATOR_COUNT][EG_STAGE_COUNT];
static q31_t s_egval[OPERATOR_COUNT];
static q31_t s_oplevel[OPERATOR_COUNT];
static q31_t s_outlevel[OPERATOR_COUNT];
#ifdef OP6
static float s_klslevel[OPERATOR_COUNT] = {LEVEL_SCALE_FACTOR_DB, LEVEL_SCALE_FACTOR_DB, LEVEL_SCALE_FACTOR_DB, LEVEL_SCALE_FACTOR_DB, LEVEL_SCALE_FACTOR_DB, LEVEL_SCALE_FACTOR_DB};
static float s_krslevel[OPERATOR_COUNT] = {RATE_SCALING_FACTOR, RATE_SCALING_FACTOR, RATE_SCALING_FACTOR, RATE_SCALING_FACTOR, RATE_SCALING_FACTOR, RATE_SCALING_FACTOR};
static float s_egratelevel[OPERATOR_COUNT] = {1.f, 1.f, 1.f, 1.f, 1.f, 1.f};
#ifdef WAVE_PINCH
static q31_t s_wavewidth[OPERATOR_COUNT * 2] = {0x7FFFFFFF, 0x01000000, 0x7FFFFFFF, 0x01000000, 0x7FFFFFFF, 0x01000000, 0x7FFFFFFF, 0x01000000, 0x7FFFFFFF, 0x01000000, 0x7FFFFFFF, 0x01000000};
#endif
#else
static float s_klslevel[OPERATOR_COUNT] = {LEVEL_SCALE_FACTOR_DB, LEVEL_SCALE_FACTOR_DB, LEVEL_SCALE_FACTOR_DB, LEVEL_SCALE_FACTOR_DB};
static float s_krslevel[OPERATOR_COUNT] = {RATE_SCALING_FACTOR, RATE_SCALING_FACTOR, RATE_SCALING_FACTOR, RATE_SCALING_FACTOR};
static float s_egratelevel[OPERATOR_COUNT] = {1.f, 1.f, 1.f, 1.f};
#ifdef WAVE_PINCH
static q31_t s_wavewidth[OPERATOR_COUNT * 2] = {0x7FFFFFFF, 0x01000000, 0x7FFFFFFF, 0x01000000, 0x7FFFFFFF, 0x01000000, 0x7FFFFFFF, 0x01000000};
#endif
#endif
static float s_klsoffset[OPERATOR_COUNT] = {0.f};
static float s_egrateoffset[OPERATOR_COUNT] = {0.f};
static float s_krsoffset[OPERATOR_COUNT] = {0.f};
static q31_t s_level_scaling[OPERATOR_COUNT];
static q31_t s_kvslevel[OPERATOR_COUNT];
static q31_t s_velocitylevel[OPERATOR_COUNT];

static float s_attack_rate_exp_factor;
static float s_release_rate_exp_factor;

static float s_level_scale_factor;

static q31_t s_feedback[FEEDBACK_COUNT];
static uint8_t s_feedback_src[FEEDBACK_COUNT];
static uint8_t s_feedback_src_alg[FEEDBACK_COUNT];
static uint8_t s_feedback_dst_alg[FEEDBACK_COUNT];

#ifdef PEG
static int32_t s_pegrate[PEG_STAGE_COUNT + 1];
static int32_t s_peglevel[PEG_STAGE_COUNT];
static uint32_t s_peg_sample_count[PEG_STAGE_COUNT];
static int32_t s_pegval;
static float s_pegrate_releaserecip;
static uint8_t s_pegstage;
static uint8_t s_peg_stage_start;
#endif 

static pitch_t s_oppitch[OPERATOR_COUNT];
static q31_t s_phase[OPERATOR_COUNT];
#ifndef UNIT_TARGET_PLATFORM_MICROKORG2
static int32_t s_pitchbend = 0;
#endif
#ifndef UNIT_OSC_H_
static uint8_t s_note;
#endif

#if UNIT_INPUT_CHANNELS != 0
enum {
  audio_mod_none = 0,
  audio_mod_amp = 1,
  audio_mod_ring = 2
};
static uint8_t s_audio_op_wave= OPERATOR_COUNT;
static uint8_t s_audio_op_phase = OPERATOR_COUNT;
static uint8_t s_audio_mod = audio_mod_none;
#endif

enum {
  state_running = 0,
  state_noteon = 1,
  state_noteoff = 2,
  state_wave_changed = 4
};
#ifdef WFGEN
static uint32_t s_state = state_wave_changed;
#else
static uint32_t s_state = 0;
#endif

static int32_t sParams[PARAM_COUNT];

#ifdef UNIT_TARGET_MODULE_OSC
#ifdef UNIT_OSC_H_
const unit_runtime_osc_context_t *runtime_context;
#endif
#elif defined(UNIT_TARGET_MODULE_GENERICFX)
const unit_runtime_genericfx_context_t *runtime_context;
#endif

#ifdef UNIT_TARGET_PLATFORM_NTS3_KAOSS
//ToDo: switch to gate_arp.h
#include "gate_arp.h"
#include "patterns.h"
enum {
  arp_stopped = 0,
  arp_gate_off = 2,
  arp_gate_on = 3,
};
static uint32_t s_arp;
static uint32_t s_arp_state;
static uint32_t s_arp_counter;
static const uint8_t *s_arp_ptr;
static uint32_t s_arp_num;
static uint32_t s_arp_idx;

static uint32_t s_arp_quantize;

static uint32_t s_arp_sample_counter;
static uint32_t s_arp_sample_counter_old;

//48000 * 60 / bpm * (PATTERN_QUANTIZE)

/*
PATTERN_QUANTIZE
#define k_samplerate        (48000)
#define k_samplerate_recipf (2.08333333333333e-005f)
*/
//static uint32_t s_tempo_old;
//static uint32_t s_tempo_new;

#define TOUCH_NOTE 60

#include "scales.h"
static const uint8_t *s_scale_ptr;
static uint32_t s_scale_num;
static uint32_t s_scale_steps = 12;
static int32_t  s_scale_pitch_note;
#endif

fast_inline const dx_voice_t * getDxVoice(uint32_t index) {
  dx_voice_t *dxvoice_ptr = &((dx_voice_t *)dx_voices)[index];
  if (*dxvoice_ptr->dx7.vnam != 0 || *dxvoice_ptr->dx11.vnam != 0)
    return dxvoice_ptr;
  return nullptr;
}
/*
fast_inline float paramScale(uint8_t *param, uint32_t opidx) {
  return .000001f * param[opidx] * param[((s_algorithm[opidx] & ALG_OUT_MASK) >> 7) + OPERATOR_COUNT] * param[OPERATOR_COUNT + 2];
}
*/
fast_inline float paramScale(int16_t *param, uint32_t opidx) {
  return .7450581e-8f * param[opidx] * param[((s_algorithm[opidx] & ALG_OUT_MASK) >> 7) + OPERATOR_COUNT] * param[OPERATOR_COUNT + 2];
}
/*
fast_inline int32_t paramOffset(int8_t *param, uint32_t opidx) {
  return param[opidx] + param[((s_algorithm[opidx] & ALG_OUT_MASK) >> 7) + OPERATOR_COUNT] + param[OPERATOR_COUNT + 2];
}
*/
fast_inline int32_t paramOffset(int16_t *param, uint32_t opidx) {
  return param[opidx] + param[((s_algorithm[opidx] & ALG_OUT_MASK) >> 7) + OPERATOR_COUNT] + param[OPERATOR_COUNT + 2];
}

fast_inline static void setOpLevel(uint32_t opidx) {
// make it non-negative and apply -96dB to further fit EG level
    s_oplevel[opidx] = q31sub((usat_lsl(31, q31add(s_level_scaling[opidx], s_velocitylevel[opidx]), 0)), 0x7F000000);
}

fast_inline void setOutLevel() {
  for (uint32_t i = 0; i < OPERATOR_COUNT; i++)
// saturate Out Level to 0dB offset of Q31
    s_outlevel[i] = q31add(f32_to_q31(scale_level(s_op_level[i] + paramOffset(s_level_offset, i) * .25f) * paramScale(s_level_scale, i) * LEVEL_SCALE_FACTORF), 0x00FFFFFF);
}

fast_inline void setKvsLevel() {
  for (uint32_t i = 0; i < OPERATOR_COUNT; i++)
    s_kvslevel[i] = f32_to_q31((s_kvs[i] + paramOffset(s_kvs_offset, i) * .015625f) * paramScale(s_kvs_scale, i) * .015625f);
}

fast_inline static void setVelocityLevel() {
  for (uint32_t i = 0; i < OPERATOR_COUNT; i++) {
// Velocity * KVS
    s_velocitylevel[i] = smmul(s_velocity, s_kvslevel[i]) << 7;
    setOpLevel(i);
  }
}

fast static void setVelocity(float velocity) {
  s_velocity = f32_to_q31((POWF(velocity, .27f) * 60.f - 208.f) * .0625f * LEVEL_SCALE_FACTOR_DB);
  setVelocityLevel(); 
}

/*
void setLevel() {
  for (uint32_t i = 0; i < OPERATOR_COUNT; i++) {
// saturate Out Level to 0dB offset of Q31
    s_oplevel[i] = q31add(f32_to_q31(scale_level(clipminmaxi32(0, s_op_level[i] + paramOffset(s_level_offset, i), 99)) * paramScale(s_level_scale, i) * LEVEL_SCALE_FACTORF), 0x00FFFFFF);
// saturate with KLS
    s_oplevel[i] = q31add(s_oplevel[i], s_level_scaling[i]);
// adjust 0dB level to fit positive Velocity * KVS and add them
    s_oplevel[i] = q31add(q31sub(s_oplevel[i], 0x07000000), f32_to_q31(s_velocity * clipminmaxf(0.f, s_kvs[i] + paramOffset(s_kvs_offset, i) * 0.07f, 7.f) * paramScale(s_kvs_scale, i)));
// make it non-negative and apply -96dB to further fit EG level
    s_oplevel[i] = q31sub((usat_lsl(31, s_oplevel[i], 0)), 0x7F000000);
  }
}
*/
#ifdef WFBITS
fast_inline void setWaveform() {
  for (uint32_t i = 0; i < OPERATOR_COUNT; i++) {
#ifdef WFROM
    s_waveform[i] = wavesAll[clipminmaxi32(
      0, s_op_waveform[i] +
      paramOffset(s_waveform_offset, i) +
      (i & 0x01 ? (s_waveform_offset[OPERATOR_COUNT + 3 + (i >> 1)] / 10) : (s_waveform_offset[OPERATOR_COUNT + 3 + (i >> 1)] % 10)) +
      ((s_algorithm[i] & ALG_OUT_MASK) ? (s_waveform_offset[sizeof(s_waveform_offset) / sizeof(*s_waveform_offset) - 1] / 10) : (s_waveform_offset[sizeof(s_waveform_offset) / sizeof(*s_waveform_offset) - 1] % 10)),
      WAVEFORM_COUNT - 1
    )];
#else
    s_waveform[i] = clipminmaxi32(
      0, s_op_waveform[i] +
      paramOffset(s_waveform_offset, i) +
      (i & 0x01 ? (s_waveform_offset[OPERATOR_COUNT + 3 + (i >> 1)] / 10) : (s_waveform_offset[OPERATOR_COUNT + 3 + (i >> 1)] % 10)) +
      ((s_algorithm[i] & ALG_OUT_MASK) ? (s_waveform_offset[sizeof(s_waveform_offset) / sizeof(*s_waveform_offset) - 1] / 10) : (s_waveform_offset[sizeof(s_waveform_offset) / sizeof(*s_waveform_offset) - 1] % 10)),
      WAVEFORM_COUNT - 1
    );
#ifdef WFGEN
    if (s_waveform[i] != s_waveform_current[i])
      s_state |= state_wave_changed;
#endif
#endif
  }
}
#endif

fast_inline void setFeedback(uint32_t idx) {
  float value = clipmaxf(s_feedback_level[idx] + s_feedback_offset[idx], 7.f);
  s_feedback[idx] = value <= 0.f ? 0 : f32_to_q31(POW2F(value * s_feedback_scale[idx]) * FEEDBACK_RECIPF);
}

fast_inline void setFeedbackRoute(uint32_t idx) {
  uint32_t dst;
  if (s_feedback_route[idx] == 0) {
    s_feedback_src[idx] = s_feedback_src_alg[idx];
    dst = s_feedback_dst_alg[idx];
  } else {
    dst = s_feedback_route[idx] / OPERATOR_COUNT;
    s_feedback_src[idx] = OPERATOR_COUNT - clipminmaxi32(1, dst, OPERATOR_COUNT);
    dst = OPERATOR_COUNT - clipminmaxi32(1, s_feedback_route[idx] - dst * OPERATOR_COUNT, OPERATOR_COUNT);
  }
  for (uint32_t i = 0; i < OPERATOR_COUNT; i++) {
#ifdef MOD16
    if (i == dst)
      s_modmatrix[i][OPERATOR_COUNT + idx] = MI_SCALE_FACTOR;
    else
      s_modmatrix[i][OPERATOR_COUNT + idx] = 0;
#else
    if (i == dst)
      s_algorithm[i] |= ALG_FBK_MASK << idx;
    else
      s_algorithm[i] &= ~(ALG_FBK_MASK << idx);
#endif
  }
}

fast_inline void setAlgorithm() {
  int32_t comp = 0;
#ifdef MOD16
#ifdef CUSTOM_ALGORITHM_COUNT
  uint32_t algidx = (s_algorithm_select == 0 ? s_algorithm_idx : s_algorithm_select - 1) + s_algorithm_offset;
  if (algidx < ALGORITHM_COUNT)
    s_algorithm = dx7_algorithm[clipminmaxi32(0, algidx, ALGORITHM_COUNT + CUSTOM_ALGORITHM_COUNT - 1)];
#else
  s_algorithm = dx7_algorithm[clipminmaxi32(0, (s_algorithm_select == 0 ? s_algorithm_idx : s_algorithm_select - 1) + s_algorithm_offset, ALGORITHM_COUNT - 1)];
#endif
#endif
  s_feedback_dst_alg[0] = OPERATOR_COUNT;
#if FEEDBACK_COUNT == 2
  s_feedback_dst_alg[1] = OPERATOR_COUNT;
#endif
  for (uint32_t i = 0; i < OPERATOR_COUNT; i++) {
#ifndef MOD16
    s_algorithm[i] = dx7_algorithm[clipminmaxi32(0, (s_algorithm_select == 0 ? s_algorithm_idx : s_algorithm_select - 1) + s_algorithm_offset, ALGORITHM_COUNT - 1)][i];
#endif
#ifdef CUSTOM_ALGORITHM_COUNT
      if (algidx < ALGORITHM_COUNT) {
#endif
    for (uint32_t fbidx = 0; fbidx < FEEDBACK_COUNT; fbidx++) {
      if (s_algorithm[i] & (ALG_FBK_MASK << fbidx)) {
        s_feedback_src_alg[fbidx] = s_algorithm[i] & (ALG_FBK_MASK - 1);
        s_feedback_dst_alg[fbidx] = i;
#ifndef MOD16
        s_algorithm[i] &= ~(ALG_FBK_MASK - 1);
#endif
      }
    }
    if (s_algorithm[i] & ALG_OUT_MASK)
      comp++;
#ifdef CUSTOM_ALGORITHM_COUNT
      } else {
         comp += custom_algorithm[algidx - ALGORITHM_COUNT][i][OPERATOR_COUNT];
      }
#endif

#ifdef MOD16
    for (uint32_t j = 0; j < OPERATOR_COUNT; j++) {
#ifdef CUSTOM_ALGORITHM_COUNT
      if (algidx < ALGORITHM_COUNT) {
#endif
      if ((j < (OPERATOR_COUNT - 1)) && (s_algorithm[i] & (1 << j)) && !(s_algorithm[i] & (ALG_FBK_MASK + ALG_FBK2_MASK)))
        s_modmatrix[i][j] = MI_SCALE_FACTOR;
      else
        s_modmatrix[i][j] = 0;
#ifdef CUSTOM_ALGORITHM_COUNT
    } else {
      s_modmatrix[i][j] = (custom_algorithm[algidx - ALGORITHM_COUNT][i][j] * CUSTOM_MI_SCALE_FACTOR) >> 16;
    }
#endif
    }
#endif
  }
  setFeedbackRoute(0);
#if FEEDBACK_COUNT == 2
  setFeedbackRoute(1);
#endif
#ifdef CUSTOM_ALGORITHM_COUNT
  if (algidx >= ALGORITHM_COUNT)
    comp = 0x7FFFFFFF / comp;
#endif
  for (uint32_t i = 0; i < OPERATOR_COUNT; i++) {
#ifdef CUSTOM_ALGORITHM_COUNT
    if (algidx < ALGORITHM_COUNT) {
#endif
    if (s_algorithm[i] & ALG_OUT_MASK)
      s_comp[i] = compensation[comp - 1];
    else
      s_comp[i] = 0;
#ifdef CUSTOM_ALGORITHM_COUNT
    } else {
//      s_comp[i] = (custom_algorithm[algidx - ALGORITHM_COUNT][i][OPERATOR_COUNT] * CUSTOM_OUT_SCALE_FACTOR) >> 16;
      s_comp[i] = (custom_algorithm[algidx - ALGORITHM_COUNT][i][OPERATOR_COUNT] * comp) >> 16;
    }
#endif
  }
#ifdef WFBITS
  setWaveform();
#endif
}

fast_inline static void initvoice(int32_t voice_index) {
  if (dx_voices[0][voice_index].dx7.vnam[0]) {
#ifdef OP6
    const dx7_voice_t *voice = &dx_voices[0][voice_index].dx7;
    s_opi = voice->opi;
    s_algorithm_idx = voice->als;
    s_transpose = voice->trnp - TRANSPOSE_CENTER;

    s_feedback_level[0] = voice->fbl;
#ifdef PEG
    s_peg_stage_start = PEG_STAGE_COUNT - DX7_PEG_STAGE_COUNT;
    for (uint32_t i = s_peg_stage_start; i < PEG_STAGE_COUNT; i++) {
      s_peglevel[i] = scale_pitch_level(voice->pl[i - s_peg_stage_start]) * PEG_SCALE;
      s_pegrate[i] = scale_pitch_rate(voice->pr[i - s_peg_stage_start]) * PEG_RATE_SCALE;
    }
#endif
    for (uint32_t i = 0; i < OPERATOR_COUNT; i++) {
      s_pitchfreq[i] = !voice->op[i].pm;
      s_detune[i] = (voice->op[i].pd - DX7_DETUNE_CENTER) * 3;
#ifdef WFBITS
#ifdef TWEAK_WF
      s_op_waveform[i] = voice->op[i].osw & ((1 << WFBITS) - 1);
#else
      s_op_waveform[i] = 0;
#endif
#endif

      s_phase[i] = 0;

//todo: check dx7 D1/D2/R rates
      for (uint32_t j = 0; j < EG_STAGE_COUNT; j++) {
        s_egrate[i][j] = voice->op[i].r[j];
        s_eglevel[i][j] = scale_level(voice->op[i].l[j]) * LEVEL_SCALE_FACTOR;
      }

      if (s_pitchfreq[i])
        s_oppitch[i] = f32_to_pitch(((voice->op[i].pc == 0 ? .5f : voice->op[i].pc) * (1.f + voice->op[i].pf * .01f)));
      else
        s_oppitch[i] = f32_to_pitch(fastexpf(M_LN10 * ((voice->op[i].pc & 3) + voice->op[i].pf * .01f)) * k_samplerate_recipf);

      s_kvs[i] = voice->op[i].ts;
      s_op_rate_scale[i] = voice->op[i].rs * DX7_RATE_SCALING_FACTOR;
      s_op_level[i] = voice->op[i].tl;
      s_break_point[i] = voice->op[i].bp + NOTE_A_1;
//fold negative/position curves into curve depth sign
      s_left_depth[i] = voice->op[i].ld;
      s_right_depth[i] = voice->op[i].rd;
      if (voice->op[i].lc < 2) {
        s_left_curve[i] = voice->op[i].lc;
      } else {
        s_left_curve[i] = 5 - voice->op[i].lc;
      }
      if (voice->op[i].rc < 2) {
        s_right_curve[i] = voice->op[i].rc;
      } else {
        s_right_curve[i] = 5 - voice->op[i].rc;
      }
    }
    s_attack_rate_exp_factor = DX7_RATE_EXP_FACTOR;
    s_release_rate_exp_factor = DX7_RATE_EXP_FACTOR;
    s_level_scale_factor = DX7_LEVEL_SCALE_FACTOR;
#endif
  } else {
#ifdef OP4
    const dx11_voice_t *voice = &dx_voices[0][voice_index].dx11;
#ifdef TWEAK_ALG
    s_algorithm_idx = dx11_algorithm_lut[voice->alg + (voice->alghi << 3)];
#else
    s_algorithm_idx = dx11_algorithm_lut[voice->alg];
#endif
    s_opi = 0;
    s_transpose = voice->trps - TRANSPOSE_CENTER;
    s_feedback_level[0] = voice->fbl;
#ifdef PEG
    s_peg_stage_start = PEG_STAGE_COUNT - DX11_PEG_STAGE_COUNT;
    for (uint32_t i = s_peg_stage_start; i < PEG_STAGE_COUNT; i++) {
      s_peglevel[i] = scale_pitch_level(voice->pl[i - s_peg_stage_start]) * PEG_SCALE;
      s_pegrate[i] = scale_pitch_rate(voice->pr[i - s_peg_stage_start]) * PEG_RATE_SCALE;
    }
#endif
    for (uint32_t k = DX11_OPERATOR_COUNT; k--;) {
      uint32_t i;
      if (s_algorithm_idx == 7)
        i = dx11_alg3_op_lut[k];
      else
        i = k;

      s_pitchfreq[i] = !voice->opadd[i].fixrg;
      s_detune[i] = (voice->op[i].det - DX11_DETUNE_CENTER) * 3;
#ifdef WFBITS
      s_op_waveform[i] = voice->opadd[i].osw & ((1 << WFBITS) - 1);
#endif

      s_phase[i] = 0;

//todo: check dx11 rates
      for (uint32_t j = 0; j < EG_STAGE_COUNT; j++) {
        s_egrate[i][j] = j == (EG_STAGE_COUNT - 1) && voice->op[i].r[j] == 0 ? 1 : voice->op[i].r[j]; //zero release rate workaround from TX81Z
        s_eglevel[i][j] = f32_to_q31(1.f - (1.f - (j == 0 ? 1.f : (j == 1 || (j == 2 && voice->op[i].r[j] == 0)) ? scale_level(voice->op[i].d1l * DX11_TO_DX7_LEVEL_SCALE_FACTOR) * LEVEL_SCALE_FACTOR : 0.f)) / (1 << (i != 3 ? voice->opadd[i].egsft : 0)));
      }

//todo: Fine freq ratio
//https://github.com/mamedev/mame/blob/master/src/devices/sound/ym2151.cpp
//dt2_tab = { 0/768, 384/768, 500/768, 608/768 }
//pitch = 2 ^ (key/12 + LFO in + dt2_tab[op dt2]) * op ratio + keytracked op detune * op ratio
      if (s_pitchfreq[i])
        s_oppitch[i] = f32_to_pitch(dx11_ratio_lut[voice->op[i].f]);
//        s_oppitch[i] = log2f(dx11_ratio_lut[voice->op[i].f]) * 256.f * 12.f;
      else
        s_oppitch[i] = f32_to_pitch(((((voice->op[i].f & 0x3C) << 2) + voice->opadd[i].fine + (voice->op[i].f < 4 ? 8 : 0)) << voice->opadd[i].fixrg) * k_samplerate_recipf);
//        s_oppitch[i] = log2f(((((voice->op[i].f & 0x3C) << 2) + voice->opadd[i].fine + (voice->op[i].f < 4 ? 8 : 0)) << voice->opadd[i].fixrg)) * 256.f * 12.f;

      s_kvs[i] = voice->op[i].kvs;
      s_op_rate_scale[i] = voice->op[i].rs * DX11_RATE_SCALING_FACTOR;
      s_op_level[i] = voice->op[i].out;
      s_left_depth[i] = 0;
      s_right_depth[i] = - voice->op[i].ls;
      s_left_curve[i] = 0;
      s_right_curve[i] = 0;
      s_break_point[i] = NOTE_C1;
    }
    s_attack_rate_exp_factor = DX11_RATE_EXP_FACTOR;
    s_release_rate_exp_factor = DX11_RELEASE_RATE_EXP_FACTOR;
    s_level_scale_factor = DX11_LEVEL_SCALE_FACTOR;
#ifdef OP6
    s_op_level[4] = 0;
    s_op_level[5] = 0;
    s_kvs[4] = 0;
    s_kvs[5] = 0;
    for (uint32_t j = 0; j < EG_STAGE_COUNT; j++) {
      s_egrate[4][j] = 0;
      s_egrate[5][j] = 0;
      s_eglevel[4][j] = 0;
      s_eglevel[5][j] = 0;
    }
#endif
#endif
  }
  setAlgorithm();
  setOutLevel();
  setKvsLevel();
  setVelocityLevel();
  for (uint32_t i = 0; i < FEEDBACK_COUNT; i++)
    setFeedback(i);
  for (uint32_t i = 0; i < OPERATOR_COUNT + FEEDBACK_COUNT * 2; i++)
    s_opval[i] = 0;
  for (uint32_t i = 0; i < OPERATOR_COUNT; i++) {
    s_sample_count[i][EG_STAGE_COUNT - 1] = 0xFFFFFFFF;
    s_egsrate[i][EG_STAGE_COUNT - 1] = 0;
    s_egstage[i] = EG_STAGE_COUNT - 1;
    s_egval[i] = 0;
  }

#ifdef PEG
  uint32_t samples = 0;
  int32_t dl;
  for (uint32_t i = s_peg_stage_start; i < PEG_STAGE_COUNT - 1; i++) {
    dl = (s_peglevel[i] - s_peglevel[i != s_peg_stage_start ? i - 1 : PEG_STAGE_COUNT - 1]);
    if (dl < 0)
      s_pegrate[i] = -s_pegrate[i];
    samples += dl / s_pegrate[i];
    s_peg_sample_count[i] = samples;
  }
  s_pegrate[PEG_STAGE_COUNT] = s_pegrate[PEG_STAGE_COUNT - 1];
  s_pegrate_releaserecip = 1.f / s_pegrate[PEG_STAGE_COUNT];
  s_pegrate[PEG_STAGE_COUNT - 1] = 0;
  s_peg_sample_count[PEG_STAGE_COUNT - 1] = 0xFFFFFFFF;
  s_pegstage = PEG_STAGE_COUNT - 1;
  s_pegval = 0;
#endif
}

fast_inline q31_t calc_rate(uint32_t i, uint32_t j, float rate_factor, float rate_exp_factor, int32_t note) {
  if (j == 0)
    rate_factor *= DX7_RATE1_FACTOR;
  float rscale = (note - NOTE_A_1) * (s_op_rate_scale[i] + s_krsoffset[i]) * s_krslevel[i];
  float rate = (s_egrate[i][j] + s_egrateoffset[i]) * s_egratelevel[i];
  return f32_to_q31(rate_factor * POW2F(rate_exp_factor * (rate + rscale)));
}

//fast_inline void noteOn(uint8_t note, uint8_t velocity) {
static void noteOn(uint8_t note, uint8_t velocity) {
#ifndef UNIT_OSC_H_
  s_note = note;
#endif
  float rate_factor;
  int32_t dl, dp, curve = 0;
  float depth = 0.f;
  int32_t voice;
  uint32_t zone;
  for (zone = 0; zone < (SPLIT_ZONES - 1) && note < s_split_point[zone]; zone++);
  s_zone_transposed = s_zone_transpose[zone];
#ifndef KIT_MODE
  voice = s_voice[zone];
/*
  s_kit_voice = (voice == 0);
  if (voice > 0)
    voice--;
  if (s_kit_voice) {
#endif
*/
#else
    voice = note;
    note = KIT_CENTER;
//#ifndef KIT_MODE
//  }
#endif
  note += s_zone_transposed;
  voice += s_zone_voice_shift[zone];
  initvoice(voice);
  uint32_t samples;
  for (uint32_t i = 0; i < OPERATOR_COUNT; i++) {
    samples = 0;
    for (uint32_t j = 0; j < EG_STAGE_COUNT - 1; j++) {
      dl = s_eglevel[i][j] - s_eglevel[i][j ? (j - 1) : (EG_STAGE_COUNT - 1)];
      if (dl != 0) {
        if (dl < 0) {
          rate_factor = DX7_DECAY_RATE_FACTOR;
        } else {
          rate_factor = DX7_ATTACK_RATE_FACTOR;
        }
        s_egsrate[i][j + EG_STAGE_COUNT] = calc_rate(i, j, rate_factor, s_attack_rate_exp_factor, note);
        samples += dl / s_egsrate[i][j + EG_STAGE_COUNT];
      } else {
        s_egsrate[i][j + EG_STAGE_COUNT] = 0;
      }
      s_sample_count[i][j + EG_STAGE_COUNT] = samples;
    }
    dp = note - s_break_point[i];
//    depth = paramOffset(s_kls_offset, i);
    depth = s_klsoffset[i];
    if (dp < 0) {
       depth += s_left_depth[i];
       curve = s_left_curve[i];
       dp = - dp;
    } else if (dp > 0) {
       depth += s_right_depth[i];
       curve = s_right_curve[i];
    }
    if (curve < 2)
      depth = - depth;
// saturate Out level with KLS and adjust 0dB level to fit positive Velocity
//    s_level_scaling[i] = q31sub(q31add(s_outlevel[i], f32_to_q31(clipminmaxf(-99, depth, 99) * paramScale(s_kls_scale, i) * ((curve & 0x01) ? ((POW2F(dp * .083333333f) - 1.f) * .015625f) : (s_level_scale_factor * dp)) * LEVEL_SCALE_FACTOR_DB)), 0x07000000);
    s_level_scaling[i] = q31sub(q31add(s_outlevel[i], f32_to_q31(depth * s_klslevel[i] * ((curve & 0x01) ? ((POW2F(dp * .083333333f) - 1.f) * .015625f) : (s_level_scale_factor * dp)))), 0x07000000);
//    setOpLevel(i);
  }
  s_zone_transposed += s_transpose;
  if (s_velocity_raw == 1023)
    setVelocity(velocity);
  s_state |= state_noteon;
}

fast_inline void noteOff(uint8_t note) {
  for (uint32_t i = 0; i < OPERATOR_COUNT; i++) {
    s_egsrate[i][EG_STAGE_COUNT - 1] = calc_rate(i, EG_STAGE_COUNT - 1, DX7_DECAY_RATE_FACTOR, s_release_rate_exp_factor, note);
    s_egsrate[i][EG_STAGE_COUNT * 2 - 1] = calc_rate(i, EG_STAGE_COUNT - 1, DX7_ATTACK_RATE_FACTOR, s_attack_rate_exp_factor, note);
    s_egsrate_recip[i][0] = 1.f / s_egsrate[i][EG_STAGE_COUNT - 1];
    s_egsrate_recip[i][1] = 1.f / s_egsrate[i][EG_STAGE_COUNT * 2 - 1];
  }
  s_state |= state_noteoff;
}

__unit_callback int8_t unit_init(const unit_runtime_desc_t * desc) {
  if (!desc)
    return k_unit_err_undef;
  if (desc->target != UNIT_HEADER_TARGET_FIELD)
    return k_unit_err_target;
  if (!UNIT_API_IS_COMPAT(desc->api))
    return k_unit_err_api_version;
  if (desc->samplerate != 48000)
    return k_unit_err_samplerate;
  if (desc->input_channels != UNIT_INPUT_CHANNELS || desc->output_channels != UNIT_OUTPUT_CHANNELS)
    return k_unit_err_geometry;
#ifdef UNIT_TARGET_MODULE_OSC
#ifdef UNIT_OSC_H_
  runtime_context = (unit_runtime_osc_context_t *)desc->hooks.runtime_context;
#endif
#else
  runtime_context = (unit_runtime_genericfx_context_t *)desc->hooks.runtime_context;
#endif

#ifdef WFROM
  osc_wave_init_all();
#endif

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
#ifdef UNIT_TARGET_PLATFORM_NTS3_KAOSS
//ToDo: switch to gate_arp.h
//  if (s_tempo_new != s_tempo_old) {
//    float tempo = uq16_16_to_f32(s_tempo_new);
//  }
  if (s_arp_state != arp_stopped && s_arp_ptr != nullptr) {
    uint32_t cnt = s_arp_quantize;
    if (s_arp_state == arp_gate_on)
      cnt *= s_arp_ptr[s_arp_idx] >> 4;
    else if (s_arp_state == arp_gate_off)
      cnt *= s_arp_ptr[s_arp_idx] & 0x0F;

    if (s_arp_counter >= cnt) {
      s_arp_counter -= cnt;
      if (s_arp_state == arp_gate_on) {
        noteOff(TOUCH_NOTE);
        s_arp_state = arp_gate_off;
      } else if (s_arp_state == arp_gate_off) {
        if (s_arp_ptr[++s_arp_idx] == 0)
          s_arp_idx = 0;
        noteOn(TOUCH_NOTE, 127);
        s_arp_state = arp_gate_on;
      }
    }
    s_arp_counter += frames;
  }
  s_arp_sample_counter += frames;
#elif defined(UNIT_TARGET_PLATFORM_MICROKORG2)
//ToDo: mK2 stuck fix
//  for (uint32_t voice_idx = 0; voice_idx < runtime_context->voiceLimit; voice_idx++) {
  for (uint32_t voice_idx = 0; voice_idx < 1; voice_idx++) {
    if (runtime_context->trigger & (1 << voice_idx)) {
      noteOn((uint32_t)runtime_context->pitch[voice_idx], 127);
    }
  }
#endif
  if (s_state) {
#ifdef WFGEN
    if ((s_state & state_wave_changed) != 0) {
      uint32_t i = 0;
      for (i = 0; i < OPERATOR_COUNT && s_waveform[i] == s_waveform_current[i]; i++);
      if (i < OPERATOR_COUNT) {
        osc_wavebank_preload(i, s_waveform[i]);
        s_waveform_current[i] = s_waveform[i];
        float * __restrict out_p = out;
        const float * out_e = out_p + frames * UNIT_OUTPUT_CHANNELS;
        for (; out_p != out_e; *out_p++ = 0.f);
      } else
        s_state &= ~state_wave_changed;
      return;

/*
      for (uint32_t i = 0; i < OPERATOR_COUNT; i++) {
        if (s_waveform[i] != s_waveform_current[i]) {
          osc_wavebank_preload(i, s_waveform[i]);
          s_waveform_current[i] = s_waveform[i];
        }
      }
      for (uint32_t f = frames; f--; *yn++ = 0);
      s_state &= ~state_wave_changed;
      return;
*/
    }
#endif
  if (s_state == state_noteon) {
    for (uint32_t i = 0; i < OPERATOR_COUNT; i++) {
      for (uint32_t j = 0; j < EG_STAGE_COUNT - 1; j++) {
        s_egsrate[i][j] = s_egsrate[i][j + EG_STAGE_COUNT];
        s_sample_count[i][j] = s_sample_count[i][j + EG_STAGE_COUNT];
      }
      s_egstage[i] = 0;
      if (s_opi)
        s_phase[i] = 0;
//todo: to reset or not to reset - that is the question (stick with the operator phase init)
      s_opval[i] = 0;
      s_egval[i] = s_eglevel[i][EG_STAGE_COUNT - 1];
//      setLevel();
// make it non-negative and apply -96dB to further fit EG level
//      s_oplevel[i] = q31sub((usat_lsl(31, q31add(s_level_scaling[i], s_velocitylevel[i]), 0)), 0x7F000000);
      setOpLevel(i);
    }
    s_sample_num = 0;
#ifdef PEG
    s_pegval = s_peglevel[PEG_STAGE_COUNT - 1];
    s_pegstage = s_peg_stage_start;
#endif
    s_state &= ~state_noteon;
  } else {
    int32_t dl;
    uint32_t samples;
    for (uint32_t i = 0; i < OPERATOR_COUNT; i++) {
      samples = s_sample_num;
      dl = s_eglevel[i][EG_STAGE_COUNT - 1] - s_egval[i];
      if (dl != 0) {
        if (dl < 0) {
          samples += dl * s_egsrate_recip[i][0];
        } else {
          s_egsrate[i][EG_STAGE_COUNT - 1] = s_egsrate[i][EG_STAGE_COUNT * 2 - 1];
          samples += dl * s_egsrate_recip[i][1];
        }
      } else {
        s_egsrate[i][EG_STAGE_COUNT - 1] = 0;
      }
      s_sample_count[i][EG_STAGE_COUNT - 1] = samples;
      s_egstage[i] = EG_STAGE_COUNT - 1;
    }
#ifdef PEG
    dl = s_peglevel[PEG_STAGE_COUNT - 1] - s_pegval;
    if (dl < 0) {
      s_pegrate[PEG_STAGE_COUNT - 1] = - s_pegrate[PEG_STAGE_COUNT];
      s_peg_sample_count[PEG_STAGE_COUNT - 1] = s_sample_num - dl * s_pegrate_releaserecip;
    } else {
      s_pegrate[PEG_STAGE_COUNT - 1] = s_pegrate[PEG_STAGE_COUNT];
      s_peg_sample_count[PEG_STAGE_COUNT - 1] = s_sample_num + dl * s_pegrate_releaserecip;
    }
    s_pegstage = PEG_STAGE_COUNT - 1;
#endif
    s_state &= ~(state_noteoff | state_noteon);
  }
  }
  q31_t osc_out, modw0;
  q31_t opw0[OPERATOR_COUNT];
#ifdef FINE_TUNE
#ifdef UNIT_OSC_H_
#ifdef UNIT_TARGET_PLATFORM_MICROKORG2
//ToDo: mK2 per-voice pitch
  uint32_t voice_idx = 0;
  uint32_t pitch = (uint32_t)(runtime_context->pitch[voice_idx] * 16777216.f);
#else
  uint32_t pitch = s_pitchbend + ((uint32_t)runtime_context->pitch << 16);
#endif
#else
  uint32_t pitch = s_pitchbend + (s_note << 24);
#endif
#else
  uint32_t pitch = runtime_context->pitch;
#endif
/*
#ifndef KIT_MODE
  if (s_kit_voice)
#endif
#ifdef FINE_TUNE
    pitch = KIT_CENTER << 24;
#else
    pitch = KIT_CENTER << 8;
#endif
*/
//  int32_t pitch = params->pitch + s_transpose;
#ifdef PEG
#ifdef FINE_TUNE
  pitch += s_pegval;
#else
  pitch += s_pegval >> 16;
#endif
#endif
//  pitch_t basew0 = f32_to_pitch(osc_w0f_for_note((pitch >> 8) + s_transpose, pitch & 0xFF));
  pitch_t basew0;
  for (uint32_t i = 0; i < OPERATOR_COUNT; i++) {
    if (s_pitchfreq[i]) {
#ifdef FINE_TUNE
      uint32_t p;
      p = pitch + (s_detune[i] + paramOffset(s_detune_offset, i) * .25f * 2.56f) * paramScale(s_detune_scale, i) * FINE_TUNE_FACTOR;
      uint8_t note = clipmini32(0, (p >> 24) + s_zone_transposed);
      basew0 = f32_to_pitch(clipmaxf(linintf((p & 0xFFFFFF) * 5.9604645e-8f, osc_notehzf(note), osc_notehzf(note + 1)), k_note_max_hz) * k_samplerate_recipf);
#else
      basew0 = f32_to_pitch(osc_w0f_for_note(((pitch + s_detune[i]) >> 8) + s_transpose, (pitch + s_detune[i]) & 0xFF));
#endif
      opw0[i] = pitch_to_phase(pitch_mul(s_oppitch[i], basew0));
    } else
      opw0[i] = pitch_to_phase(s_oppitch[i]);
/*
    int32_t p = s_oppitch[i] + s_detune[i];
    if (s_pitchfreq[i])
        p += pitch;
    p = usat(p, 16);
    opw0[i] = f32_to_q31(osc_w0f_for_note(p >> 8, p & 0xFF));
*/
  }

//  q31_t * __restrict y = (q31_t *)yn;
///ldr r10, &s_opval
///ldm r10, {r0, r1, r2, r3}
//  for (uint32_t f = frames; f--; y++) {
  float * __restrict out_p = out;
#if UNIT_INPUT_CHANNELS != 0
  const float * out_e = out_p + frames * UNIT_OUTPUT_CHANNELS;
  const float * __restrict in_p = in;
  for (; out_p != out_e; in_p += UNIT_INPUT_CHANNELS, out_p += UNIT_OUTPUT_CHANNELS) {
    q31_t osc_in = f32_to_q31((in_p[0] + in_p[1]) * .5f);
#elif defined(UNIT_TARGET_PLATFORM_MICROKORG2)
//ToDo: correct mK2 output
  const float * out_e = out_p + frames * runtime_context->outputStride;
  for (; out_p != out_e; out_p += runtime_context->outputStride) {
#else
  const float * out_e = out_p + frames * UNIT_OUTPUT_CHANNELS;
  for (; out_p != out_e; out_p += UNIT_OUTPUT_CHANNELS) {
#endif
#ifdef ROLLOUT
  modw0 = 0;
        __asm__ volatile ( \
"lsls r1, %[s_algorithm_i], #26\n" \
"itt mi\n" \
"ldrmi.w r1, [%[s_opval], #24]\n" \
"addmi %[modw0], %[modw0], r1\n" \
: [modw0] "+r" (modw0) \
: [s_algorithm_i] "l" (s_algorithm[0]), [s_opval] "r" (s_opval) \
: "r1" \
        );
      modw0 = ((smmul(modw0, MI_SCALE_FACTOR)) << 3) + s_phase[0];
#ifdef WFBITS
      s_opval[0] = smmul(osc_wavebank(modw0, s_waveform[0]), param_eglut(s_egval[0], s_oplevel[0])) << 1;
#else
      s_opval[0] = smmul(osc_sinq(modw0), param_eglut(s_egval[0], s_oplevel[0])) << 1;
#endif

  modw0 = 0;
        __asm__ volatile ( \
"lsls r1, %[s_algorithm_i], #26\n" \
"itt mi\n" \
"ldrmi.w r1, [%[s_opval], #24]\n" \
"addmi %[modw0], %[modw0], r1\n" \
"lsls r1, %[s_algorithm_i], #31\n" \
"itt mi\n" \
"ldrmi.w r1, [%[s_opval], #0]\n" \
"addmi %[modw0], %[modw0], r1\n" \
: [modw0] "+r" (modw0) \
: [s_algorithm_i] "l" (s_algorithm[1]), [s_opval] "r" (s_opval) \
: "r1" \
        );
      modw0 = ((smmul(modw0, MI_SCALE_FACTOR)) << 3) + s_phase[1];
#ifdef WFBITS
      s_opval[1] = smmul(osc_wavebank(modw0, s_waveform[1]), param_eglut(s_egval[1], s_oplevel[1])) << 1;
#else
      s_opval[1] = smmul(osc_sinq(modw0), param_eglut(s_egval[1], s_oplevel[1])) << 1;
#endif
  modw0 = 0;
        __asm__ volatile ( \
"lsls r1, %[s_algorithm_i], #26\n" \
"itt mi\n" \
"ldrmi.w r1, [%[s_opval], #24]\n" \
"addmi %[modw0], %[modw0], r1\n" \
"lsls r1, %[s_algorithm_i], #30\n" \
"itt mi\n" \
"ldrmi.w r1, [%[s_opval], #4]\n" \
"addmi %[modw0], %[modw0], r1\n" \
"lsls r1, %[s_algorithm_i], #31\n" \
"itt mi\n" \
"ldrmi.w r1, [%[s_opval], #0]\n" \
"addmi %[modw0], %[modw0], r1\n" \
"end%=:\n" \
: [modw0] "+r" (modw0) \
: [s_algorithm_i] "l" (s_algorithm[2]), [s_opval] "r" (s_opval) \
: "r1" \
        );
      modw0 = ((smmul(modw0, MI_SCALE_FACTOR)) << 3) + s_phase[2];
#ifdef WFBITS
      s_opval[2] = smmul(osc_wavebank(modw0, s_waveform[2]), param_eglut(s_egval[2], s_oplevel[2])) << 1;
#else
      s_opval[2] = smmul(osc_sinq(modw0), param_eglut(s_egval[2], s_oplevel[2])) << 1;
#endif
  modw0 = 0;
        __asm__ volatile ( \
"lsls r1, %[s_algorithm_i], #26\n" \
"itt mi\n" \
"ldrmi.w r1, [%[s_opval], #24]\n" \
"addmi %[modw0], %[modw0], r1\n" \
"lsls r1, %[s_algorithm_i], #29\n" \
"itt mi\n" \
"ldrmi.w r1, [%[s_opval], #8]\n" \
"addmi %[modw0], %[modw0], r1\n" \
"lsls r1, %[s_algorithm_i], #30\n" \
"itt mi\n" \
"ldrmi.w r1, [%[s_opval], #4]\n" \
"addmi %[modw0], %[modw0], r1\n" \
"lsls r1, %[s_algorithm_i], #31\n" \
"itt mi\n" \
"ldrmi.w r1, [%[s_opval], #0]\n" \
"addmi %[modw0], %[modw0], r1\n" \
"end%=:\n" \
: [modw0] "+r" (modw0) \
: [s_algorithm_i] "l" (s_algorithm[3]), [s_opval] "r" (s_opval) \
: "r1" \
        );
      modw0 = ((smmul(modw0, MI_SCALE_FACTOR)) << 3) + s_phase[3];
#ifdef WFBITS
      s_opval[3] = smmul(osc_wavebank(modw0, s_waveform[3]), param_eglut(s_egval[3], s_oplevel[3])) << 1;
#else
      s_opval[3] = smmul(osc_sinq(modw0), param_eglut(s_egval[3], s_oplevel[3])) << 1;
#endif
  modw0 = 0;
        __asm__ volatile ( \
"lsls r1, %[s_algorithm_i], #26\n" \
"itt mi\n" \
"ldrmi.w r1, [%[s_opval], #24]\n" \
"addmi %[modw0], %[modw0], r1\n" \
"lsls r1, %[s_algorithm_i], #28\n" \
"itt mi\n" \
"ldrmi.w r1, [%[s_opval], #12]\n" \
"addmi %[modw0], %[modw0], r1\n" \
"lsls r1, %[s_algorithm_i], #29\n" \
"itt mi\n" \
"ldrmi.w r1, [%[s_opval], #8]\n" \
"addmi %[modw0], %[modw0], r1\n" \
"lsls r1, %[s_algorithm_i], #30\n" \
"itt mi\n" \
"ldrmi.w r1, [%[s_opval], #4]\n" \
"addmi %[modw0], %[modw0], r1\n" \
"lsls r1, %[s_algorithm_i], #31\n" \
"itt mi\n" \
"ldrmi.w r1, [%[s_opval], #0]\n" \
"addmi %[modw0], %[modw0], r1\n" \
"end%=:\n" \
: [modw0] "+r" (modw0) \
: [s_algorithm_i] "l" (s_algorithm[4]), [s_opval] "r" (s_opval) \
: "r1" \
        );
      modw0 = ((smmul(modw0, MI_SCALE_FACTOR)) << 3) + s_phase[4];
#ifdef WFBITS
      s_opval[4] = smmul(osc_wavebank(modw0, s_waveform[4]), param_eglut(s_egval[4], s_oplevel[4])) << 1;
#else
      s_opval[4] = smmul(osc_sinq(modw0), param_eglut(s_egval[4], s_oplevel[4])) << 1;
#endif
  modw0 = 0;
        __asm__ volatile ( \
"lsls r1, %[s_algorithm_i], #26\n" \
"itt mi\n" \
"ldrmi.w r1, [%[s_opval], #24]\n" \
"addmi %[modw0], %[modw0], r1\n" \
"lsls r1, %[s_algorithm_i], #27\n" \
"itt mi\n" \
"ldrmi.w r1, [%[s_opval], #16]\n" \
"addmi %[modw0], %[modw0], r1\n" \
"lsls r1, %[s_algorithm_i], #28\n" \
"itt mi\n" \
"ldrmi.w r1, [%[s_opval], #12]\n" \
"addmi %[modw0], %[modw0], r1\n" \
"lsls r1, %[s_algorithm_i], #29\n" \
"itt mi\n" \
"ldrmi.w r1, [%[s_opval], #8]\n" \
"addmi %[modw0], %[modw0], r1\n" \
"lsls r1, %[s_algorithm_i], #30\n" \
"itt mi\n" \
"ldrmi.w r1, [%[s_opval], #4]\n" \
"addmi %[modw0], %[modw0], r1\n" \
"lsls r1, %[s_algorithm_i], #31\n" \
"itt mi\n" \
"ldrmi.w r1, [%[s_opval], #0]\n" \
"addmi %[modw0], %[modw0], r1\n" \
"end%=:\n" \
: [modw0] "+r" (modw0) \
: [s_algorithm_i] "l" (s_algorithm[5]), [s_opval] "r" (s_opval) \
: "r1" \
        );
      modw0 = ((smmul(modw0, MI_SCALE_FACTOR)) << 3) + s_phase[5];
#ifdef WFBITS
      s_opval[5] = smmul(osc_wavebank(modw0, s_waveform[5]), param_eglut(s_egval[5], s_oplevel[5])) << 1;
#else
      s_opval[5] = smmul(osc_sinq(modw0), param_eglut(s_egval[5], s_oplevel[5])) << 1;
#endif
#else
#ifndef MOD16
    osc_out = 0;
#endif
///ldr r11, &s_modmatrix
    for (uint32_t i = 0; i < OPERATOR_COUNT; i++) {
///ldm r11!, {r4, r5, r6, r7}
///smuad r4, r0, r4
///smlad r4, r1, r5, r4
///smlad r4, r2, r6, r4
///smlad r4, r3, r7, r4
#ifdef MOD16
#ifdef OP6
        __asm__ volatile ( \
"add %[s_modmatrix], %[s_modmatrix], %[i], lsl #4\n" \
"ldr r0, [%[s_opval], #0]\n" \
"ldr r1, [%[s_modmatrix], #0]\n" \
"smuad %[modw0], r0, r1\n" \
"ldr r0, [%[s_opval], #4]\n" \
"ldr r1, [%[s_modmatrix], #4]\n" \
"smlad %[modw0], r0, r1, %[modw0]\n" \
"ldr r0, [%[s_opval], #8]\n" \
"ldr r1, [%[s_modmatrix], #8]\n" \
"smlad %[modw0], r0, r1, %[modw0]\n" \
"ldr r0, [%[s_opval], #12]\n" \
"ldr r1, [%[s_modmatrix], #12]\n" \
"smlad %[modw0], r0, r1, %[modw0]\n" \
: [modw0] "=&r" (modw0) \
: [i] "r" (i), [s_opval] "r" (s_opval), [s_modmatrix] "r" (s_modmatrix) \
: "r0", "r1" \
        );
#else
      __asm__ volatile ( \
"add %[s_modmatrix], %[s_modmatrix], %[i], lsl #2\n" \
"add %[s_modmatrix], %[s_modmatrix], %[i], lsl #3\n" \
"ldr r0, [%[s_opval], #0]\n" \
"ldr r1, [%[s_modmatrix], #0]\n" \
"smuad %[modw0], r0, r1\n" \
"ldr r0, [%[s_opval], #4]\n" \
"ldr r1, [%[s_modmatrix], #4]\n" \
"smlad %[modw0], r0, r1, %[modw0]\n" \
"ldr r0, [%[s_opval], #8]\n" \
"ldr r1, [%[s_modmatrix], #8]\n" \
"smlad %[modw0], r0, r1, %[modw0]\n" \
: [modw0] "=&r" (modw0) \
: [i] "r" (i), [s_opval] "r" (s_opval), [s_modmatrix] "r" (s_modmatrix) \
: "r0", "r1" \
        );
#endif
      modw0 = (modw0 << 3) + s_phase[i];
#else
      modw0 = 0;
#if UNIT_INPUT_CHANNELS != 0
      if (i == s_audio_op_phase)
        modw0 = osc_in;
#endif
#if FEEDBACK_COUNT == 2
#ifdef __cortex_a7__
__asm__ volatile ( \
"lsls r1, %[s_algorithm_i], #25\n" \
"bpl 1f\n" \
"ldr r1, [%[s_opval], #28]\n" \
"add %[modw0], %[modw0], r1\n" \
"1:\n" \
: [modw0] "+r" (modw0) \
: [i] "r" (i), [s_algorithm_i] "l" (s_algorithm[i]), [s_opval] "r" (s_opval) \
: "r1" \
        );
#else
      __asm__ volatile ( \
"lsls r1, %[s_algorithm_i], #25\n" \
"itt mi\n" \
"ldrmi.w r1, [%[s_opval], #28]\n" \
"addmi %[modw0], %[modw0], r1\n" \
: [modw0] "+r" (modw0) \
: [i] "r" (i), [s_algorithm_i] "l" (s_algorithm[i]), [s_opval] "r" (s_opval) \
: "r1" \
        );
#endif
#endif
#ifdef OP6
#ifdef __cortex_a7__
__asm__ volatile ( \
"lsls r1, %[s_algorithm_i], #26\n" \
"bpl 0f\n" \
"ldr r1, [%[s_opval], #24]\n" \
"add %[modw0], %[modw0], r1\n" \
"0:\n" \
"cmp %[i], #0\n" \
"beq 1f\n" \
"cmp %[i], #1\n" \
"beq 2f\n" \
"cmp %[i], #2\n" \
"beq 3f\n" \
"cmp %[i], #3\n" \
"beq 4f\n" \
"cmp %[i], #4\n" \
"beq 5f\n" \
"lsls r1, %[s_algorithm_i], #27\n" \
"bpl 5f\n" \
"ldr r1, [%[s_opval], #16]\n" \
"add %[modw0], %[modw0], r1\n" \
"5:\n" \
"lsls r1, %[s_algorithm_i], #28\n" \
"bpl 4f\n" \
"ldr r1, [%[s_opval], #12]\n" \
"add %[modw0], %[modw0], r1\n" \
"4:\n" \
"lsls r1, %[s_algorithm_i], #29\n" \
"bpl 3f\n" \
"ldr r1, [%[s_opval], #8]\n" \
"add %[modw0], %[modw0], r1\n" \
"3:\n" \
"lsls r1, %[s_algorithm_i], #30\n" \
"bpl 2f\n" \
"ldr r1, [%[s_opval], #4]\n" \
"add %[modw0], %[modw0], r1\n" \
"2:\n" \
"lsls r1, %[s_algorithm_i], #31\n" \
"bpl 1f\n" \
"ldr r1, [%[s_opval], #0]\n" \
"add %[modw0], %[modw0], r1\n" \
"1:\n" \
: [modw0] "+r" (modw0) \
: [i] "r" (i), [s_algorithm_i] "l" (s_algorithm[i]), [s_opval] "r" (s_opval) \
: "r1" \
        );
#else
        __asm__ volatile ( \
"lsls r1, %[s_algorithm_i], #26\n" \
"itt mi\n" \
"ldrmi.w r1, [%[s_opval], #24]\n" \
"addmi %[modw0], %[modw0], r1\n" \
"tbb [pc, %[i]]\n" \
".byte 0x1C\n" \
".byte 0x17\n" \
".byte 0x12\n" \
".byte 0x0D\n" \
".byte 0x08\n" \
".byte 0x03\n" \
"lsls r1, %[s_algorithm_i], #27\n" \
"itt mi\n" \
"ldrmi.w r1, [%[s_opval], #16]\n" \
"addmi %[modw0], %[modw0], r1\n" \
"lsls r1, %[s_algorithm_i], #28\n" \
"itt mi\n" \
"ldrmi.w r1, [%[s_opval], #12]\n" \
"addmi %[modw0], %[modw0], r1\n" \
"lsls r1, %[s_algorithm_i], #29\n" \
"itt mi\n" \
"ldrmi.w r1, [%[s_opval], #8]\n" \
"addmi %[modw0], %[modw0], r1\n" \
"lsls r1, %[s_algorithm_i], #30\n" \
"itt mi\n" \
"ldrmi.w r1, [%[s_opval], #4]\n" \
"addmi %[modw0], %[modw0], r1\n" \
"lsls r1, %[s_algorithm_i], #31\n" \
"itt mi\n" \
"ldrmi.w r1, [%[s_opval], #0]\n" \
"addmi %[modw0], %[modw0], r1\n" \
: [modw0] "+r" (modw0) \
: [i] "r" (i), [s_algorithm_i] "l" (s_algorithm[i]), [s_opval] "r" (s_opval) \
: "r1" \
        );
#endif
#else
      __asm__ volatile ( \
"lsls r1, %[s_algorithm_i], #26\n" \
"itt mi\n" \
"ldrmi.w r1, [%[s_opval], #12]\n" \
"addmi %[modw0], %[modw0], r1\n" \
"tbb [pc, %[i]]\n" \
".byte 0x11\n" \
".byte 0x0C\n" \
".byte 0x07\n" \
".byte 0x02\n" \
"lsls r1, %[s_algorithm_i], #29\n" \
"itt mi\n" \
"ldrmi.w r1, [%[s_opval], #8]\n" \
"addmi %[modw0], %[modw0], r1\n" \
"lsls r1, %[s_algorithm_i], #30\n" \
"itt mi\n" \
"ldrmi.w r1, [%[s_opval], #4]\n" \
"addmi %[modw0], %[modw0], r1\n" \
"lsls r1, %[s_algorithm_i], #31\n" \
"itt mi\n" \
"ldrmi.w r1, [%[s_opval], #0]\n" \
"addmi %[modw0], %[modw0], r1\n" \
: [modw0] "+r" (modw0) \
: [i] "r" (i), [s_algorithm_i] "l" (s_algorithm[i]), [s_opval] "r" (s_opval) \
: "r1" \
        );
#endif
      modw0 = ((smmul(modw0, MI_SCALE_FACTOR)) << 3) + s_phase[i];
#endif
      s_phase[i] += opw0[i];

#ifdef MOD16
      s_opval[i] = smmul(OSC_FUNC(modw0), param_eglut(s_egval[i], s_oplevel[i])) >> 15;
#else
      s_opval[i] = smmul(
#if UNIT_INPUT_CHANNELS != 0
        i == s_audio_op_wave ? osc_in :
#endif
      OSC_FUNC(modw0), param_eglut(s_egval[i], s_oplevel[i])) << 1;
      osc_out += smmul(s_opval[i], s_comp[i]) << 1;
#endif
      if ( s_sample_num < s_sample_count[i][s_egstage[i]] ) {
        s_egval[i] = q31add(s_egval[i], s_egsrate[i][s_egstage[i]]);
      } else {
        s_egval[i] = s_eglevel[i][s_egstage[i]];
        if (s_egstage[i] < EG_STAGE_COUNT - 2)
          s_egstage[i]++;
      }
    }
#endif
#ifdef MOD16
#ifdef OP6
      __asm__ volatile ( \
///ldr r9, &s_comp
///ldm r9, {r4, r5, r6}
///smuad r4, r0, r4
///smlad r4, r1, r5, r4
///smlad r4, r2, r6, r4
///lsl r4, r4
"ldr r0, [%[s_opval], #0]\n" \
"ldr r1, [%[s_comp], #0]\n" \
"smuad %[osc_out], r0, r1\n" \
"ldr r0, [%[s_opval], #4]\n" \
"ldr r1, [%[s_comp], #4]\n" \
"smlad %[osc_out], r0, r1, %[osc_out]\n" \
"ldr r0, [%[s_opval], #8]\n" \
"ldr r1, [%[s_comp], #8]\n" \
"smlad %[osc_out], r0, r1, %[osc_out]\n" \
"lsl %[osc_out], %[osc_out], #1\n" \
: [osc_out] "=&r" (osc_out) \
: [s_opval] "r" (s_opval), [s_comp] "r" (s_comp) \
: "r0", "r1" \
        );
      __asm__ volatile ( \
"ldrb r1, [%[s_feedback_src], #0]\n" \
"ldrb r2, [%[s_feedback_src], #1]\n" \
"ldrsh r0, [%[s_opval], r1, lsl #1]\n" \
"ldr r1, [%[s_feedback], #0]\n" \
"smulwb r0, r1, r0\n" \
"ldrsh r1, [%[s_opval], r2, lsl #1]\n" \
"ldr r2, [%[s_feedback], #4]\n" \
"smulwb r1, r2, r1\n" \
"pkhtb r0, r1, r0, asr #16\n" \
"ldr r1, [%[s_opval], #16]\n" \
"str r0, [%[s_opval], #16]\n" \
"sadd16 r0, r0, r1\n" \
"str r0, [%[s_opval], #12]\n" \
: \
: [s_opval] "r" (s_opval), [s_feedback_src] "r" (s_feedback_src), [s_feedback] "r" (s_feedback) \
: "r0", "r1", "r2", "memory" \
        );
#else
      __asm__ volatile ( \
"ldr r0, [%[s_opval], #0]\n" \
"ldr r1, [%[s_comp], #0]\n" \
"smuad %0, r0, r1\n" \
"ldr r0, [%[s_opval], #4]\n" \
"ldr r1, [%[s_comp], #4]\n" \
"smlad %[osc_out], r0, r1, %[osc_out]\n" \
"lsl %[osc_out], %[osc_out], #1\n" \
: [osc_out] "=&r" (osc_out) \
: [s_opval] "r" (s_opval), [s_comp] "r" (s_comp) \
: "r0", "r1" \
      __asm__ volatile ( \
"ldrb r1, [%[s_feedback_src], #0]\n" \
"ldrb r2, [%[s_feedback_src], #1]\n" \
"ldrsh r0, [%[s_opval], r1, lsl #1]\n" \
"ldr r1, [%[s_feedback], #0]\n" \
"smulwb r0, r1, r0\n" \
"ldrsh r1, [%[s_opval], r2, lsl #1]\n" \
"ldr r2, [%[s_feedback], #4]\n" \
"smulwb r1, r2, r1\n" \
"pkhtb r0, r1, r0, asr #16\n" \
"ldr r1, [%[s_opval], #12]\n" \
"str r0, [%[s_opval], #12]\n" \
"sadd16 r0, r0, r1\n" \
"str r0, [%[s_opval], #8]\n" \
: \
: [s_opval] "r" (s_opval), [s_feedback_src] "r" (s_feedback_src), [s_feedback] "r" (s_feedback) \
: "r0", "r1", "r2", "memory" \
        );
#endif
#else
#ifdef ROLLOUT
    for (uint32_t i = 0; i < OPERATOR_COUNT; i++) {
      osc_out += smmul(s_opval[i], s_comp[i]) << 1;
      s_phase[i] += opw0[i];
      if ( s_sample_num < s_sample_count[i][s_egstage[i]] ) {
        s_egval[i] = q31add(s_egval[i], s_egsrate[i][s_egstage[i]]);
      } else {
        s_egval[i] = s_eglevel[i][s_egstage[i]];
        if (s_egstage[i] < EG_STAGE_COUNT - 2)
          s_egstage[i]++;
      }
    }
#endif
    s_opval[OPERATOR_COUNT] = s_opval[OPERATOR_COUNT + FEEDBACK_COUNT];
    s_opval[OPERATOR_COUNT + FEEDBACK_COUNT] = smmul(s_opval[s_feedback_src[0]], s_feedback[0]);
    s_opval[OPERATOR_COUNT] += s_opval[OPERATOR_COUNT + FEEDBACK_COUNT];
#if FEEDBACK_COUNT == 2
    s_opval[OPERATOR_COUNT + 1] = s_opval[OPERATOR_COUNT + FEEDBACK_COUNT + 1];
    s_opval[OPERATOR_COUNT + FEEDBACK_COUNT + 1] = smmul(s_opval[s_feedback_src[1]], s_feedback[1]);
    s_opval[OPERATOR_COUNT + 1] += s_opval[OPERATOR_COUNT + FEEDBACK_COUNT + 1];
#endif
#endif
#ifdef PEG
    if (
      s_sample_num < s_peg_sample_count[s_pegstage]
    ) {
      s_pegval += s_pegrate[s_pegstage];
    } else {
      s_pegval = s_peglevel[s_pegstage];
      if (s_pegstage < PEG_STAGE_COUNT - 2)
        s_pegstage++;
    }
#endif
    s_sample_num++;
#ifdef SHAPE_LFO
    if (s_audio_mod == audio_mod_amp)
      osc_out = (osc_in >> 1) + 0x40000000;
    if (s_audio_mod != audio_mod_none)
      osc_out = smmul(osc_out, osc_in) << 1;
    out_p[0] = q31_to_f32(q31sub(osc_out, smmul(osc_out, runtime_context->shape_lfo) << 1));
#if UNIT_OUTPUT_CHANNELS == 2
    out_p[1] = out_p[0];
#endif
#else
    *out_p = q31_to_f32(osc_out);
#if UNIT_OUTPUT_CHANNELS == 2
    out_p[1] = out_p[0];
#endif
#endif
  }
}

__unit_callback void unit_set_param_value(uint8_t index, int32_t value) {
  value = (int16_t)value;
  sParams[index] = value;    
/*
  switch (index) {
    case param_shape:
      s_velocity = f32_to_q31((POWF(value * .124144672f, .27f) * 60.f - 208.f) * .0625f * LEVEL_SCALE_FACTOR_DB);
      setVelocityLevel();
      break;
    case param_alt:
      s_feedback_offset[0] = value * .0625f; // 1/16
      setFeedback(0);
      break;
    case param_dxvoice_idx:
#ifndef KIT_MODE
      s_voice[0] = value;
#endif
    break;
    default:
      break;
  }
*/

  index = CUSTOM_PARAM_GET(index);
  switch (index) {
    case CUSTOM_PARAM_ID(1):
//      s_velocity = f32_to_q31((POWF(clipmaxf(value * .125f, 127.f), .27f) * 60.f - 208.f) * .0625f * LEVEL_SCALE_FACTOR_DB);
//      setVelocityLevel();
        s_velocity_raw = value;
        setVelocity(clipmaxi32(value, 1016) * .125f);
      break;
#ifndef KIT_MODE
    case CUSTOM_PARAM_ID(2):
    case CUSTOM_PARAM_ID(3):
    case CUSTOM_PARAM_ID(4):
      s_voice[index - CUSTOM_PARAM_ID(2)] = value;
      break;
#endif
    case CUSTOM_PARAM_ID(5):
    case CUSTOM_PARAM_ID(6):
      s_split_point[index - CUSTOM_PARAM_ID(5)] = value;
      break;
    case CUSTOM_PARAM_ID(7):
    case CUSTOM_PARAM_ID(8):
    case CUSTOM_PARAM_ID(9):
      s_zone_transpose[index - CUSTOM_PARAM_ID(7)] = value;
      break;
    case CUSTOM_PARAM_ID(10):
    case CUSTOM_PARAM_ID(11):
    case CUSTOM_PARAM_ID(12):
      s_zone_voice_shift[index - CUSTOM_PARAM_ID(10)] = value;
      break;
/*
    case CUSTOM_PARAM_ID(13):
    case CUSTOM_PARAM_ID(14):
      CUSTOM_PARAM_SET(index - CUSTOM_PARAM_ID(13), value - 100 + (value >= 100 ? CUSTOM_PARAM_ID(1) : - CUSTOM_PARAM_ID(1)));
      break;
*/
    case CUSTOM_PARAM_ID(15):
#if FEEDBACK_COUNT == 2
    case CUSTOM_PARAM_ID(16):
#endif
      index -= CUSTOM_PARAM_ID(15);
      s_feedback_offset[index] = value * .015625f;
      goto setfeedback;
      break;
    case CUSTOM_PARAM_ID(17):
#if FEEDBACK_COUNT == 2
    case CUSTOM_PARAM_ID(18):
#endif
      index -= CUSTOM_PARAM_ID(17);
      s_feedback_scale[index] = value * .001953125f;
setfeedback:
      setFeedback(index);
      break;
    case CUSTOM_PARAM_ID(19):
#if FEEDBACK_COUNT == 2
    case CUSTOM_PARAM_ID(20):
#endif
      index -= CUSTOM_PARAM_ID(19);
      s_feedback_route[index] = value;
      setFeedbackRoute(index);
      break;
    case CUSTOM_PARAM_ID(21):
      s_algorithm_select = value;
      goto setalgorithm;
      break;
    case CUSTOM_PARAM_ID(22):
      s_algorithm_offset = value;
setalgorithm:
      setAlgorithm();
      break;
    case CUSTOM_PARAM_ID(23):
    case CUSTOM_PARAM_ID(24):
    case CUSTOM_PARAM_ID(25):
#ifdef OP6
    case CUSTOM_PARAM_ID(26):
    case CUSTOM_PARAM_ID(27):
#else
      index += 2;
#endif
    case CUSTOM_PARAM_ID(28):
    case CUSTOM_PARAM_ID(29):
    case CUSTOM_PARAM_ID(30):
    case CUSTOM_PARAM_ID(31):
      s_level_offset[CUSTOM_PARAM_ID(31) - index] = value;
      goto setoutlevel;
      break;
    case CUSTOM_PARAM_ID(32):
    case CUSTOM_PARAM_ID(33):
    case CUSTOM_PARAM_ID(34):
#ifdef OP6
    case CUSTOM_PARAM_ID(35):
    case CUSTOM_PARAM_ID(36):
#else
      index += 2;
#endif
    case CUSTOM_PARAM_ID(37):
    case CUSTOM_PARAM_ID(38):
    case CUSTOM_PARAM_ID(39):
    case CUSTOM_PARAM_ID(40):
      s_level_scale[CUSTOM_PARAM_ID(40) - index] = value;
setoutlevel:
      setOutLevel();
      break;
    case CUSTOM_PARAM_ID(41):
    case CUSTOM_PARAM_ID(42):
    case CUSTOM_PARAM_ID(43):
#ifdef OP6
    case CUSTOM_PARAM_ID(44):
    case CUSTOM_PARAM_ID(45):
#else
      index += 2;
#endif
    case CUSTOM_PARAM_ID(46):
    case CUSTOM_PARAM_ID(47):
    case CUSTOM_PARAM_ID(48):
    case CUSTOM_PARAM_ID(49):
      s_kls_offset[CUSTOM_PARAM_ID(49) - index] = value;
      for (uint32_t i = 0; i < OPERATOR_COUNT; i++)
        s_klsoffset[i] = paramOffset(s_kls_offset, i) * .25f;
      break;
    case CUSTOM_PARAM_ID(50):
    case CUSTOM_PARAM_ID(51):
    case CUSTOM_PARAM_ID(52):
#ifdef OP6
    case CUSTOM_PARAM_ID(53):
    case CUSTOM_PARAM_ID(54):
#else
      index += 2;
#endif
    case CUSTOM_PARAM_ID(55):
    case CUSTOM_PARAM_ID(56):
    case CUSTOM_PARAM_ID(57):
    case CUSTOM_PARAM_ID(58):
      s_kls_scale[CUSTOM_PARAM_ID(58) - index] = value;
      for (uint32_t i = 0; i < OPERATOR_COUNT; i++)
        s_klslevel[i] = paramScale(s_kls_scale, i) * LEVEL_SCALE_FACTOR_DB;
      break;
    case CUSTOM_PARAM_ID(59):
    case CUSTOM_PARAM_ID(60):
    case CUSTOM_PARAM_ID(61):
#ifdef OP6
    case CUSTOM_PARAM_ID(62):
    case CUSTOM_PARAM_ID(63):
#else
      index += 2;
#endif
    case CUSTOM_PARAM_ID(64):
    case CUSTOM_PARAM_ID(65):
    case CUSTOM_PARAM_ID(66):
    case CUSTOM_PARAM_ID(67):
      s_kvs_offset[CUSTOM_PARAM_ID(67) - index] = value;
      goto setkvslevel;
      break;
    case CUSTOM_PARAM_ID(68):
    case CUSTOM_PARAM_ID(69):
    case CUSTOM_PARAM_ID(70):
#ifdef OP6
    case CUSTOM_PARAM_ID(71):
    case CUSTOM_PARAM_ID(72):
#else
      index += 2;
#endif
    case CUSTOM_PARAM_ID(73):
    case CUSTOM_PARAM_ID(74):
    case CUSTOM_PARAM_ID(75):
    case CUSTOM_PARAM_ID(76):
      s_kvs_scale[CUSTOM_PARAM_ID(76) - index] = value;
setkvslevel:
      setKvsLevel();
      break;
    case CUSTOM_PARAM_ID(77):
    case CUSTOM_PARAM_ID(78):
    case CUSTOM_PARAM_ID(79):
#ifdef OP6
    case CUSTOM_PARAM_ID(80):
    case CUSTOM_PARAM_ID(81):
#else
      index += 2;
#endif
    case CUSTOM_PARAM_ID(82):
    case CUSTOM_PARAM_ID(83):
    case CUSTOM_PARAM_ID(84):
    case CUSTOM_PARAM_ID(85):
      s_egrate_offset[CUSTOM_PARAM_ID(85) - index] = value;
      for (uint32_t i = 0; i < OPERATOR_COUNT; i++)
        s_egrateoffset[i] = paramOffset(s_egrate_offset, i) * .25f;
      break;
    case CUSTOM_PARAM_ID(86):
    case CUSTOM_PARAM_ID(87):
    case CUSTOM_PARAM_ID(88):
#ifdef OP6
    case CUSTOM_PARAM_ID(89):
    case CUSTOM_PARAM_ID(90):
#else
      index += 2;
#endif
    case CUSTOM_PARAM_ID(91):
    case CUSTOM_PARAM_ID(92):
    case CUSTOM_PARAM_ID(93):
    case CUSTOM_PARAM_ID(94):
      s_egrate_scale[CUSTOM_PARAM_ID(94) - index] = value;
      for (uint32_t i = 0; i < OPERATOR_COUNT; i++)
        s_egratelevel[i] = paramScale(s_egrate_scale, i);
      break;
    case CUSTOM_PARAM_ID(95):
    case CUSTOM_PARAM_ID(96):
    case CUSTOM_PARAM_ID(97):
#ifdef OP6
    case CUSTOM_PARAM_ID(98):
    case CUSTOM_PARAM_ID(99):
#else
      index += 2;
#endif
    case CUSTOM_PARAM_ID(100):
    case CUSTOM_PARAM_ID(101):
    case CUSTOM_PARAM_ID(102):
    case CUSTOM_PARAM_ID(103):
      s_krs_offset[CUSTOM_PARAM_ID(103) - index] = value;
      for (uint32_t i = 0; i < OPERATOR_COUNT; i++)
        s_krsoffset[i] = paramOffset(s_krs_offset, i) * .015625f;
      break;
    case CUSTOM_PARAM_ID(104):
    case CUSTOM_PARAM_ID(105):
    case CUSTOM_PARAM_ID(106):
#ifdef OP6
    case CUSTOM_PARAM_ID(107):
    case CUSTOM_PARAM_ID(108):
#else
      index += 2;
#endif
    case CUSTOM_PARAM_ID(109):
    case CUSTOM_PARAM_ID(110):
    case CUSTOM_PARAM_ID(111):
    case CUSTOM_PARAM_ID(112):
      s_krs_scale[CUSTOM_PARAM_ID(112) - index] = value;
      for (uint32_t i = 0; i < OPERATOR_COUNT; i++)
        s_krslevel[i] = paramScale(s_krs_scale, i) * RATE_SCALING_FACTOR;
      break;
    case CUSTOM_PARAM_ID(113):
    case CUSTOM_PARAM_ID(114):
    case CUSTOM_PARAM_ID(115):
#ifdef OP6
    case CUSTOM_PARAM_ID(116):
    case CUSTOM_PARAM_ID(117):
#else
      index += 2;
#endif
    case CUSTOM_PARAM_ID(118):
    case CUSTOM_PARAM_ID(119):
    case CUSTOM_PARAM_ID(120):
    case CUSTOM_PARAM_ID(121):
      s_detune_offset[CUSTOM_PARAM_ID(121) - index] = value;
      break;
#ifdef FINE_TUNE
    case CUSTOM_PARAM_ID(122):
    case CUSTOM_PARAM_ID(123):
    case CUSTOM_PARAM_ID(124):
#ifdef OP6
    case CUSTOM_PARAM_ID(125):
    case CUSTOM_PARAM_ID(126):
#else
      index += 2;
#endif
    case CUSTOM_PARAM_ID(127):
    case CUSTOM_PARAM_ID(128):
    case CUSTOM_PARAM_ID(129):
    case CUSTOM_PARAM_ID(130):
      s_detune_scale[CUSTOM_PARAM_ID(130) - index] = value;
      break;
#endif
#ifdef WFBITS
#ifndef WFGEN
    case CUSTOM_PARAM_ID(131):
#ifdef OP6
    case CUSTOM_PARAM_ID(132):
#else
      index++;
#endif
    case CUSTOM_PARAM_ID(133):
    case CUSTOM_PARAM_ID(134):
#endif
    case CUSTOM_PARAM_ID(135):
    case CUSTOM_PARAM_ID(136):
    case CUSTOM_PARAM_ID(137):
#ifdef OP6
    case CUSTOM_PARAM_ID(138):
    case CUSTOM_PARAM_ID(139):
#else
      index += 2;
#endif
    case CUSTOM_PARAM_ID(140):
    case CUSTOM_PARAM_ID(141):
    case CUSTOM_PARAM_ID(142):
    case CUSTOM_PARAM_ID(143):
//#ifdef WFGEN
//      s_waveform_offset[CUSTOM_PARAM_ID(143) - index] = value >= 100 ? (value - 100) : (35 + 100 - value);
//#else
      value -=
        value >= 12 ? 7 :
        value >= 11 ? 3 :
        value >= 8 ? 8 :
        0;
      s_waveform_offset[CUSTOM_PARAM_ID(143) - index] = value;
//#endif
      setWaveform();
      break;
#endif
#ifdef WAVE_PINCH
    case CUSTOM_PARAM_ID(144):
    case CUSTOM_PARAM_ID(145):
    case CUSTOM_PARAM_ID(146):
#ifdef OP6
    case CUSTOM_PARAM_ID(147):
    case CUSTOM_PARAM_ID(148):
#else
      index += 2;
#endif
    case CUSTOM_PARAM_ID(149):
    case CUSTOM_PARAM_ID(150):
    case CUSTOM_PARAM_ID(151):
    case CUSTOM_PARAM_ID(152):
      s_waveform_pinch[CUSTOM_PARAM_ID(152) - index] = value;
      for (uint32_t i = 0; i < OPERATOR_COUNT; i++) {
        value = clipminmaxi32(1, 100 - paramOffset(s_waveform_pinch, i), 100);
        s_wavewidth[i * 2] = 0x0147AE14 * value; // 1/100 * witdh
        s_wavewidth[i * 2 + 1] = 0x64000000 / value; // (100 >> 7) / width
      }
      break;
#endif
/* LFO Route stub
    case CUSTOM_PARAM_ID(153):
      break;
    case CUSTOM_PARAM_ID(154):
      break;
*/
#if UNIT_INPUT_CHANNELS != 0
    case CUSTOM_PARAM_ID(155):
#ifdef UNIT_TARGET_PLATFORM_NTS1_MKII
      runtime_context->notify_input_usage(value == 0 ? k_runtime_osc_input_unused : k_runtime_osc_input_used);
#endif
      s_audio_op_wave= OPERATOR_COUNT;
      s_audio_op_phase = OPERATOR_COUNT;
      s_audio_mod = audio_mod_none;
      if (value == 0)
        return;
      else if (value <= OPERATOR_COUNT)
        s_audio_op_wave  = OPERATOR_COUNT - value;
      else if (value <= OPERATOR_COUNT * 2)
        s_audio_op_phase  = OPERATOR_COUNT * 2 - value;
      else if (value == 13)
        s_audio_mod = audio_mod_amp;
      else if (value == 14)
        s_audio_mod = audio_mod_ring;
      break;
#endif
#ifdef UNIT_TARGET_PLATFORM_NTS3_KAOSS
    case CUSTOM_PARAM_ID(156):
      if (value) {
        if (value != s_arp_num) {
          s_arp_num = value;
          s_arp_ptr = patterns;
          while (--value)
            while (*(s_arp_ptr++));
          s_arp_idx = 0;
        }
      } else {
        s_arp_ptr = nullptr;
//        s_arp_state = arp_stopped;
      }
      break;
#ifdef UNIT_TARGET_PLATFORM_NTS3_KAOSS
    case CUSTOM_PARAM_ID(157):
      if (value) {
        if (value != s_scale_num) {
          s_scale_ptr = scales;
          while (--value)
            while (*(s_scale_ptr++) & 0x0F);
          for (s_scale_steps = 0; s_scale_ptr[s_scale_steps] & 0x0F; s_scale_steps++);
          s_scale_steps <<= 1;
          if (s_scale_ptr[s_scale_steps >> 1] != 0)
            s_scale_steps++;
          s_scale_steps++;
        }
      } else {
        s_scale_ptr = nullptr;
      }
      s_scale_num = value;
      goto setpitch;
    case CUSTOM_PARAM_ID(158):
      s_pitchbend = value << 16;
setpitch:
      if (s_scale_ptr != nullptr) {
        s_pitchbend += TOUCH_NOTE << 24;
        uint32_t whole_pitch = (s_pitchbend / 0x0C000000) * 0x0C000000;
        uint32_t scale_pitch = 0x0C000000 / s_scale_steps;
        uint32_t key_idx = (s_pitchbend - whole_pitch) / scale_pitch;
        if (key_idx != s_scale_pitch_note) {
          s_scale_pitch_note = key_idx;
          noteOn(TOUCH_NOTE, 127);
        }
        s_pitchbend = whole_pitch;
        if (key_idx > 0) {
          key_idx--;
          uint32_t idx = s_scale_ptr[key_idx >> 1];
          if ((key_idx & 0x01) == 0)
            idx >>= 4;
          idx &= 0x0F;
          s_pitchbend += (idx << 24);
        }
        s_pitchbend -= TOUCH_NOTE << 24;
      } 
      break;
#endif
#endif
    default:
      break;
  }
}

__unit_callback int32_t unit_get_param_value(uint8_t id) {
  return sParams[id];
}

__unit_callback const char * unit_get_param_str_value(uint8_t id, int32_t value) {
  static char name[VOICE_NAME_SIZE + 1] = {0};
#ifdef UNIT_TARGET_PLATFORM_NTS3_KAOSS
  static const char notes[][3] = {"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"};
#endif
#ifdef UNIT_TARGET_PLATFORM_DRUMLOGUE
  static const char *allowed = " ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!?#$%&'()*+,-.:;<=>@";
#elif defined(UNIT_TARGET_PLATFORM_MICROKORG2)
  static const char *allowed = " ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._!\"#$%&'()*+,/:;<=>?@[]^`~";
//ToDo: mK2 waveform naming
#else
#endif
  static char string[12][5] = {"DX  ", "OP  ", "SY  ", "Saw ", "Sqr ", "Par ", "WA  ", "WB  ", "WC  ", "WD  ", "WE  ", "WF  "};
#ifdef WFBITS
  static const uint8_t wfcounts[12] = {8, 0, 16, 6, 6, 7, 16, 16, 14, 13, 15, 16};
#endif
  static const uint8_t algcounts[3] = {32, 8, 45};
  static char *s;
  static const char *vnam;
  const dx_voice_t *dxvoice_ptr;
  uint32_t j;
  value = (int16_t)value;
  id = CUSTOM_PARAM_GET(id);
  switch (id) {
    case CUSTOM_PARAM_ID(2):
    case CUSTOM_PARAM_ID(3):
    case CUSTOM_PARAM_ID(4):
      if ((dxvoice_ptr = getDxVoice(value)) == nullptr)
        break;
      vnam = dxvoice_ptr->dx7.vnam;
      if (*vnam == 0)
        vnam = &dxvoice_ptr->dx11.vnam[0];
#if defined(UNIT_TARGET_PLATFORM_DRUMLOGUE) || defined(UNIT_TARGET_PLATFORM_MICROKORG2)
      size_t len;
      memcpy(name, vnam, VOICE_NAME_SIZE);
      len = strlen(name);
      for (size_t l = strspn(name, allowed); l < len; name[l] = '.', l = strspn(name, allowed)); //replace non-allowed characters
//      for (char *p = strrchr(name, ' '); p == &name[len - 1]; *p = 0, p = strrchr(name, ' '), len--); //remove trailing spaces
//      for (char *p = strstr(name, "  "); p != nullptr; strcpy(p, p + 1), p = strstr(name, "  ")); //squash multiple spaces
      len = strlen(name);
      for (uint32_t i = 1; i < len; i++) { //capitalize
        if (strchr(" -.", name[i - 1]) == nullptr)
          name[i] = tolower(name[i]);
//ToDo: mK2 voice name split
      }
      for (char *p = strstr(name, " "); p != nullptr; strcpy(p, p + 1), p = strstr(name, " ")); //remove spaces
#else
      j = 0;
      for (uint32_t i = 0; i < VOICE_NAME_SIZE; i++) {
        if (vnam[i] != 0 && vnam[i] != ' ')
          name[j++] = (vnam[i] >= 'A' && vnam[i] <= 'Z') || (vnam[i] >= 'a' && vnam[i] <= 'z') || (vnam[i] >= '0' && vnam[i] <= '9') || vnam[i] == '-' ? vnam[i] : '_';
      }
      name[j] = 0;
#endif
      return name;
#if FEEDBACK_COUNT == 2
    case CUSTOM_PARAM_ID(20):
#endif
    case CUSTOM_PARAM_ID(19):
      if (value == 0 )
        return "Keep";
      value--;
      s = (char*)" to ";
      j = value / OPERATOR_COUNT;
      s[0] = '1' + j;
      s[3] = '1' + (value - j * OPERATOR_COUNT);
      return s;
    case CUSTOM_PARAM_ID(21):
      if (value == 0)
        return "Keep";
      for (uint32_t i = 0; i < sizeof(algcounts); i++) {
        s = string[i];
        if (value <= algcounts[i]) {
          if (i == 1)
            value += 32;
          j = value / 10;
          s[2] = '0' + j;
          s[3] = '0' + (value - j * 10);
          break;
        }
        value -= algcounts[i];
      }
      return s;
#ifdef WFBITS
    case CUSTOM_PARAM_ID(135):
    case CUSTOM_PARAM_ID(136):
    case CUSTOM_PARAM_ID(137):
#ifdef OP6
    case CUSTOM_PARAM_ID(138):
    case CUSTOM_PARAM_ID(139):
#else
      index += 2;
#endif
    case CUSTOM_PARAM_ID(140):
    case CUSTOM_PARAM_ID(141):
    case CUSTOM_PARAM_ID(142):
    case CUSTOM_PARAM_ID(143):
#ifdef WFGEN
      for (uint32_t i = 0; i < sizeof(wfcounts); i++) {
        s = string[i];
        if (value < wfcounts[i]) {
          value++;
          j = value / 10;
          if (i < 3 || i > 5)
            s[2] = '0' + j;
          s[3] = '0' + (value - j * 10);
          break;
        }
        value -= wfcounts[i];
      }
      return s;
#else

#endif
      break;
#endif
    case CUSTOM_PARAM_ID(155):
      if (value == 0)
        return "Thru";
      if (value == OPERATOR_COUNT * 2 + 1)
        return "AMod";
      if (value == OPERATOR_COUNT * 2 + 2)
        return "RMod";
      if (value <= OPERATOR_COUNT)
        s = (char*)"Op W";
      else {
        s = (char*)"Op P";
        value -= OPERATOR_COUNT;
      }
      s[2] = '0' + value;
      return s;
#ifdef UNIT_TARGET_PLATFORM_NTS3_KAOSS
    case CUSTOM_PARAM_ID(157):
      if (value == 0)
        return "Off";
      else
        return scale_names[value - 1];
#endif
    default:
      break;
  }
  return nullptr;
}

__unit_callback void unit_set_tempo(uint32_t tempo) {
  (void)tempo;
}

#if defined(UNIT_TARGET_PLATFORM_NTS1_MKII) || defined(UNIT_TARGET_PLATFORM_NTS3_KAOSS)
__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter) {
  (void)counter;
#ifdef UNIT_TARGET_PLATFORM_NTS3_KAOSS
  s_arp_quantize = (s_arp_sample_counter - s_arp_sample_counter_old) * 4 / PATTERN_QUANTIZE;
  s_arp_sample_counter_old  = s_arp_sample_counter;
#endif
}
#endif

#if defined(UNIT_TARGET_PLATFORM_NTS1_MKII) || defined(UNIT_TARGET_PLATFORM_DRUMLOGUE)
__unit_callback void unit_note_on(uint8_t note, uint8_t velocity) {
  noteOn(note, velocity);
}

__unit_callback void unit_note_off(uint8_t note) {
  noteOff(note);
}

__unit_callback void unit_all_note_off() {
  unit_note_off(0);
}

__unit_callback void unit_pitch_bend(uint16_t bend) {
  s_pitchbend = ((int32_t)bend - PITCH_BEND_CENTER) * PITCH_BEND_SENSITIVITY;
}

__unit_callback void unit_channel_pressure(uint8_t pressure) {
  setVelocity(pressure);
}

__unit_callback void unit_aftertouch(uint8_t note, uint8_t aftertouch) {
  (void)note;
  setVelocity(aftertouch);
}
#endif

#ifdef UNIT_TARGET_PLATFORM_NTS3_KAOSS
__unit_callback void unit_touch_event(uint8_t id, uint8_t phase, uint32_t x, uint32_t y) {
  (void)id;
  (void)phase;
  (void)x;
  (void)y;
    
  switch (phase) {
  case k_unit_touch_phase_began:
    noteOn(TOUCH_NOTE, 127);
//    if (s_arp_ptr != nullptr) {
      s_arp_state = arp_gate_on;
      s_arp_idx = 0;
      s_arp_counter = 0;
//    }
    break;
  case k_unit_touch_phase_ended:
  case k_unit_touch_phase_cancelled:
    noteOff(TOUCH_NOTE);
//    if (s_arp_ptr != nullptr) {
      s_arp_state = arp_stopped;
//    }
    break;  
  case k_unit_touch_phase_moved:
  case k_unit_touch_phase_stationary:
  default:
    break;
  }
}
#elif defined(UNIT_TARGET_PLATFORM_DRUMLOGUE)
__unit_callback void unit_gate_on(uint8_t velocity) {
  noteOn(gate_note = sParams[param_gate_note], velocity);
}

__unit_callback void unit_gate_off() {
  noteOff(gate_note);
}

__unit_callback const uint8_t * unit_get_param_bmp_value(uint8_t id, int32_t value) {
    (void)id;
    (void)value;
    return nullptr;
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
#elif defined(UNIT_TARGET_PLATFORM_MICROKORG2)
__unit_callback void unit_platform_exclusive(uint8_t messageId, void * data, uint32_t dataSize) {
  (void)dataSize;
  switch (messageId) {
    case kMk2PlatformExclusiveModData: {
//ToDo: modulation for Velocity and other destinations
      if (s_velocity_raw < 1023)
        break;
      const mk2_mod_data_t *mod_data = (mk2_mod_data_t *)data;
      for (uint32_t i = 0; i < kNumMk2ModSrc; i++) {
        if (mod_data->index[i] > 0 )
          continue;
        float depth = mod_data->depth[i] * 127.f;
        for (uint32_t voice_idx = 0; voice_idx < runtime_context->voiceLimit; voice_idx++)
          setVelocity(mod_data->data[voice_idx] * depth);
      }

      switch (runtime_context->voiceLimit) {
        case kMk2MaxVoices:
          for (uint32_t i = 0; i < kNumMk2ModSrc; i++) {
            if (mod_data->index[i] == 0 )
              continue;
            vModPatches[mod_data->index[i]].val[0] = vmulq_n_f32(vld1q_f32(&mod_data->data[i * kMk2MaxVoices]), mod_data->depth[i]);
            vModPatches[mod_data->index[i]].val[1] = vmulq_n_f32(vld1q_f32(&mod_data->data[i * kMk2MaxVoices + 4]), mod_data->depth[i]);
          }
          break;
        case kMk2HalfVoices:
          for (uint32_t i = 0; i < kNumMk2ModSrc; i++) {
            if (mod_data->index[i] < MOD_PATCHES_COUNT)
              vModPatches[mod_data->index[i]].val[0] = vmulq_n_f32(vld1q_f32(&mod_data->data[i * kMk2HalfVoices]), mod_data->depth[i]);
          }
          break;
        case kMk2QuarterVoices:
          for (uint32_t i = 0; i < kNumMk2ModSrc; i++) {
            if (mod_data->index[i] < MOD_PATCHES_COUNT)
              vst1_f32((float *)&((float32x2_t *)vModPatches)[mod_data->index[i]], vmul_n_f32(vld1_f32(&mod_data->data[i * kMk2QuarterVoices]), mod_data->depth[i]));
          }
          break;
        case kMk2SingleVoice: {
          for (uint32_t i = 0; i < kNumMk2ModSrc; i++) {
            if (mod_data->index[i] < MOD_PATCHES_COUNT)
              ((float *)vModPatches)[mod_data->index[i]] = mod_data->data[i * kMk2SingleVoice] * mod_data->depth[i];
          }
          break;
        }
      }
      break;
    }
    case kMk2PlatformExclusiveModDestName: {
      mk2_mod_dest_name_t *mod_dest = (mk2_mod_dest_name_t *)data;
//ToDo: dynamic mod destinations
      if (mod_dest->index == 0)
        strncpy(mod_dest->name, "Velocity", UNIT_PARAM_NAME_LEN);
      break;
    }  
  }
}
#endif
