/*
 *  File: morpheus.cc
 *
 *  Morphing wavetable oscillator
 * 
 *  2020-2025 (c) Oleg Burdaev
 *  mailto: dukesrg@gmail.com
 */

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
  lfo_axis_x = 0U,
  lfo_axis_y
};

enum {
  lfo_mod_amp,
  lfo_mod_ring,
  lfo_mod_phase,
  lfo_mod_none
};

typedef struct {
  uint32_t mode;
  uint32_t wave;
  uint32_t dimensionexp;  
  uint32_t dimension;
  uint32_t modulation;
  float depth;
#ifdef UNIT_TARGET_PLATFORM
  float bpmfreq;
#endif
  float freq;  
  float shape;
  float snh;
  float offset;
  dsp::SimpleLFO lfo;
  q31_t phiold;
} vco_t;

static vco_t s_vco[LFO_AXES_COUNT];
static float s_phase[VOICE_COUNT] = {0.f};

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
  param_lfo_rate_x = 0U,
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
static float s_bpmfreq;
#endif

#ifdef UNIT_OSC_H_
const unit_runtime_desc_t *runtime_desc;
#else
uint16_t pitch = 0x3C00;
float amp;
#endif

#ifdef UNIT_TARGET_MODULE_OSC
#define PITCH runtime_context->pitch
#else
#define PITCH pitch
#endif

#if defined(UNIT_TARGET_PLATFORM_NTS1_MKII) || defined(UNIT_TARGET_PLATFORM_NTS1)
q31_t shape_lfo_old;
#endif

#ifdef UNIT_TARGET_PLATFORM_MICROKORG2
#define MOD_PATCHES_COUNT 2
static float32x4x2_t vModPatches[MOD_PATCHES_COUNT];
#define STRIDE kMk2HalfVoices
#define STRIDE_COUNT ((uint32_t)runtime_context->voiceLimit >> 2)
#else
#define STRIDE UNIT_OUTPUT_CHANNELS
#define STRIDE_COUNT 1
#endif

static inline __attribute__((optimize("Ofast"), always_inline))
void set_vco_freq(uint32_t index) {
    s_vco[index].lfo.setF0(
#ifdef UNIT_TARGET_PLATFORM
      s_vco[index].bpmfreq > 0.f ? s_vco[index].bpmfreq :
#endif
    s_vco[index].freq, k_samplerate_recipf);
}

static inline __attribute__((optimize("Ofast"), always_inline))
void set_vco_rate(uint32_t index, uint32_t value) {
#ifdef UNIT_TARGET_PLATFORM_MICROKORG2
//ToDo: apply microKORG2 path modulation to X/Y Value
  s_vco[index].shape = param_val_to_f32(value) + vModPatches[index].val[0][index];
#else
  s_vco[index].shape = param_val_to_f32(value);
#endif
  s_vco[index].freq = (fasterdbampf(s_vco[index].shape * LFO_RATE_LOG_BIAS) - 1.f) * LFO_MAX_RATE;
  set_vco_freq(index);
}

#ifdef UNIT_TARGET_PLATFORM
static inline __attribute__((optimize("Ofast"), always_inline))
void set_vco_bpm(uint32_t index, uint32_t value) {
  s_vco[index].bpmfreq = s_bpmfreq * value;
  set_vco_freq(index);
}
#endif

static inline __attribute__((optimize("Ofast"), always_inline))
float get_vco(vco_t &vco) {
  float x;

  if ((vco.mode == lfo_mode_one_shot
#if LFO_MODE_COUNT == 8
  || vco.mode == lfo_mode_one_shot_plus_shape_lfo
#endif
  ) && vco.phiold > 0 && vco.lfo.phi0 <= 0) {
    vco.lfo.phi0 = 0x7FFFFFFF;
    vco.lfo.w0 = 0;
  }

  switch (vco.wave) {
    case 0:
      x = vco.lfo.saw_bi();
     break;
    case 1:
      x = vco.lfo.triangle_bi();
      break;
    case 2:
      x = vco.lfo.square_bi();
      break;
    case 3:
      x = vco.lfo.sine_bi();
      break;
    case 4:
      if (vco.phiold > 0 && vco.lfo.phi0 <= 0)
        vco.snh = osc_white();
      x = vco.snh;
      break;
    default:
      x = q31_to_f32(vco.lfo.phi0) * .5f + .5f;
#if LFO_WAVEFORM_COUNT == 159
      if (vco.wave >= (5 + WAVE_COUNT))
        x = osc_wave_scanf(wavesAll[vco.wave - (5 + WAVE_COUNT)], x);
      else
#endif
        x = osc_wavebank(x, (uint32_t)(vco.wave - 5));
      break;
  }

  vco.phiold = vco.lfo.phi0;
  vco.lfo.cycle();

  return clipminmaxf(0.f, x * vco.depth + vco.offset, 1.f);
}

static inline __attribute__((optimize("Ofast"), always_inline))
void note_on(uint32_t voice_idx) {
  s_phase[voice_idx] = 0.f;
  for (uint32_t i = 0; i < LFO_AXES_COUNT; i++) {
    set_vco_freq(i);
    if (s_vco[i].wave == 4)
      s_vco[i].snh = osc_white();
    switch (s_vco[i].mode) {
      case lfo_mode_one_shot:
      case lfo_mode_key_trigger:
#if LFO_MODE_COUNT == 8
      case lfo_mode_key_trigger_plus_shape_lfo:
      case lfo_mode_one_shot_plus_shape_lfo:
#endif
        s_vco[i].lfo.reset();
        s_vco[i].phiold = s_vco[i].lfo.phi0;
        break;
      case lfo_mode_random:
#if LFO_MODE_COUNT == 8
      case lfo_mode_random_plus_shape_lfo:
#endif
        s_vco[i].lfo.phi0 = f32_to_q31(osc_white());
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
#else
__unit_callback void unit_render(const float * in, float * out, uint32_t frames) {
  (void) in;
#ifdef UNIT_TARGET_MODULE_OSC
  const unit_runtime_osc_context_t *runtime_context = (unit_runtime_osc_context_t *)runtime_desc->hooks.runtime_context;
#endif
#endif
  unit_output_type_t * __restrict out_p;
#ifdef UNIT_TARGET_PLATFORM_MICROKORG2
//ToDo: microKORG2 support
  float32x4x2_t *vpitch = (float32x4x2_t *)&PITCH;
  uint32x4x2_t vnote = {vcvtq_u32_f32(vpitch->val[0]), vcvtq_u32_f32(vpitch->val[1])};
  float32x4x2_t vw0 = {
    osc_w0f_for_notex4(vnote.val[0], vpitch->val[0] - vcvtq_f32_u32(vnote.val[0])),
    osc_w0f_for_notex4(vnote.val[1], vpitch->val[1] - vcvtq_f32_u32(vnote.val[1]))
  };
//  float *w0 = (float *)&vw0;
  float32x4x2_t *vphase = (float32x4x2_t *)s_phase;
  unit_output_type_t * __restrict out_p;
  for (uint32_t voice_idx = 0; voice_idx < runtime_context->voiceLimit; voice_idx++) {
//ToDo: TBC microKORG2 trigger field behaviour, bit order, voiceOffset and voiceLimit dependency
    if (runtime_context->trigger & (1 << voice_idx)) {
      note_on(voice_idx);
    }
//ToDo: microKORG2 2x4 voices buffer alignment       
    out_p = out + runtime_context->bufferOffset + (runtime_context->voiceOffset & 3) + (voice_idx >> 2) * (frames << 2);
  }
#else
  float w0 = osc_w0f_for_note(PITCH >> 8, PITCH & 0xFF);
  out_p = out;
#endif
  const unit_output_type_t * out_e = out_p + frames * STRIDE;  

  for (uint32_t i = 0; i < LFO_AXES_COUNT; i++) {
    s_vco[i].offset = .5f;
#if defined(UNIT_TARGET_MODULE_OSC) && !defined(UNIT_TARGET_PLATFORM_MICROKORG2)
    if (s_vco[i].mode >= lfo_mode_one_shot_plus_shape_lfo) {
#if defined(UNIT_TARGET_PLATFORM_NTS1_MKII) || defined(UNIT_TARGET_PLATFORM_NTS1)
//ToDo: unipolar LFO for AMP mod
      if (runtime_context->shape_lfo | shape_lfo_old)
        s_vco[i].offset += q31_to_f32(runtime_context->shape_lfo) - .5f;
      shape_lfo_old = runtime_context->shape_lfo;
#else
      if (s_vco[i].modulation == lfo_mod_amp)
        s_vco[i].offset += q31_to_f32(runtime_context->shape_lfo) * .5f;
#endif
    }
#endif
    if (s_vco[i].depth == 0.f)
      s_vco[i].offset += s_vco[i].shape - .5f;
  }

  if (s_vco[lfo_axis_x].dimensionexp == 0 || s_vco[lfo_axis_y].dimensionexp == 0) {
    uint32_t wave_axis;
    uint32_t mod_axis;
    if (s_vco[lfo_axis_x].modulation == lfo_mod_none) {
      wave_axis = lfo_axis_x;
      mod_axis = lfo_axis_y;
    } else {
      wave_axis = lfo_axis_y;
      mod_axis = lfo_axis_x;
    }

    float mod_offset = -.5f;
    float mod_scale = 2.f;
    if (s_vco[mod_axis].modulation == lfo_mod_amp) {
      if (s_vco[mod_axis].depth > 0.f)
        mod_offset += s_vco[mod_axis].depth;
      else 
        mod_offset -= s_vco[mod_axis].depth;
      mod_scale = 1.f;
    }
//ToDo: microKORG2 2x4 voices buffer alignment
//    for (uint32_t stride_idx = 0; stride_idx < STRIDE_COUNT; stride_idx++) {
//      out_p = out + STRIDE_COUNT * STRIDE * frames;
    for (; out_p != out_e; out_p += STRIDE) {
      for(uint32_t voice_idx = 0; voice_idx < VOICE_COUNT; voice_idx++) {  
        float mod = (get_vco(s_vco[mod_axis]) + mod_offset) * mod_scale;
        float phase = s_phase[voice_idx];
        if (s_vco[mod_axis].modulation == lfo_mod_phase)
          phase += mod + 1.f;
        float out_f = osc_wavebank(phase, get_vco(s_vco[wave_axis]) * (WAVE_COUNT - 1));
        if (s_vco[mod_axis].modulation < lfo_mod_phase)
          out_f *= mod;
#ifdef UNIT_TARGET_PLATFORM_NTS3_KAOSS
        out_f *= amp;
#endif
        out_p[0] = float_to_output(out_f);
#ifndef UNIT_TARGET_PLATFORM_MICROKORG2
        s_phase[0] += w0;
        s_phase[0] -= (uint32_t)s_phase[0];
#endif
      }
#ifdef UNIT_TARGET_PLATFORM_MICROKORG2
      vphase->val[0] += vw0.val[0];
      vphase->val[1] += vw0.val[1];
      vphase->val[0] -= vcvtq_f32_u32(vcvtq_u32_f32(vphase->val[0]));
      vphase->val[1] -= vcvtq_f32_u32(vcvtq_u32_f32(vphase->val[1]));
#endif
//    }
    }
  } else {
//ToDo: microKORG2 2x4 voices buffer alignment       
    for (; out_p != out_e; out_p += STRIDE) {
      for(uint32_t voice_idx = 0; voice_idx < VOICE_COUNT; voice_idx++) {  
//ToDo: variable grid mode
        float out_f = osc_wavebank(s_phase[voice_idx], get_vco(s_vco[lfo_axis_x]) * (WAVE_COUNT_X - 1), get_vco(s_vco[lfo_axis_y]) * (WAVE_COUNT_Y - 1));
#ifdef UNIT_TARGET_PLATFORM_NTS3_KAOSS
        out_f *= amp;
#endif
        out_p[0] = float_to_output(out_f);
#if UNIT_OUTPUT_CHANNELS == 2
        out_p[1] = out_p[0];
#endif
#ifndef UNIT_TARGET_PLATFORM_MICROKORG2
        s_phase[0] += w0;
        s_phase[0] -= (uint32_t)s_phase[0];
#endif
      }
#ifdef UNIT_TARGET_PLATFORM_MICROKORG2
      vphase->val[0] += vw0.val[0];
      vphase->val[1] += vw0.val[1];
      vphase->val[0] -= vcvtq_f32_u32(vcvtq_u32_f32(vphase->val[0]));
      vphase->val[1] -= vcvtq_f32_u32(vcvtq_u32_f32(vphase->val[1]));
#endif
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
      set_vco_bpm(index - param_lfo_bpm_sync_x, value);
      break;
#endif
#else
    case param_lfo_bpm_sync_x:
      if (value < 0) {
        value -= value;
        index++;
      }
      set_vco_bpm(index - param_lfo_bpm_sync_x, value);
      break;
    case param_pitch:
      pitch = value << 5 & 0xFF00;
      break;
#endif
    case param_lfo_modes:
#ifdef USER_TARGET_PLATFORM
      value++;
#endif
      for (int32_t i = LFO_AXES_COUNT; i >= 0; i--) {
        uint32_t newvalue;
#ifdef USER_TARGET_PLATFORM
        newvalue = value / 10;
        s_vco[i].mode = value - newvalue * 10 - 1;
        if (s_vco[i].mode >= LFO_MODE_COUNT)
          s_vco[i].mode = LFO_MODE_COUNT;
#else
        newvalue = value / LFO_MODE_COUNT;
        s_vco[i].mode = value - newvalue * LFO_MODE_COUNT;
#endif
        value = newvalue;
      }
      break;
    case param_lfo_dimensions:
      if (value <= lfo_mod_phase) {
        s_vco[lfo_axis_y].modulation = value;
        value = 0;
      } else {
        s_vco[lfo_axis_y].modulation = lfo_mod_none;
        value -= lfo_mod_phase;
      }
      if (value >= WAVE_COUNT_EXP) {
        s_vco[lfo_axis_x].modulation = value - WAVE_COUNT_EXP;
        value = WAVE_COUNT_EXP;
      } else {
        s_vco[lfo_axis_x].modulation = lfo_mod_none;
      }
      s_vco[lfo_axis_x].dimensionexp = WAVE_COUNT_EXP - value;
      s_vco[lfo_axis_y].dimensionexp = value;
      s_vco[lfo_axis_x].dimension = 2 << s_vco[lfo_axis_x].dimensionexp;
      s_vco[lfo_axis_y].dimension = 2 << s_vco[lfo_axis_y].dimensionexp;
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
  return Params[index];
}

__unit_callback const char * unit_get_param_str_value(uint8_t index, int32_t value) {
#if LFO_MODE_COUNT == 8
  static const char modes[] = "1 T R F 1STSRSFS";
#else
  static const char modes[] = "1 T R F ";
#endif
#ifdef UNIT_TARGET_PLATFORM_MICROKORG2
//ToDo: TBC microKORG2 param strings constraints
  static const char dimensions[][12] = {"64}x{Amp", "64}x{Ring", "64}x{Phase", "32}x|2", "16}x|4", "8}x|8", "4}x|16", "2}x|32", "{Amp}x|64", "{Ring}x|64", "{Phase}x|64"};
#else
  static const char dimensions[][5] = {"64 A", "64 R", "64 P", "32 2", "16 4", "8 8", "4 16", "2 32", "A 64", "R 64", "P 64"};
#endif
  static char modename[LFO_AXES_COUNT * 2 + 1];
  static char wfnames[][4] = {"Saw", "Tri", "Sqr", "Sin", "SnH"};
  static char wtnames[][5] = {"WT  ", "WA  ", "WB  ", "WC  ", "WD  ", "WE  ", "WF  "};
  static const uint8_t wfcounts[] = {WAVE_COUNT, 16, 16, 14, 13, 15, 16};
  static char *s;
  value = (int16_t)value;
  switch (index) {
    case param_lfo_rate_x:
    case param_lfo_rate_y:
//ToDo: Rate / position string representation
      break;
    case param_lfo_modes:
      for (int32_t i = LFO_AXES_COUNT; i >= 0; i--) {
        uint32_t newvalue = value / LFO_MODE_COUNT;
        uint32_t idx = value - newvalue * LFO_MODE_COUNT;
        value = newvalue;
        modename[i * 2] = modes[idx * 2];
        modename[i * 2 + 1] = modes[idx * 2 + 1];
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
        s = wtnames[i];
        if (value < wfcounts[i]) {
          value++;
          uint32_t j = value / 10;
          s[2] = '0' + j;
          s[3] = '0' + (value - j * 10);
          break;
        }
        value -= wfcounts[i];
      }
      return s;
  }
  return nullptr;
}

__unit_callback void unit_set_tempo(uint32_t tempo) {
  s_bpmfreq = tempo == 0 ? 0.f : (3932160.f / tempo); // 60 * (2<<16) / tempo
  set_vco_bpm(lfo_axis_x, Params[param_lfo_bpm_sync_x]);
  set_vco_bpm(lfo_axis_y, Params[param_lfo_bpm_sync_y]);
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
      mk2_mod_data_t *mod_data = (mk2_mod_data_t *)data;
      for (uint32_t i = 0; i < MOD_PATCHES_COUNT; i++) {
//__asm__ volatile ( "nop\nnop\nnop\nnop\n");
//ToDo: TBC if voiceOffset must be respected
        for (uint32_t j = 0; j < runtime_context->voiceLimit; j++)
          ((float *)&vModPatches[i])[j] = mod_data->data[runtime_context->voiceLimit * i + j];
        vModPatches[i].val[0] *= mod_data->depth[0][i];
        vModPatches[i].val[1] *= mod_data->depth[0][i];
//__asm__ volatile ( "nop\nnop\nnop\nnop\n");
      }
      break;
    }
    case kMk2PlatformExclusiveModDestName: {
      mk2_mod_dest_name_t *mod_dest = (mk2_mod_dest_name_t *)data;
      if (mod_dest->index < MOD_PATCHES_COUNT) {
        for (uint32_t i = 0; i < UNIT_PARAM_NAME_LEN; i++)
          mod_dest->name[i] = unit_header.params[mod_dest->index].name[i];
      }
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
