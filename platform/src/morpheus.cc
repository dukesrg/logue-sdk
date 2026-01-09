/*
 *  File: morpheus.cc
 *
 *  Morphing wavetable oscillator
 * 
 *  2020-2026 (c) Oleg Burdaev
 *  mailto: dukesrg@gmail.com
 */

#include <string.h>
#include <stdio.h>
#include "logue_wrap.h"
#include "logue_perf.h"

#include "fixed_math.h"
#include "simplelfo.hpp"
#include "osc_apiq.h"

#define SAMPLE_COUNT 256
#define WAVE_COUNT 64
#define WAVE_COUNT_X 8
#define WAVE_COUNT_Y 8
#ifdef UNIT_TARGET_PLATFORM_MICROKORG2
#define WAVEBANK_ALLOCATE
#endif
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
#ifdef SHAPE_LFO_SUPPORTED
  lfo_mode_one_shot_plus_shape_lfo,
  lfo_mode_key_trigger_plus_shape_lfo,
  lfo_mode_random_plus_shape_lfo,
  lfo_mode_free_run_plus_shape_lfo,
#endif
  lfo_mode_count = LFO_MODE_COUNT
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

#ifdef BPM_SYNC_SUPPORTED
static float s_bpmfreq;
static uint16_t s_lfo_bpm_sync[lfo_axes_count];
#ifdef UNIT_TARGET_PLATFORM_NTS1
#define BPM_SYNC_SCALE .0016666667f // 0.1 beat / 60 sec                          
#include "fx_api.h"
//ToDo: NTS-1 LFO sync parameters support
#else
#define BPM_SYNC_SCALE .25431316e-6f // 1 / 2^16 / 60 sec
#endif 
#endif

#ifdef WAVETABLE_FILE_SUPPORTED
#include "logue_fs.h"
#include "wav_fmt.h"
//ToDo: redefine path for wavetable files
#include "SystemPaths.h"
const char *wavetable_file_path = looperPath;
const char wavetable_file_prefix[4] = "";
const char *wavetable_file_suffix = ".wav";
static fs_dir wavetable_file_list = fs_dir(wavetable_file_path, wavetable_file_prefix, wavetable_file_suffix);
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
  float out_value[VOICE_COUNT];
  bool out_valid[VOICE_COUNT];

  fast_inline void update_freq() {
    for (uint32_t voice_idx = 0; voice_idx < VOICE_COUNT; voice_idx++)
      lfo[voice_idx].setF0(
  #ifdef BPM_SYNC_SUPPORTED
        bpmfreq > 0.f ? bpmfreq :
  #endif
      freq, k_samplerate_recipf);
  }

  fast_inline void force_recalc() {
    for (uint32_t voice_idx = 0; voice_idx < VOICE_COUNT; voice_idx++)
      out_valid[voice_idx] = false;
  }

  fast_inline void set_mode(uint32_t value) {
    mode = value;
    force_recalc();
  }

  fast_inline void set_wave(uint32_t value) {
    wave = value;
    force_recalc();
  }

  fast_inline void set_depth(float value) {
    depth = value;
    force_recalc();
  }

  #ifdef BPM_SYNC_SUPPORTED
  fast_inline void set_bpmfreq(float value) {
    bpmfreq = value;
    update_freq();
  }
#endif

  fast_inline void set_shape(uint32_t value) {
    shape = param_val_to_f32(value);
    freq = (fasterdbampf(shape * LFO_RATE_LOG_BIAS) - 1.f) * LFO_MAX_RATE;
    update_freq();
    force_recalc();
  }

  fast_inline void set_offset(float value) {
    offset = value;
    force_recalc();
  }

  fast_inline void reset(uint32_t voice_idx) {
    update_freq();
    if (wave == 4)
      snh[voice_idx] = osc_white();
    switch (mode) {
      case lfo_mode_one_shot:
      case lfo_mode_key_trigger:
#ifdef SHAPE_LFO_SUPPORTED
      case lfo_mode_key_trigger_plus_shape_lfo:
      case lfo_mode_one_shot_plus_shape_lfo:
#endif
        lfo[voice_idx].reset();
        phiold[voice_idx] = lfo[voice_idx].phi0;
        break;
      case lfo_mode_random:
#ifdef SHAPE_LFO_SUPPORTED
      case lfo_mode_random_plus_shape_lfo:
#endif
        lfo[voice_idx].phi0 = f32_to_q31(osc_white());
      default:
        break;
    }
    out_valid[voice_idx] = false;
  }

  fast_inline void advance(uint32_t samples) {
    for (uint32_t voice_idx = 0; voice_idx < VOICE_COUNT; voice_idx++) {
      phiold[voice_idx] = lfo[voice_idx].phi0;
      lfo[voice_idx].phi0 += lfo[voice_idx].w0 * samples;
      if ((mode == lfo_mode_one_shot
    #ifdef SHAPE_LFO_SUPPORTED
      || mode == lfo_mode_one_shot_plus_shape_lfo
    #endif
      ) && phiold[voice_idx] > 0 && lfo[voice_idx].phi0 <= 0) {
        lfo[voice_idx].phi0 = 0x7FFFFFFF;
        lfo[voice_idx].w0 = 0;
      }
      out_valid[voice_idx] = false;
    }
  }

  fast_inline void cycle() {    
    advance(1);
  }

  fast_inline float out(uint32_t voice_idx) {
    if (!out_valid[voice_idx]) {
      float out;
      if (depth == 0.f) {
        out = offset + shape - .5f;
      } else {
        switch (wave) {
          case 0:
            out = lfo[voice_idx].saw_bi();
          break;
          case 1:
            out = lfo[voice_idx].triangle_bi();
            break;
          case 2:
            out = lfo[voice_idx].square_bi();
            break;
          case 3:
            out = lfo[voice_idx].sine_bi();
            break;
          case 4:
            if (phiold[voice_idx] > 0 && lfo[voice_idx].phi0 <= 0)
              snh[voice_idx] = osc_white();
            out = snh[voice_idx];
            break;
          default:
            out = q31_to_f32(lfo[voice_idx].phi0) * .5f + .5f;
#if LFO_WAVEFORM_COUNT == 159
            if (wave >= (5 + WAVE_COUNT))
              out = osc_wave_scanf(wavesAll[wave - (5 + WAVE_COUNT)], out);
            else
#endif
            out = osc_wavebank(out, (uint32_t)(wave - 5));
          break;
        }
        out = out * depth + offset;
      }
      out = clipminmaxf(0.f, out, 1.f);
      out_value[voice_idx] = out;
      out_valid[voice_idx] = true;
    }
    return out_value[voice_idx];
  }
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
#ifdef BPM_SYNC_SUPPORTED
//ToDo: NTS-1 BPM sync params
  param_lfo_bpm_sync_x = k_num_user_osc_param_id,
  param_lfo_bpm_sync_y,
#endif
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
#ifdef UNIT_TARGET_PLATFORM_NTS3_KAOSS
  param_pitch,
#endif
#ifdef UNIT_TARGET_PLATFORM_MICROKORG2
  param_wavetable_file_idx,
#endif
#ifdef BPM_SYNC_SUPPORTED
  param_lfo_bpm_sync_x,
#ifdef UNIT_TARGET_PLATFORM_NTS3_KAOSS
  param_lfo_bpm_sync_y = param_lfo_bpm_sync_x,
#else
  param_lfo_bpm_sync_y,
#endif
#endif
#endif
  param_num
};

#ifdef UNIT_TARGET_PLATFORM
static int32_t Params[PARAM_COUNT];
#endif

#ifdef UNIT_TARGET_MODULE_OSC
#ifdef UNIT_OSC_H_
const unit_runtime_osc_context_t *runtime_context;
#endif
#define PITCH runtime_context->pitch
#else
float amp;
uint16_t pitch = 0x3C00;
#define PITCH pitch
#endif

#ifdef UNIT_TARGET_PLATFORM_MICROKORG2
#define MOD_PATCHES_COUNT 2
static float32x4x2_t vModPatches[MOD_PATCHES_COUNT];
#define STRIDE (runtime_context->outputStride)
#define STRIDE_COUNT (((uint32_t)runtime_context->voiceLimit >> 3) + 1)
#define VOICE_LIMIT (((runtime_context->voiceLimit - 1) & (STRIDE - 1)) + 1)
float32x4x2_t vphase;
float *s_phase = (float *)&vphase;
uint32_t __attribute__((aligned(32))) vphase_u32[VOICE_COUNT];
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

#ifdef BPM_SYNC_SUPPORTED
static fast_inline void set_tempo(uint32_t tempo) {
  static uint32_t tempo_old;
  if (tempo != tempo_old) {
    s_bpmfreq = tempo == 0 ? 0.f : (BPM_SYNC_SCALE / tempo);
    s_vco[lfo_axis_x].set_bpmfreq(s_bpmfreq * s_lfo_bpm_sync[lfo_axis_x]);
    s_vco[lfo_axis_y].set_bpmfreq(s_bpmfreq * s_lfo_bpm_sync[lfo_axis_y]);
    tempo_old = tempo;
  }
}
#endif

#ifdef WAVETABLE_FILE_SUPPORTED
static fast_inline float *load_wavetable_data(uint32_t index) {
  float *wavetable_ptr = nullptr;
  void *rawdata_ptr = nullptr;
  float * dst_ptr = nullptr;
  uint32_t rawdata_size = (uint32_t)-1;
  char path[MAXNAMLEN + 1];
  FILE * fp;
  riff_chunk_t riff;
  fmt_chunk_t fmt;
  chunk_t chunk;

  if ((int)index >= wavetable_file_list.count
    || snprintf(path, sizeof(path), "%s/%s", wavetable_file_path, wavetable_file_list.get(index)) < 0
    || (fp = fopen(path, "rb")) == 0
  ) return wavetable_ptr;

  if (fread(&riff, sizeof(riff_chunk_t), 1, fp) != 1
    || riff.chunkId != RIFF_CHUNK_ID
    || riff.format != WAVE_FORMAT_ID
  ) return nullptr;

  for (bool supported = true;
    supported
    && (uint32_t)ftell(fp) < riff.chunkSize + sizeof(chunk_t)
    && (fread(&chunk, sizeof(chunk_t), 1, fp) == 1)
    ;
  ) {
    switch (chunk.chunkId) {
      case FMT_CHUNK_ID:
        if (fread(&fmt, sizeof(fmt_chunk_t), 1, fp) != 1
          || fmt.channels != 1
          || (fmt.format != WAVE_FORMAT_PCM && fmt.format != WAVE_FORMAT_FLOAT)
        ) {
          supported = false;
          break;
        }
        rawdata_size = (fmt.bitsPerSample >> 3) * SAMPLE_COUNT * WAVE_COUNT;
//ToDo: larger file support?
        break;
      case DATA_CHUNK_ID:
        if (chunk.chunkSize < rawdata_size || (rawdata_ptr = aligned_alloc(32, rawdata_size)) == nullptr) {
          supported = false;
          break;
        }

        if (fread(rawdata_ptr, rawdata_size, 1, fp) != 1) {
          free(rawdata_ptr);
          supported = false;
          break;
        }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
        wavetable_ptr = (float *)wavebank;
#pragma GCC diagnostic pop
PERFMON_START
        switch (fmt.format) {
          case WAVE_FORMAT_PCM:
            switch (fmt.bitsPerSample) {
              case 8:
                dst_ptr = wavetable_ptr;
                __asm__ volatile (".p2align 5");
                for (uint8_t *src_ptr = (uint8_t *)rawdata_ptr, *end_ptr = src_ptr + SAMPLE_COUNT * WAVE_COUNT; __builtin_expect(src_ptr < end_ptr, 1); src_ptr += 16, dst_ptr += 16) {
                  int8x16_t v_s8 = vreinterpretq_s8_u8(veorq_u8(vld1q_u8((uint8_t *)__builtin_assume_aligned(src_ptr, 16)), vdupq_n_u8(0x80)));
                  __builtin_prefetch(src_ptr + 128, 0, 3);
                  int16x8_t v_s16_l = vmovl_s8(vget_low_s8(v_s8));
                  int16x8_t v_s16_h = vmovl_s8(vget_high_s8(v_s8));
                  int32x4_t v_s32_1 = vmovl_s16(vget_low_s16(v_s16_l));
                  int32x4_t v_s32_2 = vmovl_s16(vget_high_s16(v_s16_l));
                  int32x4_t v_s32_3 = vmovl_s16(vget_low_s16(v_s16_h));
                  int32x4_t v_s32_4 = vmovl_s16(vget_high_s16(v_s16_h));
                  float32x4_t v_f32_1 = vcvtq_n_f32_s32(v_s32_1, 7);
                  float32x4_t v_f32_2 = vcvtq_n_f32_s32(v_s32_2, 7);
                  float32x4_t v_f32_3 = vcvtq_n_f32_s32(v_s32_3, 7);
                  float32x4_t v_f32_4 = vcvtq_n_f32_s32(v_s32_4, 7);
                  vst1q_f32((float *)__builtin_assume_aligned(dst_ptr, 16), v_f32_1);
                  vst1q_f32((float *)__builtin_assume_aligned(dst_ptr + 4, 16), v_f32_2);
                  vst1q_f32((float *)__builtin_assume_aligned(dst_ptr + 8, 16), v_f32_3);
                  vst1q_f32((float *)__builtin_assume_aligned(dst_ptr + 12, 16), v_f32_4);
                }
                break;
              case 16: {
                dst_ptr = wavetable_ptr;
                __asm__ volatile (".p2align 5");
                for (int16_t *src_ptr = (int16_t *)rawdata_ptr, *end_ptr = src_ptr + SAMPLE_COUNT * WAVE_COUNT; __builtin_expect(src_ptr < end_ptr, 1); src_ptr += 8, dst_ptr += 8) {
                  int16x8_t v_q15_0 = vld1q_s16((int16_t *)__builtin_assume_aligned(src_ptr, 16));
                  __builtin_prefetch(src_ptr + 72, 0, 3);
                  int32x4_t v_q31_0 = vmovl_s16(vget_low_s16(v_q15_0));
                  int32x4_t v_q31_1 = vmovl_s16(vget_high_s16(v_q15_0));
                  float32x4_t v_f32_0 = vcvtq_n_f32_s32(v_q31_0, 15);
                  float32x4_t v_f32_1 = vcvtq_n_f32_s32(v_q31_1, 15);
                  vst1q_f32((float *)__builtin_assume_aligned(dst_ptr, 16), v_f32_0);
                  vst1q_f32((float *)__builtin_assume_aligned(dst_ptr + 4, 16), v_f32_1);
                }
/*
int16_t *src_ptr = (int16_t *)rawdata_ptr;
int16_t *end_ptr = src_ptr + SAMPLE_COUNT * WAVE_COUNT;
__asm__ volatile (".p2align 6");
__asm__ volatile (
  ".Lloop_%=:\n"
  "vld1.16 {d16-d17}, [%[src]:128]!\n"
  "vmovl.s16 q10, d16\n"
  "vld1.16 {d18-d19}, [%[src]:128]!\n"
//  "pld [%[src], #128]\n"
  "vmovl.s16 q11, d17\n"
  "cmp %[src], %[end]\n"
  "vcvt.f32.s32 q10, q10, #15\n"
  "vmovl.s16 q12, d18\n"
  "vcvt.f32.s32 q11, q11, #15\n"
  "vmovl.s16 q13, d19\n"
  "vcvt.f32.s32 q12, q12, #15\n"
  "vst1.32 {q10,q11}, [%[dst]:256]!\n"
  "vcvt.f32.s32 q13, q13, #15\n"
  "vst1.32 {q12,q13}, [%[dst]:256]!\n"
  "blt .Lloop_%=\n"
  : [src]"+r"(src_ptr), [dst]"+r"(dst_ptr)
  : [end]"r"(end_ptr)
  : "memory", "q8", "q9", "q10", "q11", "q12", "q13"
);
*/
//__asm__ volatile ("nop\nnop\nnop\nnop\n");
                break;
              } 
              case 24:
                dst_ptr = wavetable_ptr;
                __asm__ volatile (".p2align 5");
                for (uint8_t *src_ptr = (uint8_t *)rawdata_ptr, *end_ptr = src_ptr + SAMPLE_COUNT * WAVE_COUNT; __builtin_expect(src_ptr < end_ptr, 1); src_ptr += 24, dst_ptr += 8) {
                  uint8x8x3_t v_u8 = vld3_u8((uint8_t *)__builtin_assume_aligned(src_ptr, 8));
                  __builtin_prefetch(src_ptr + 144, 0, 3);
                  uint16x8_t v_u16_b0 = vmovl_u8(v_u8.val[0]);
                  uint16x8_t v_u16_b1 = vmovl_u8(v_u8.val[1]);
                  int16x8_t v_s16_b2 = vmovl_s8(vreinterpret_s8_u8(v_u8.val[2]));                  
                  int32x4_t v_s32_0l = vreinterpretq_s32_u32(vmovl_u16(vget_low_u16(v_u16_b0)));
                  int32x4_t v_s32_0h = vreinterpretq_s32_u32(vmovl_u16(vget_high_u16(v_u16_b0)));
                  int32x4_t v_s32_1l = vreinterpretq_s32_u32(vshll_n_u16(vget_low_u16(v_u16_b1), 8));
                  int32x4_t v_s32_1h = vreinterpretq_s32_u32(vshll_n_u16(vget_high_u16(v_u16_b1), 8));
                  int32x4_t v_s32_2l = vshll_n_s16(vget_low_s16(v_s16_b2), 16);
                  int32x4_t v_s32_2h = vshll_n_s16(vget_high_s16(v_s16_b2), 16);
                  int32x4_t v_s32_l = vorrq_s32(v_s32_0l, v_s32_1l);
                  int32x4_t v_s32_h = vorrq_s32(v_s32_0h, v_s32_1h);
                  v_s32_l = vorrq_s32(v_s32_l, v_s32_2l);
                  v_s32_h = vorrq_s32(v_s32_h, v_s32_2h);
                  float32x4_t v_f32_l = vcvtq_n_f32_s32(v_s32_l, 23);
                  float32x4_t v_f32_h = vcvtq_n_f32_s32(v_s32_h, 23);
                  vst1q_f32((float *)__builtin_assume_aligned(dst_ptr, 16), v_f32_l);
                  vst1q_f32((float *)__builtin_assume_aligned(dst_ptr + 4, 16), v_f32_h);
                }
                break;
              case 32:
                dst_ptr = wavetable_ptr;
                __asm__ volatile (".p2align 5");
                for (int32_t *src_ptr = (int32_t *)rawdata_ptr, *end_ptr = src_ptr + SAMPLE_COUNT * WAVE_COUNT; __builtin_expect(src_ptr < end_ptr, 1); src_ptr += 4, dst_ptr += 4) {
                  int32x4_t v_q31_0 = vld1q_s32((int32_t *)__builtin_assume_aligned(src_ptr, 16));
                  __builtin_prefetch(src_ptr + 32, 0, 3);
                  float32x4_t v_f32_0 = vcvtq_n_f32_s32(v_q31_0, 31);
                  vst1q_f32((float *)__builtin_assume_aligned(dst_ptr, 16), v_f32_0);
                }
                break;
              default:
                wavetable_ptr = nullptr;
                break;
            }
            break;
          case WAVE_FORMAT_FLOAT:
            switch (fmt.bitsPerSample) {
              case 32:
                memcpy(__builtin_assume_aligned(wavetable_ptr, 32), __builtin_assume_aligned(rawdata_ptr, 32), rawdata_size);
                break;
//              case 64:
//Will be too slow, convert file to float32 first
//                break;
              default:
                wavetable_ptr = nullptr;
                break;
            }
            break;
          default:
            wavetable_ptr = nullptr;
            break;
        }
PERFMON_END((SAMPLE_COUNT * WAVE_COUNT) >> 2)
        free(rawdata_ptr);
        break;
      default:
        if (fseek(fp, chunk.chunkSize, SEEK_CUR) != 0)
          supported = false;
        break;
    }
  }
  fclose(fp);
  return wavetable_ptr;
}
#endif

static fast_inline void note_on(uint32_t voice_idx) {
  s_phase[voice_idx] = 0.f;
  s_vco[lfo_axis_x].reset(voice_idx);
  s_vco[lfo_axis_y].reset(voice_idx);
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
  if (desc->input_channels != UNIT_INPUT_CHANNELS || desc->output_channels != UNIT_OUTPUT_CHANNELS)
    return k_unit_err_geometry;
#ifdef UNIT_OSC_H_
  runtime_context = (unit_runtime_osc_context_t *)desc->hooks.runtime_context;
#endif
#endif
#ifdef UNIT_TARGET_PLATFORM_MICROKORG2
  osc_wavebank_allocate();
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
#if defined(BPM_SYNC_SUPPORTED) && defined(UNIT_TARGET_PLATFORM_NTS1)
  set_tempo(fx_get_bpm());
#endif
#else
__unit_callback void unit_render(const float * in, float * out, uint32_t frames) {
  (void) in;
#endif
//  PERFMON_START
#ifdef UNIT_TARGET_PLATFORM_MICROKORG2
//ToDo: microKORG2 support
  float32x4x2_t vpitch = {vld1q_f32(&PITCH[0]), vld1q_f32(&PITCH[4])};
  uint32x4x2_t vnote = {vcvtq_u32_f32(vpitch.val[0]), vcvtq_u32_f32(vpitch.val[1])};
  float32x4x2_t vw0 = {
    osc_w0f_for_notex4(vnote.val[0], vpitch.val[0] - vcvtq_f32_u32(vnote.val[0])),
    osc_w0f_for_notex4(vnote.val[1], vpitch.val[1] - vcvtq_f32_u32(vnote.val[1]))
  };
  uint32x4_t vw_0 = vcvtq_n_u32_f32(vw0.val[0], 32);
  uint32x4_t vw_1 = vcvtq_n_u32_f32(vw0.val[1], 32);
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

#ifdef SHAPE_LFO_SUPPORTED
    float shape_lfo = 0.f;
#if defined(UNIT_TARGET_PLATFORM_NTS1_MKII) || defined(UNIT_TARGET_PLATFORM_NTS1)
    static q31_t shape_lfo_old;
    if (shape_lfo_old | runtime_context->shape_lfo)
      shape_lfo = q31_to_f32(runtime_context->shape_lfo) - .5f;
    shape_lfo_old = runtime_context->shape_lfo;
#else
    shape_lfo = q31_to_f32(runtime_context->shape_lfo) * .5f;
#endif
#endif

  for (uint32_t i = 0; i < lfo_axes_count; i++) {
    float offset = .5f;
#ifdef SHAPE_LFO_SUPPORTED
    if (s_vco[i].mode >= lfo_mode_one_shot_plus_shape_lfo)
      offset += shape_lfo;
#endif
#ifdef UNIT_TARGET_PLATFORM_MICROKORG2
//ToDo: apply microKORG2 patch modulation per voice
    offset += vModPatches[i].val[0][0] * .5f;
#endif
    s_vco[i].set_offset(offset);
  }

  if (s_mod_type != mod_type_none) {
#ifdef UNIT_TARGET_PLATFORM_MICROKORG2
    switch (runtime_context->voiceLimit) {
      case kMk2MaxVoices: {
        uint32x4_t vphase_u32_0 = vld1q_u32(&vphase_u32[0]);
        uint32x4_t vphase_u32_1 = vld1q_u32(&vphase_u32[4]);
__asm__ volatile ("nop\nnop\nnop\nnop\n");
__asm__ volatile ("nop\nnop\nnop\nnop\n");
__asm__ volatile ("nop\nnop\nnop\nnop\n");
__asm__ volatile ("nop\nnop\nnop\nnop\n");
        __asm__ volatile (".p2align 5");
        for (float
          * __restrict out_p0 = (float *)__builtin_assume_aligned(out, 16),
          * __restrict out_p1 = out_p0 + frames * 4,
          * out_e = out_p1
          ; __builtin_expect(out_p0 < out_e, 1)
          ; out_p0 += 4, out_p1 += 4
        ) {
          float32x4_t vphase_f32_0 = vcvtq_n_f32_u32(vphase_u32_0, 32);
          float32x4_t vphase_f32_1 = vcvtq_n_f32_u32(vphase_u32_1, 32);
          float32x4_t v_0;
          float32x4_t v_1;
          vst1q_f32(out_p0, v_0);
          vst1q_f32(out_p1, v_1);
          vphase_u32_0 = vaddq_u32(vphase_u32_0, vw_0);
          vphase_u32_1 = vaddq_u32(vphase_u32_1, vw_1);
        }
        vst1q_u32(&vphase_u32[0], vphase_u32_0);
        vst1q_u32(&vphase_u32[4], vphase_u32_1);
        break;
      }
      case kMk2HalfVoices: {
        uint32x4_t vphase_u32_0 = vld1q_u32(&vphase_u32[0]);
        __asm__ volatile (".p2align 5");
        for (float
          * __restrict out_p0 = (float *)__builtin_assume_aligned(out + runtime_context->bufferOffset, 16),
          * __restrict out_e = out_p0 + frames * 4
          ; __builtin_expect(out_p0 < out_e, 1)
          ; out_p0 += 4
        ) {
          float32x4_t vphase_f32_0 = vcvtq_n_f32_u32(vphase_u32_0, 32);
          float32x4_t v_0;

          vst1q_f32(out_p0, v_0);
          vphase_u32_0 = vaddq_u32(vphase_u32_0, vw_0);
        }
        vst1q_u32(&vphase_u32[0], vphase_u32_0);
        break;
      }
      case kMk2QuarterVoices: {
        uint32x2_t vphase_u32_0 = vld1_u32(&vphase_u32[0]);
        __asm__ volatile (".p2align 5");
        for (float
          * __restrict out_p0 = out + runtime_context->voiceOffset,
          * __restrict out_e = out_p0 + frames * runtime_context->outputStride
          ; __builtin_expect(out_p0 < out_e, 1)
          ; out_p0 += runtime_context->outputStride
        ) {
          float32x2_t vphase_f32_0 = vcvtq_n_f32_u32(vphase_u32_0, 32);
          float32x2_t v_0;

          vst1_f32((float *)__builtin_assume_aligned(out_p0, 8), v_0);
          vphase_u32_0 = vadd_u32(vphase_u32_0, vw_0);
        }
        vst1_u32(&vphase_u32[0], vphase_u32_0);
        break;
      }
      case kMk2SingleVoice: {
        uint32_t vphase_u32_0 = vphase_u32[0];
        __asm__ volatile (".p2align 5");
        for (float
          * __restrict out_p0 = (float *)__builtin_assume_aligned(out + runtime_context->voiceOffset, 4),
          * __restrict out_e = out_p0 + frames * 2
          ; __builtin_expect(out_p0 < out_e, 1)
          ; out_p0 += 2
        ) {
          float vphase_f32_0 = uq32_to_f32(vphase_u32_0);
          float v_0;

          *out_p0 = v_0;
          vphase_u32_0 += vw_0.val[0];
        }
        vphase_u32[0] = vphase_u32_0;
        break;
      }
    }
#else

#endif

    for (uint32_t stride_idx = 0; stride_idx < STRIDE_COUNT; stride_idx++) {
      out_p = out + stride_idx * STRIDE * (frames - 1);
      out_e = out_p + STRIDE * frames;
      for (; out_p != out_e; out_p += STRIDE) {
        for (uint32_t voice_idx = STRIDE * stride_idx; voice_idx < STRIDE * stride_idx + VOICE_LIMIT; voice_idx++) {  
          float mod = (s_vco_mod->out(voice_idx) + s_mod_offset) * s_mod_scale;
          float phase = s_phase[voice_idx];
          if (s_mod_type == mod_type_phase)
            phase += mod;
          float out_f = osc_wavebank(phase, s_vco_car->out(voice_idx) * (WAVE_COUNT - 1));
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
        s_vco_car->cycle();
        s_vco_mod->cycle();
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
          float out_f = osc_wavebank(s_phase[voice_idx], s_vco[lfo_axis_x].out(voice_idx) * (WAVE_COUNT_X - 1), s_vco[lfo_axis_y].out(voice_idx) * (WAVE_COUNT_Y - 1));
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
        s_vco[lfo_axis_x].cycle();
        s_vco[lfo_axis_y].cycle();
#ifdef UNIT_TARGET_PLATFORM_MICROKORG2
        vphase.val[stride_idx] += vw0.val[stride_idx];
        vphase.val[stride_idx] -= vcvtq_f32_u32(vcvtq_u32_f32(vphase.val[stride_idx]));
#endif
      }
    }
  }
//  PERFMON_END(frames)
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
  PERFMON_RESET(PARAM_COUNT - 1, index, value)
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
      s_vco[index].set_shape(value);
      break;
#else
    case param_pitch:
      pitch = value << 5 & 0xFF00;
      break;
#endif
#ifdef BPM_SYNC_SUPPORTED
    case param_lfo_bpm_sync_x:
#ifdef UNIT_TARGET_MODULE_OSC
    case param_lfo_bpm_sync_y:
#else
      if (value < 0) {
        value = -value;
        index++;
      }
#endif
      index -= param_lfo_bpm_sync_x;
      s_lfo_bpm_sync[index] = value;
      s_vco[index].set_bpmfreq(s_bpmfreq * s_lfo_bpm_sync[index]);
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
        s_vco[i].set_mode(value - newvalue * 10 - 1);
        if (s_vco[i].mode >= lfo_mode_count)
          s_vco[i].set_mode(lfo_mode_count - 1);
#else
        newvalue = value / lfo_mode_count;
        s_vco[i].set_mode(value - newvalue * lfo_mode_count);
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
      s_vco[index].set_wave(value);
      break;
    case param_lfo_depth_x:
    case param_lfo_depth_y:
      index -= param_lfo_depth_x;
#ifdef USER_TARGET_PLATFORM
      value -= 100;
#endif
      s_vco[index].set_depth((int16_t)value * .005f);
      break;
#ifdef WAVETABLE_FILE_SUPPORTED
    case param_wavetable_file_idx:
      load_wavetable_data(value);
      break;
#endif
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
  PERFMON_VALUE(PARAM_COUNT - 1, index, value)
#ifdef UNIT_TARGET_PLATFORM_MICROKORG2
  static const char *modes[] = {"{One shot", "{Key trigger", "{Random", "{Free run"};
  static char modename[25];
  static const char *dimensions[] = {"|64{X|Amp", "|64{X|Ring", "|64{X|Phase", "32 X 2", "16 X 4", "8 X 8", "4 X 16", "2 X 32", "|Amp{X|64", "|Ring{X|64", "|Phase{X|64"};
  static const char *wfnames[] = {"Saw", "Triangle", "Square", "Sine", "|Sample{&|Hold"};
  static const char *wtnames[] = {"{Wavetable}", "{Wave bank}A ", "{Wave bank}B ", "{Wave bank}C ", "{Wave bank}D ", "{Wave bank}E ", "{Wave bank}F "};
#else
#ifdef SHAPE_LFO_SUPPORTED
  static const char modes[] = "1 T R F 1STSRSFS";
#else
  static const char modes[] = "1 T R F ";
#endif
  static char modename[lfo_axes_count * 2 + 1];
  static const char *dimensions[] = {"64 A", "64 R", "64 P", "32 2", "16 4", "8 8", "4 16", "2 32", "A 64", "R 64", "P 64"};
  static const char *wfnames[] = {"Saw", "Tri", "Sqr", "Sin", "SnH"};
  static char wtnames[][5] = {"WT  ", "WA  ", "WB  ", "WC  ", "WD  ", "WE  ", "WF  "};
#endif
#ifdef WAVETABLE_FILE_SUPPORTED
  static char filename[MAXNAMLEN + 1];
  size_t filename_len;
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
#ifdef BPM_SYNC_SUPPORTED
      } else if (s_vco[index].bpmfreq > 0.f) {
        sprintf(modename, "%02u.%01u|bar", (uint16_t)value >> 4, (uint16_t)value >> 2 & 0x3);
#endif
      } else {
        sprintf(modename, "%.3f|Hz", (dbampf(param_val_to_f32(value) * LFO_RATE_LOG_BIAS) - 1.f) * LFO_MAX_RATE);
      }
      return modename;
#endif
    case param_lfo_modes:
      modename[0] = 0;
      for (int32_t i = lfo_axes_count - 1; i >= 0; i--) {
        uint32_t newvalue = value / lfo_mode_count;
        uint32_t idx = value - newvalue * lfo_mode_count;
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
#ifdef WAVETABLE_FILE_SUPPORTED
    case param_wavetable_file_idx:
      if (value >= wavetable_file_list.count)
        break;
      s = wavetable_file_list.get(value);
      filename_len = strrchr(s, '.') - s;
      memcpy(filename, s, filename_len);
      filename[filename_len] = 0;
      return filename;
#endif
  }
  return nullptr;
}

__unit_callback void unit_set_tempo(uint32_t tempo) {
#ifdef BPM_SYNC_SUPPORTED
  set_tempo(tempo);
#else
  (void)tempo;
#endif
}

#ifndef UNIT_TARGET_PLATFORM_MICROKORG2
__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter) {
  (void)counter;
}
#endif

__unit_callback void unit_reset() {}

__unit_callback void unit_resume() {}

__unit_callback void unit_suspend() {}

__unit_callback void unit_teardown() {
#ifdef WAVETABLE_FILE_SUPPORTED
  wavetable_file_list.cleanup();
#endif
#ifndef UNIT_TARGET_PLATFORM_MICROKORG2
  osc_wavebank_free();
#endif
}
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
      s_vco[lfo_axis_x].set_shape(x);
      s_vco[lfo_axis_y].set_shape(y);
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
  switch (messageId) {
    case kMk2PlatformExclusiveModData: {
      const mk2_mod_data_t *mod_data = (mk2_mod_data_t *)data;
      switch (runtime_context->voiceLimit) {
        case kMk2MaxVoices:
          for (uint32_t i = 0; i < kNumMk2ModSrc; i++) {
            if (mod_data->index[i] >= MOD_PATCHES_COUNT)
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
      if (mod_dest->index < MOD_PATCHES_COUNT)
        strncpy(mod_dest->name, unit_header.params[mod_dest->index].name, UNIT_PARAM_NAME_LEN);
      break;
    }  
  }
}
#endif
