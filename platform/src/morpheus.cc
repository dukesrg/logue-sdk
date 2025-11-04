/*
 *  File: morpheus.cc
 *
 *  Morphing wavetable oscillator
 * 
 *  2020-2025 (c) Oleg Burdaev
 *  mailto: dukesrg@gmail.com
 */

#include <string.h>
#include <stdio.h>
#include "logue_wrap.h"

#include "fixed_math.h"
#include "simplelfo.hpp"
#include "osc_apiq.h"

#define SAMPLE_COUNT 256
#define WAVE_COUNT 64
#define WAVE_COUNT_X 8
#define WAVE_COUNT_Y 8
#include "wavebank.h"

#define WAVE_COUNT_EXP 6
#define LFO_AXES_COUNT 2
#define VOICE_COUNT UNIT_OUTPUT_CHANNELS

#define LFO_MAX_RATE .33333334f //maximum LFO rate in Hz divided by logarithmic slope 10/30
#define LFO_RATE_LOG_BIAS 29.827234f //normalize logarithmic LFO for 0...1 log10(30+1)/0.05

enum {
  lfo_mode_one_shot = 0U,
  lfo_mode_key_trigger,
  lfo_mode_random,
  lfo_mode_free_run,
#if LFO_MODE_COUNT == 8
  lfo_mode_one_shot_plus_shape_lfo,
  lfo_mode_key_trigger_plus_shape_lfo,
  lfo_mode_random_plus_shape_lfo,
  lfo_mode_free_run_plus_shape_lfo,
#endif
};

enum {
#if LFO_AXES_COUNT == 2
  lfo_axis_x = 0U,
  lfo_axis_y,
#endif
  lfo_axes_count
};

enum {
  mod_type_amp,
  mod_type_ring,
  mod_type_phase,
  mod_type_none,
  mod_type_count
};

#if defined(UNIT_TARGET_PLATFORM) || defined(UNIT_TARGET_PLATFORM_NTS1)
#define BPM_SYNC_SUPPORTED
static float s_bpmfreq;
#ifdef UNIT_TARGET_PLATFORM_NTS1
#define BPM_SYNC_SCALE 600.f // 60 sec / 0.1 beat
#include "fx_api.h"
static uint16_t s_tempo;
//ToDo: NTS-1 LFO sync parameters support
static uint16_t s_lfo_bpm_sync[lfo_axes_count];
#define BPM_SYNC_VALUE s_lfo_bpm_sync
#else
#define BPM_SYNC_SCALE 3932160.f // 60 sec / 1/2<<16 beat
#define BPM_SYNC_VALUE (&Params[param_lfo_bpm_sync_x])
#endif 
#endif

typedef struct {
  uint32_t mode;
  uint32_t wave;
  float depth;
#ifdef BPM_SYNC_SUPPORTED
  float bpmfreq;
#endif
  float freq;  
  float shape;
  float offset;
  float snh[VOICE_COUNT];
  dsp::SimpleLFO lfo[VOICE_COUNT];
  q31_t phiold[VOICE_COUNT];
} vco_t;

static vco_t s_vco[lfo_axes_count];
static vco_t *s_vco_car, *s_vco_mod;

enum {
#ifdef USER_TARGET_PLATFORM
  param_lfo_rate_x = k_user_osc_param_shape,
  param_lfo_rate_y = k_user_osc_param_shiftshape,
  param_lfo_modes = k_user_osc_param_id1,
  param_lfo_dimensions = k_user_osc_param_id2,
  param_lfo_waveform_x = k_user_osc_param_id3,
  param_lfo_waveform_y = k_user_osc_param_id4,
  param_lfo_depth_x = k_user_osc_param_id5,
  param_lfo_depth_y = k_user_osc_param_id6,
#else
#ifdef UNIT_TARGET_MODULE_OSC
#ifdef UNIT_TARGET_PLATFORM_NTS1_MKII
  param_lfo_rate_x = k_num_unit_osc_fixed_param_id,
#else
  param_lfo_rate_x = 0U,
#endif 
  param_lfo_rate_y,
  param_lfo_modes,
#else
  param_lfo_modes = 0U,
#endif
  param_lfo_dimensions,
  param_lfo_waveform_x,
  param_lfo_waveform_y,
  param_lfo_depth_x,
  param_lfo_depth_y,
  param_lfo_bpm_sync_x,
#ifdef UNIT_TARGET_PLATFORM_NTS3_KAOSS
  param_lfo_bpm_sync_y = param_lfo_bpm_sync_x,
  param_pitch,
#else
  param_lfo_bpm_sync_y,
#endif
#endif
  param_num
};  

#ifdef UNIT_TARGET_PLATFORM
static int32_t Params[PARAM_COUNT];
#endif

#ifdef UNIT_TARGET_MODULE_OSC
#ifdef UNIT_OSC_H_
const unit_runtime_desc_t *runtime_desc;
#endif
#define PITCH runtime_context->pitch
#else
float amp;
uint16_t pitch = 0x3C00;
#define PITCH pitch
#endif

#if defined(UNIT_TARGET_PLATFORM_NTS1_MKII) || defined(UNIT_TARGET_PLATFORM_NTS1)
q31_t shape_lfo_old;
#endif

#ifdef UNIT_TARGET_PLATFORM_MICROKORG2
#define MOD_PATCHES_COUNT 2
static float32x4x2_t vModPatches[MOD_PATCHES_COUNT];
#define STRIDE (runtime_context->outputStride)
#define STRIDE_COUNT (((uint32_t)runtime_context->voiceLimit >> 3) + 1)
#define VOICE_LIMIT (((runtime_context->voiceLimit - 1) & (STRIDE - 1)) + 1)
float32x4x2_t vphase;
float *s_phase = (float *)&vphase;
#else
#define STRIDE UNIT_OUTPUT_CHANNELS
#define STRIDE_COUNT 1
#define VOICE_LIMIT UNIT_OUTPUT_CHANNELS
static float s_phase[VOICE_COUNT] = {0.f};
#endif

static uint32_t dimensionexp[lfo_axes_count];
static uint32_t dimension[lfo_axes_count];

static uint32_t s_mod_type;
static float s_mod_offset;
static float s_mod_scale;

static inline __attribute__((optimize("Ofast"), always_inline))
void set_vco_freq(uint32_t index) {
  for (uint32_t voice_idx = 0; voice_idx < VOICE_COUNT; voice_idx++)
    s_vco[index].lfo[voice_idx].setF0(
#ifdef BPM_SYNC_SUPPORTED
      s_vco[index].bpmfreq > 0.f ? s_vco[index].bpmfreq :
#endif
    s_vco[index].freq, k_samplerate_recipf);
}

static inline __attribute__((optimize("Ofast"), always_inline))
void set_vco_rate(uint32_t index, uint32_t value) {
  s_vco[index].shape = param_val_to_f32(value);
  s_vco[index].freq = (dbampf(s_vco[index].shape * LFO_RATE_LOG_BIAS) - 1.f) * LFO_MAX_RATE;
  set_vco_freq(index);
}

#ifdef BPM_SYNC_SUPPORTED
static inline __attribute__((optimize("Ofast"), always_inline))
void update_vco_bpm(uint32_t index) {
  s_vco[index].bpmfreq = s_bpmfreq * BPM_SYNC_VALUE[index];
  set_vco_freq(index);
}

static inline __attribute__((optimize("Ofast"), always_inline))
void set_tempo(uint32_t tempo) {
  s_bpmfreq = tempo == 0 ? 0.f : (BPM_SYNC_SCALE / tempo);
  update_vco_bpm(lfo_axis_x);
  update_vco_bpm(lfo_axis_y);
}
#endif

static inline __attribute__((optimize("Ofast"), always_inline))
float get_vco(vco_t &vco, uint32_t index) {
  float x;

  if ((vco.mode == lfo_mode_one_shot
#if LFO_MODE_COUNT == 8
  || vco.mode == lfo_mode_one_shot_plus_shape_lfo
#endif
  ) && vco.phiold[index] > 0 && vco.lfo[index].phi0 <= 0) {
    vco.lfo[index].phi0 = 0x7FFFFFFF;
    vco.lfo[index].w0 = 0;
  }

  switch (vco.wave) {
    case 0:
      x = vco.lfo[index].saw_bi();
     break;
    case 1:
      x = vco.lfo[index].triangle_bi();
      break;
    case 2:
      x = vco.lfo[index].square_bi();
      break;
    case 3:
      x = vco.lfo[index].sine_bi();
      break;
    case 4:
      if (vco.phiold[index] > 0 && vco.lfo[index].phi0 <= 0)
        vco.snh[index] = osc_white();
      x = vco.snh[index];
      break;
    default:
      x = q31_to_f32(vco.lfo[index].phi0) * .5f + .5f;
#if LFO_WAVEFORM_COUNT == 159
      if (vco.wave >= (5 + WAVE_COUNT))
        x = osc_wave_scanf(wavesAll[vco.wave - (5 + WAVE_COUNT)], x);
      else
#endif
        x = osc_wavebank(x, (uint32_t)(vco.wave - 5));
      break;
  }

  vco.phiold[index] = vco.lfo[index].phi0;
  vco.lfo[index].cycle();

  return clipminmaxf(0.f, x * vco.depth + vco.offset, 1.f);
}

static inline __attribute__((optimize("Ofast"), always_inline))
void note_on(uint32_t voice_idx) {
  s_phase[voice_idx] = 0.f;
  for (uint32_t i = 0; i < lfo_axes_count; i++) {
    set_vco_freq(i);
    if (s_vco[i].wave == 4)
      s_vco[i].snh[voice_idx] = osc_white();
    switch (s_vco[i].mode) {
      case lfo_mode_one_shot:
      case lfo_mode_key_trigger:
#if LFO_MODE_COUNT == 8
      case lfo_mode_key_trigger_plus_shape_lfo:
      case lfo_mode_one_shot_plus_shape_lfo:
#endif
        s_vco[i].lfo[voice_idx].reset();
        s_vco[i].phiold[voice_idx] = s_vco[i].lfo[voice_idx].phi0;
        break;
      case lfo_mode_random:
#if LFO_MODE_COUNT == 8
      case lfo_mode_random_plus_shape_lfo:
#endif
        s_vco[i].lfo[voice_idx].phi0 = f32_to_q31(osc_white());
      default:
        break;
    }
  }
}

#ifdef USER_TARGET_PLATFORM
void OSC_INIT(__attribute__((unused)) uint32_t platform, __attribute__((unused)) uint32_t api) {
#else
__unit_callback int8_t unit_init(const unit_runtime_desc_t * desc) {
  if (!desc)
    return k_unit_err_undef;
  if (desc->target != UNIT_HEADER_TARGET_FIELD)
    return k_unit_err_target;
  if (!UNIT_API_IS_COMPAT(desc->api))
    return k_unit_err_api_version;
  if (desc->samplerate != 48000)
    return k_unit_err_samplerate;
#ifndef UNIT_TARGET_PLATFORM_MICROKORG2
    if (desc->input_channels != UNIT_INPUT_CHANNELS || desc->output_channels != UNIT_OUTPUT_CHANNELS)
    return k_unit_err_geometry;
#endif
#ifdef UNIT_OSC_H_
  runtime_desc = desc;
#endif
#endif
#if LFO_WAVEFORM_COUNT == 159
  osc_wave_init_all();
#endif
#ifndef USER_TARGET_PLATFORM
  return k_unit_err_none;
#endif
}

#ifdef USER_TARGET_PLATFORM
void OSC_CYCLE(const user_osc_param_t * const runtime_context, int32_t * out, const uint32_t frames) {
#ifdef UNIT_TARGET_PLATFORM_NTS1
  uint16_t tempo = fx_get_bpm();
  if (s_tempo != tempo) {
    set_tempo(tempo);
    s_tempo = tempo;
  }
#endif
#else
__unit_callback void unit_render(const float * in, float * out, uint32_t frames) {
  (void) in;
#ifdef UNIT_TARGET_MODULE_OSC
  const unit_runtime_osc_context_t *runtime_context = (unit_runtime_osc_context_t *)runtime_desc->hooks.runtime_context;
#endif
#endif
#ifdef UNIT_TARGET_PLATFORM_MICROKORG2
//ToDo: microKORG2 support
  float32x4x2_t vpitch = {vld1q_f32(&PITCH[0]), vld1q_f32(&PITCH[4])};
  uint32x4x2_t vnote = {vcvtq_u32_f32(vpitch.val[0]), vcvtq_u32_f32(vpitch.val[1])};
  float32x4x2_t vw0 = {
    osc_w0f_for_notex4(vnote.val[0], vpitch.val[0] - vcvtq_f32_u32(vnote.val[0])),
    osc_w0f_for_notex4(vnote.val[1], vpitch.val[1] - vcvtq_f32_u32(vnote.val[1]))
  };
  out += runtime_context->bufferOffset + runtime_context->voiceOffset;
  for (uint32_t voice_idx = 0; voice_idx < runtime_context->voiceLimit; voice_idx++) {
    if (runtime_context->trigger & (1 << voice_idx)) {
      note_on(voice_idx);
    }
  }
#else
  float w0 = osc_w0f_for_note(PITCH >> 8, PITCH & 0xFF);
#endif
  unit_output_type_t * __restrict out_p = out;
  const unit_output_type_t * out_e;

  for (uint32_t i = 0; i < lfo_axes_count; i++) {
    s_vco[i].offset = .5f;
#ifdef UNIT_TARGET_PLATFORM_MICROKORG2
//ToDo: apply microKORG2 patch modulation per voice
    s_vco[i].offset += vModPatches[i].val[0][0] * .5f;
#elif defined(UNIT_TARGET_MODULE_OSC)
    if (s_vco[i].mode >= lfo_mode_one_shot_plus_shape_lfo) {
#if defined(UNIT_TARGET_PLATFORM_NTS1_MKII) || defined(UNIT_TARGET_PLATFORM_NTS1)
      if (runtime_context->shape_lfo | shape_lfo_old)
        s_vco[i].offset += q31_to_f32(runtime_context->shape_lfo) - .5f;
      shape_lfo_old = runtime_context->shape_lfo;
#else
      s_vco[i].offset += q31_to_f32(runtime_context->shape_lfo) * .5f;
#endif
    }
#endif
    if (s_vco[i].depth == 0.f)
      s_vco[i].offset += s_vco[i].shape - .5f;
  }

  if (s_mod_type != mod_type_none) {
    for (uint32_t stride_idx = 0; stride_idx < STRIDE_COUNT; stride_idx++) {
      out_p = out + stride_idx * STRIDE * (frames - 1);
      out_e = out_p + STRIDE * frames;
      for (; out_p != out_e; out_p += STRIDE) {
        for (uint32_t voice_idx = STRIDE * stride_idx; voice_idx < STRIDE * stride_idx + VOICE_LIMIT; voice_idx++) {  
          float mod = (get_vco(*s_vco_mod, voice_idx) + s_mod_offset) * s_mod_scale;
          float phase = s_phase[voice_idx];
          if (s_mod_type == mod_type_phase)
            phase += mod;
          float out_f = osc_wavebank(phase, get_vco(*s_vco_car, voice_idx) * (WAVE_COUNT - 1));
          if (s_mod_type < mod_type_phase)
            out_f *= mod;
#ifdef UNIT_TARGET_PLATFORM_NTS3_KAOSS
          out_f *= amp;
#endif
          out_p[voice_idx] = float_to_output(out_f);
#if UNIT_OUTPUT_CHANNELS == 2
          out_p[voice_idx + 1] = out_p[voice_idx];
          voice_idx++;
#endif
#ifndef UNIT_TARGET_PLATFORM_MICROKORG2
          s_phase[voice_idx] += w0;
          s_phase[voice_idx] -= (uint32_t)s_phase[voice_idx];
#endif
        }
#ifdef UNIT_TARGET_PLATFORM_MICROKORG2
        vphase.val[stride_idx] += vw0.val[stride_idx];
        vphase.val[stride_idx] -= vcvtq_f32_u32(vcvtq_u32_f32(vphase.val[stride_idx]));
#endif
      }
    }
  } else {
//ToDo: grid dimensions supports
    for (uint32_t stride_idx = 0; stride_idx < STRIDE_COUNT; stride_idx++) {
      out_p = out + stride_idx * STRIDE * (frames - 1);
      out_e = out_p + STRIDE * frames;
      for (; out_p != out_e; out_p += STRIDE) {
        for(uint32_t voice_idx = STRIDE * stride_idx; voice_idx < STRIDE * stride_idx + VOICE_LIMIT; voice_idx++) {  
          float out_f = osc_wavebank(s_phase[voice_idx], get_vco(s_vco[lfo_axis_x], voice_idx) * (WAVE_COUNT_X - 1), get_vco(s_vco[lfo_axis_y], voice_idx) * (WAVE_COUNT_Y - 1));
#ifdef UNIT_TARGET_PLATFORM_NTS3_KAOSS
          out_f *= amp;
#endif
          out_p[voice_idx] = float_to_output(out_f);
#if UNIT_OUTPUT_CHANNELS == 2
          out_p[voice_idx + 1] = out_p[voice_idx];
          voice_idx++;
#endif
#ifndef UNIT_TARGET_PLATFORM_MICROKORG2
          s_phase[voice_idx] += w0;
          s_phase[voice_idx] -= (uint32_t)s_phase[voice_idx];
#endif
        }
#ifdef UNIT_TARGET_PLATFORM_MICROKORG2
        vphase.val[stride_idx] += vw0.val[stride_idx];
        vphase.val[stride_idx] -= vcvtq_f32_u32(vcvtq_u32_f32(vphase.val[stride_idx]));
#endif
      }
    }
  }
}

#ifdef UNIT_TARGET_MODULE_OSC
#ifndef UNIT_TARGET_PLATFORM_MICROKORG2
#ifndef UNIT_OSC_H_
void OSC_NOTEON(__attribute__((unused)) const user_osc_param_t * const params) {
#else
__unit_callback void unit_note_on(uint8_t note, uint8_t velocity) {
  (void)note;
  (void)velocity;
#endif
  note_on(0);
}

#ifndef UNIT_OSC_H_
void OSC_NOTEOFF(const user_osc_param_t * const params) {
  (void)params;
#else
__unit_callback void unit_note_off(uint8_t note) {
  (void)note;
#endif
}
#endif

#ifdef UNIT_TARGET_PLATFORM_NTS1_MKII
__unit_callback void unit_all_note_off() {}

__unit_callback void unit_pitch_bend(uint16_t pitch_bend) {
  (void)pitch_bend;
}

__unit_callback void unit_channel_pressure(uint8_t pressure) {
  (void)pressure;
}

__unit_callback void unit_aftertouch(uint8_t note, uint8_t aftertouch) {
  (void)note;
  (void)aftertouch;
}
#endif
#endif

#ifdef USER_TARGET_PLATFORM
void OSC_PARAM(uint16_t index, uint16_t value) {
#else
__unit_callback void unit_set_param_value(uint8_t index, int32_t value) {
#ifdef UNIT_TARGET_PLATFORM_NTS1_MKII
  if (index < k_num_unit_osc_fixed_param_id)
    index += k_num_unit_osc_fixed_param_id;
#endif
  Params[index] = value;
#endif
  switch (index) {
#ifdef UNIT_TARGET_MODULE_OSC
    case param_lfo_rate_x:
    case param_lfo_rate_y:
      index -= param_lfo_rate_x;
      set_vco_rate(index, value);
      break;
#ifdef UNIT_OSC_H_
    case param_lfo_bpm_sync_x:
    case param_lfo_bpm_sync_y:
      update_vco_bpm(index - param_lfo_bpm_sync_x);
      break;
#endif
#else
    case param_lfo_bpm_sync_x:
      if (value < 0) {
        value -= value;
        index++;
      }
      update_vco_bpm(index - param_lfo_bpm_sync_x);
      break;
    case param_pitch:
      pitch = value << 5 & 0xFF00;
      break;
#endif
    case param_lfo_modes:
#ifdef USER_TARGET_PLATFORM
      value++;
#endif
      for (int32_t i = lfo_axes_count - 1; i >= 0; i--) {
        uint32_t newvalue;
#ifdef USER_TARGET_PLATFORM
        newvalue = value / 10;
        s_vco[i].mode = value - newvalue * 10 - 1;
        if (s_vco[i].mode >= LFO_MODE_COUNT)
          s_vco[i].mode = LFO_MODE_COUNT - 1;
#else
        newvalue = value / LFO_MODE_COUNT;
        s_vco[i].mode = value - newvalue * LFO_MODE_COUNT;
#endif
        value = newvalue;
      }
      break;
    case param_lfo_dimensions:
      if (value <= mod_type_phase) {
        s_mod_type = value;
        value = 0;
        s_vco_car = &s_vco[lfo_axis_x];
        s_vco_mod = &s_vco[lfo_axis_y];
      } else {
        s_mod_type = mod_type_none;
        value -= mod_type_phase;
      }
      if (value >= WAVE_COUNT_EXP) {
        s_mod_type = value - WAVE_COUNT_EXP;
        value = WAVE_COUNT_EXP;
        s_vco_car = &s_vco[lfo_axis_y];
        s_vco_mod = &s_vco[lfo_axis_x];
      }
      dimensionexp[lfo_axis_x] = WAVE_COUNT_EXP - value;
      dimensionexp[lfo_axis_y] = value;
      dimension[lfo_axis_x] = 1 << dimensionexp[lfo_axis_x];
      dimension[lfo_axis_y] = 1 << dimensionexp[lfo_axis_y];
      if (s_mod_type != mod_type_none) {
//Amplitude Modulation - keep unipolar [0..1]
//Ring Modulation - convert to bipolar [-1..1]
//Phase Modulation - convert to unipolar [0..2] to keep positive phase wave sample lookup
        static const float s_mod_offsets[] = {0.f, -.5f, 0.f};
        static const float s_mod_scales[] = {1.f, 2.f, 2.f};
        s_mod_offset = s_mod_offsets[s_mod_type];
        s_mod_scale = s_mod_scales[s_mod_type];
      }
      break;
    case param_lfo_waveform_x:
    case param_lfo_waveform_y:
      index -= param_lfo_waveform_x;
#ifdef USER_TARGET_PLATFORM
      value = value == 0 ? 0 : value < 100 ? (104 + WAVE_COUNT - value) : (value - 100);
#endif
      s_vco[index].wave = value;
      break;
    case param_lfo_depth_x:
    case param_lfo_depth_y:
      index -= param_lfo_depth_x;
#ifdef USER_TARGET_PLATFORM
      value -= 100;
#endif
      s_vco[index].depth = (int16_t)value * .005f;
      break;
  }
}

#ifdef UNIT_TARGET_PLATFORM
__unit_callback int32_t unit_get_param_value(uint8_t index) {
#ifdef UNIT_TARGET_PLATFORM_NTS1_MKII
  if (index < k_num_unit_osc_fixed_param_id)
    index += k_num_unit_osc_fixed_param_id;
#endif
  return Params[index];
}

__unit_callback const char * unit_get_param_str_value(uint8_t index, int32_t value) {
#ifdef UNIT_TARGET_PLATFORM_MICROKORG2
  static const char *modes[] = {"{One shot", "{Key trigger", "{Random", "{Free run"};
  static char modename[25];
  static const char *dimensions[] = {"|64{X|Amp", "|64{X|Ring", "|64{X|Phase", "32 X 2", "16 X 4", "8 X 8", "4 X 16", "2 X 32", "|Amp{X|64", "|Ring{X|64", "|Phase{X|64"};
  static const char *wfnames[] = {"Saw", "Triangle", "Square", "Sine", "|Sample{&|Hold"};
  static const char *wtnames[] = {"{Wavetable}", "{Wave bank}A ", "{Wave bank}B ", "{Wave bank}C ", "{Wave bank}D ", "{Wave bank}E ", "{Wave bank}F "};
#else
#if LFO_MODE_COUNT == 8
  static const char modes[] = "1 T R F 1STSRSFS";
#else
  static const char modes[] = "1 T R F ";
#endif
  static char modename[lfo_axes_count * 2 + 1];
  static const char *dimensions[] = {"64 A", "64 R", "64 P", "32 2", "16 4", "8 8", "4 16", "2 32", "A 64", "R 64", "P 64"};
  static const char *wfnames[] = {"Saw", "Tri", "Sqr", "Sin", "SnH"};
  static char wtnames[][5] = {"WT  ", "WA  ", "WB  ", "WC  ", "WD  ", "WE  ", "WF  "};
#endif
  static const uint8_t wfcounts[] = {WAVE_COUNT, 16, 16, 14, 13, 15, 16};
  static char *s;
  value = (int16_t)value;
  switch (index) {
#ifdef UNIT_TARGET_PLATFORM_MICROKORG2
    case param_lfo_rate_x:
    case param_lfo_rate_y:
      index -= param_lfo_rate_x;
      if (s_vco[index].depth == 0.f) {
        if (dimensionexp[index] == 0) {
//ToDo: values for ring and phase mod
          sprintf(modename, "%.2f|db", ampdbf(param_val_to_f32(value)));
        } else {       
          sprintf(modename, "%.3f", (dimension[index] - 1) * param_val_to_f32(value) + 1.f);
        }
      } else if (s_vco[index].bpmfreq == 0.f) {
        sprintf(modename, "%.3f|Hz", (dbampf(param_val_to_f32(value) * LFO_RATE_LOG_BIAS) - 1.f) * LFO_MAX_RATE);
      } else {
        sprintf(modename, "%02u.%01u|bar", (uint16_t)value >> 4, (uint16_t)value >> 2 & 0x3);
      }
      return modename;
#endif
    case param_lfo_modes:
      modename[0] = 0;
      for (int32_t i = lfo_axes_count - 1; i >= 0; i--) {
        uint32_t newvalue = value / LFO_MODE_COUNT;
        uint32_t idx = value - newvalue * LFO_MODE_COUNT;
        value = newvalue;
#ifdef UNIT_TARGET_PLATFORM_MICROKORG2
        strcat(modename, modes[idx]);
#else
        modename[i * 2] = modes[idx * 2];
        modename[i * 2 + 1] = modes[idx * 2 + 1];
#endif
      }
      return modename;
    case param_lfo_dimensions:
      return dimensions[value];
    case param_lfo_waveform_x:
    case param_lfo_waveform_y:
      if (value < 5)
        return wfnames[value];
      value -= 5;
      for (uint32_t i = 0; i < sizeof(wfcounts); i++) {
        if (value < wfcounts[i]) {
          value++;
          uint32_t j = value / 10;
#ifdef UNIT_TARGET_PLATFORM_MICROKORG2
          char wfname[16];
          char wfnum[3];
          s = wfnum;
          if (j > 0)
            *s++ = '0' + j;
          *s = '0' + (value - j * 10);
          strcpy(wfname, wtnames[i]);
          strcat(wfname, wfnum);
          s = wfname;
#else
          s = wtnames[i];
          s[2] = '0' + j;
          s[3] = '0' + (value - j * 10);
#endif
          break;
        }
        value -= wfcounts[i];
      }
      return s;
  }
  return nullptr;
}

__unit_callback void unit_set_tempo(uint32_t tempo) {
  set_tempo(tempo);
}

#ifndef UNIT_TARGET_PLATFORM_MICROKORG2
__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter) {
  (void)counter;
}
#endif

__unit_callback void unit_reset() {}

__unit_callback void unit_resume() {}

__unit_callback void unit_suspend() {}
#endif

#ifdef UNIT_TARGET_PLATFORM_NTS3_KAOSS
__unit_callback void unit_touch_event(uint8_t id, uint8_t phase, uint32_t x, uint32_t y) {
  (void)id;
  switch(phase) {
    case k_unit_touch_phase_began:
      note_on(0);
      amp = 1.f;
//fall through
    case k_unit_touch_phase_moved:
    case k_unit_touch_phase_stationary:
      set_vco_rate(lfo_axis_x, x);
      set_vco_rate(lfo_axis_y, y);
      break;
    case k_unit_touch_phase_ended:
    case k_unit_touch_phase_cancelled:
      amp = 0.f;
      break;
  }
}
#endif

#ifdef UNIT_TARGET_PLATFORM_MICROKORG2
__unit_callback void unit_platform_exclusive(uint8_t messageId, void * data, uint32_t dataSize) {
  (void)dataSize;
  const unit_runtime_osc_context_t *runtime_context = (unit_runtime_osc_context_t *)runtime_desc->hooks.runtime_context;
  switch (messageId) {
    case kMk2PlatformExclusiveModData: {
      const mk2_mod_data_t *mod_data = (mk2_mod_data_t *)data;
      const uint32_t timbre_idx = 0;
//ToDo: TBC if voiceOffset must be respected
//ToDo: TBC if timbre index must be respected
      switch (runtime_context->voiceLimit) {
        case kMk2MaxVoices:
          vModPatches[0].val[0] = vmulq_n_f32(vld1q_f32(&mod_data->data[0]), mod_data->depth[timbre_idx][0]);
          vModPatches[0].val[1] = vmulq_n_f32(vld1q_f32(&mod_data->data[4]), mod_data->depth[timbre_idx][0]);
          vModPatches[1].val[0] = vmulq_n_f32(vld1q_f32(&mod_data->data[kMk2MaxVoices]), mod_data->depth[timbre_idx][1]);
          vModPatches[1].val[1] = vmulq_n_f32(vld1q_f32(&mod_data->data[kMk2MaxVoices + 4]), mod_data->depth[timbre_idx][1]);
          break;
        case kMk2HalfVoices:
          vModPatches[0].val[0] = vmulq_n_f32(vld1q_f32(&mod_data->data[0]), mod_data->depth[timbre_idx][0]);
          vModPatches[1].val[0] = vmulq_n_f32(vld1q_f32(&mod_data->data[kMk2HalfVoices]), mod_data->depth[timbre_idx][1]);
          break;
        case kMk2QuarterVoices: {
          float32x2x2_t data = vld2_f32(mod_data->data);
          float32x2_t depth = vld1_f32(mod_data->depth[timbre_idx]);
          vst2_f32((float *)vModPatches, (float32x2x2_t){data.val[0] * depth, data.val[1] * depth});
          break;
        }
        case kMk2SingleVoice: {
          float32x2_t data = vld1_f32(mod_data->data);
          float32x2_t depth = vld1_f32(mod_data->depth[timbre_idx]);
          vst1_f32((float *)vModPatches, data * depth);
          break;
        }
      }
      break;
    }
    case kMk2PlatformExclusiveModDestName: {
      mk2_mod_dest_name_t *mod_dest = (mk2_mod_dest_name_t *)data;
      if (mod_dest->index < MOD_PATCHES_COUNT)
        strcpy(mod_dest->name, unit_header.params[mod_dest->index].name);
      break;
    }  
/*ToDo: TBC message purpose and data format
    case kMk2PlatformExclusiveUserModSource:
      mk2_user_mod_source_t *user_mod_source = (mk2_user_mod_source_t *)data;
      break;
*/
  }
}
#endif
