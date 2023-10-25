/*
 *  File: unit.cc
 *
 *  Tesseract Synth unit.
 *
 *
 *  2022-2023 (c) Oleg Burdaev
 *  mailto: dukesrg@gmail.com
 */

#include "unit.h"  // Note: Include common definitions for all units

#include <cstddef>
#include <cstdint>
#include <arm_neon.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <cmath>
#include <ctime>
#include <limits.h>

#include "runtime.h"
#include "attributes.h"
#include "fastpow.h"
#include "arm.h"
#include "wavetable.h"

#define SAMPLERATE_RECIP 2.0833333e-5f  // 1/48000
#define OCTAVE_RECIP .083333333f        // 1/12
#define NOTE_FREQ_OFFSET -150.232645f  // 12 * log2(440/48000) - 69

#define VELOCITY_SENSITIVITY 7.8740157e-3f        // 1/127
#define CONTROL_CHANGE_SENSITIVITY 7.8740157e-3f  // 1/127
#define AFTERTOUCH_SENSITIVITY 7.8740157e-3f      // 1/127
#define PITCH_BEND_CENTER 8192
#define PITCH_BEND_SENSITIVITY 1.220703125e-4f  // 24/8192
#define DIMENSION_COUNT 4

#define POSITION_MAX 128
#define POSITION_MAX_RECIP 3.90625e-3f // 1/256

#define LFO_DEPTH_MAX 128
#define LFO_DEPTH_MAX_RECIP 7.8125e-3f // 1/128

#define LFO_RATE_SCALE 8.1380208e-8f // 1 / 48000 / 256
#define LFO_RATE_SCALE_DISPLAY 3.90625e-3f // 1 / 256
#define LFO_RATE_SCALE_DISPLAY_RECIP 256.f
#define LFO_RATE_SCALE_TEMPO -1.388888889e-6f  // -4 / 48000 / 60
#define LFO_DEPTH_SCALE 1

enum {
  param_gate_note = 0U,
  param_waveform_start,
  param_lfo_type,
  param_lfo_overflow,
  param_position_x,
  param_position_y,
  param_position_z,
  param_position_w,
  param_lfo_rate_x,
  param_lfo_rate_y,
  param_lfo_rate_z,
  param_lfo_rate_w,
  param_lfo_depth_x,
  param_lfo_depth_y,
  param_lfo_depth_z,
  param_lfo_depth_w,
  param_lfo_waveform_x,
  param_lfo_waveform_y,
  param_lfo_waveform_z,
  param_lfo_waveform_w,
  param_dimension_x,
  param_dimension_y,
  param_dimension_z,
  param_dimension_w,
};

enum {
  lfo_type_one_shot = 0U,
  lfo_type_key_trigger,
  lfo_type_random,
  lfo_type_free_run,
  lfo_type_sample_and_hold
};

enum {
  lfo_overflow_saturate = 0U,
  lfo_overflow_wrap,
  lfo_overflow_fold,
};

static wavetable_t WTs;
static const float *sWTSamplePtrs[1 << DIMENSION_COUNT];
static float sWTSizes[1 << DIMENSION_COUNT];
static uint32_t sWTMasks[1 << DIMENSION_COUNT];

static wavetable_t LFOs;
static const float *sLFOSamplePtrs[DIMENSION_COUNT];
static float sLFOSizes[DIMENSION_COUNT];
static uint32_t sLFOMasks[DIMENSION_COUNT];

static float32x4_t sLFOPhase;
static float32x4_t sLFOPhaseMax;
static float32x4_t sLFOPhaseIncrement;
static float32x4_t sLFORateTempo;
static float32x4_t sLFODepth;
static float32x4_t sLFOOutSnH;

static uint32x4_t maskLFOTypeOneShot;
static uint32x4_t maskLFOTypeKeyTrigger;
static uint32x4_t maskLFOTypeRandom;
static uint32x4_t maskLFOTypeFreeRun;
static uint32x4_t maskLFOTypeSnH;
static uint32x4_t maskLFOPhaseOverflow;
static uint32x4_t maskLFOOverflowSaturate;
static uint32x4_t maskLFOOverflowWrap;
static uint32x4_t maskLFOOverflowFold;
static uint32x4_t maskLFORateTempo;

static float sNote;
static float32x2_t sAmp;
static float sNotePhase;
static float sNotePhaseIncrement;
static float sPitchBend;
static float sTempo;

static int32_t sParams[PARAM_COUNT];
static float32x4_t sPosition;

static float32x4_t sDimension;
static float32x4_t sDimensionSaturateLimit;
static float32x4_t sDimensionMax;
static float32x4_t sDimensionRecip;
static uint32x4_t sDimensions;
static uint32x4_t sFraction0Mask;

static uint32x4_t seed = {0x363812fd, 0xbe183480, 0x4740aef7, 0x2a507c5d};

/*===========================================================================*/
/* Private Methods. */
/*===========================================================================*/

fast_inline float32x4_t randomUni() {
  seed = veorq_u32(seed, vshlq_n_u32(seed, 13));
  seed = veorq_u32(seed, vshrq_n_u32(seed, 17));
  seed = veorq_u32(seed, vshlq_n_u32(seed, 5));
  return vcvtq_f32_u32(seed) * 2.32831e-10f;
}

fast_inline float32x4_t randomBi() {
  seed = veorq_u32(seed, vshlq_n_u32(seed, 13));
  seed = veorq_u32(seed, vshrq_n_u32(seed, 17));
  seed = veorq_u32(seed, vshlq_n_u32(seed, 5));
  return vcvtq_f32_s32(vreinterpretq_s32_u32(seed)) * 4.65661e-10f;
}

fast_inline void noteOn(uint8_t note, uint8_t velocity) {
  sNote = note;
  sNotePhaseIncrement = fastpow2((sNote + sPitchBend + NOTE_FREQ_OFFSET) * OCTAVE_RECIP);
  sNotePhase = 0.f;
  sAmp = vdup_n_f32(velocity * VELOCITY_SENSITIVITY);
  sLFOOutSnH = randomUni();
  sLFOPhase = vbslq_f32(maskLFOTypeRandom | maskLFOTypeSnH,
    sLFOOutSnH,
    vreinterpretq_f32_u32(vbicq_u32(vreinterpretq_u32_f32(sLFOPhase), maskLFOTypeOneShot | maskLFOTypeKeyTrigger))
  );
  maskLFOPhaseOverflow = vdupq_n_u32(0);
}

fast_inline void noteOff(uint8_t note) {
  (void)note;
  sAmp = vdup_n_f32(0.f);
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

  WTs.init(desc, 0x65766157); // 'Wave'
  LFOs.init(desc, 0x734F464C); // 'LFOs'

  return k_unit_err_none;
}

__unit_callback void unit_teardown() {
}

__unit_callback void unit_reset() {
  sNotePhase = 0.f;
  sAmp = vdup_n_f32(0.f);
  sLFOPhase = vdupq_n_f32(0.f);
}

__unit_callback void unit_resume() {
}

__unit_callback void unit_suspend() {
}

fast_inline float calcPosition(int32_t position, int32_t dimension) {
  return POSITION_MAX_RECIP * (position + POSITION_MAX) * dimension - (dimension == 1 ? 0.f : .5f);
}

fast_inline float calcLfoDepth(int32_t depth, int32_t dimension) {
  return depth * dimension * LFO_DEPTH_MAX_RECIP * LFO_DEPTH_SCALE * .5f;
}

fast_inline float32x4_t wave4x(const float **sample_ptr, float32x4_t size, uint32x4_t size_mask, float x) {
  const float32x4_t x0f = vmulq_n_f32(size, x);
  const uint32x4_t x0i = vcvtq_u32_f32(x0f);
  const uint32x4_t x1i  = x0i + 1;
  const float32x4_t frac1 = x0f - vcvtq_f32_u32(x0i);
  const float32x4_t frac0 = 1.f - frac1;
  const uint32x4_t x0 = x0i & size_mask; 
  const uint32x4_t x1 = x1i & size_mask;
  const float32x4_t w0 = {sample_ptr[0][x0[0]], sample_ptr[1][x0[1]], sample_ptr[2][x0[2]], sample_ptr[3][x0[3]]};
  const float32x4_t w1 = {sample_ptr[0][x1[0]], sample_ptr[1][x1[1]], sample_ptr[2][x1[2]], sample_ptr[3][x1[3]]};
  return w0 * frac0 + w1 * frac1;
}

fast_inline float32x4_t wave4x(const float **sample_ptr, float32x4_t size, uint32x4_t size_mask, float32x4_t x) {
  const float32x4_t x0f = size * x;
  const uint32x4_t x0i = vcvtq_u32_f32(x0f);
  const uint32x4_t x1i  = x0i + 1;
  const float32x4_t frac1 = x0f - vcvtq_f32_u32(x0i);
  const float32x4_t frac0 = 1.f - frac1;
  const uint32x4_t x0 = x0i & size_mask; 
  const uint32x4_t x1 = x1i & size_mask;
  const float32x4_t w0 = {sample_ptr[0][x0[0]], sample_ptr[1][x0[1]], sample_ptr[2][x0[2]], sample_ptr[3][x0[3]]};
  const float32x4_t w1 = {sample_ptr[0][x1[0]], sample_ptr[1][x1[1]], sample_ptr[2][x1[2]], sample_ptr[3][x1[3]]};
  return w0 * frac0 + w1 * frac1;
}

fast_inline void preloadWave(uint32_t index, uint32_t x) {
  const wave_t w = WTs.getWave(x);
  sWTSamplePtrs[index] = w.sample_ptr;
  sWTSizes[index] = w.size;
  sWTMasks[index] = w.size_mask;
}

fast_inline void preloadWaveQT(uint32_t index, uint32x4_t x) {
  preloadWave(index + 4 * 0, x[0]);
  preloadWave(index + 4 * 1, x[1]);
  preloadWave(index + 4 * 2, x[2]);
  preloadWave(index + 4 * 3, x[3]);
}

__unit_callback void unit_render(const float * in, float * out, uint32_t frames) {
  (void)in;
  float * __restrict out_p = out;
  const float * out_e = out_p + (frames << 1);
    
  float32x4_t vLFOOut = wave4x(sLFOSamplePtrs, *(float32x4_t *)&sLFOSizes, *(uint32x4_t *)&sLFOMasks, vbslq_f32(maskLFOTypeSnH, sLFOOutSnH, sLFOPhase));
  float32x4_t sLFOPhaseOld = sLFOPhase;
  sLFOPhase += sLFOPhaseIncrement * (float)frames;
  sLFOPhase -= vcvtq_f32_u32(vcvtq_u32_f32(sLFOPhase));
  uint32x4_t maskLFOPhaseOverflowTmp = vcltq_f32(sLFOPhase, sLFOPhaseOld);
  sLFOOutSnH = vbslq_f32(maskLFOPhaseOverflowTmp, randomUni(), sLFOOutSnH);
  maskLFOPhaseOverflow |= maskLFOPhaseOverflowTmp;
  sLFOPhase = vbslq_f32(maskLFOPhaseOverflow & maskLFOTypeOneShot, sLFOPhaseMax, sLFOPhase);

  float32x4_t vPosition = sPosition;
  vPosition += vLFOOut * sLFODepth;
  vPosition = vbslq_f32(maskLFOOverflowSaturate,
    vminq_f32(sDimensionSaturateLimit, vmaxq_f32(vdupq_n_f32(0.f), vPosition)),
    vPosition
  );

  vPosition = vbslq_f32(maskLFOOverflowWrap,
    vPosition + vreinterpretq_f32_u32(vbicq_u32(
      vreinterpretq_u32_f32(sDimensionMax),
      vcgeq_f32(vPosition, vdupq_n_f32(0.f))
    )),
    vPosition
  );

  float32x4_t pos = vabsq_f32(vPosition);
  float32x4_t frac = pos;
  pos = vcvtq_f32_u32(vcvtq_u32_f32(pos));
  uint32x4_t folds = vcvtq_u32_f32(pos * sDimensionRecip);
  uint32x4_t oddMask = vtstq_u32(folds, vdupq_n_u32(1));
  frac -= pos;
  pos -= vcvtq_f32_u32(folds) * sDimension;
  vPosition = vbslq_f32(maskLFOOverflowFold,
    vbslq_f32(oddMask, sDimensionSaturateLimit - pos, pos) + vbslq_f32(oddMask, 1.f - frac, frac),
    vPosition
  );

  uint32x4_t vp0 = vcvtq_u32_f32(vPosition);
  uint32x4_t vp1 = vp0 + 1;
  float32x4_t vp0f = vcvtq_f32_u32(vp0);
  float32x4_t vp1f = vcvtq_f32_u32(vp1);
  float32x4_t vFractions1 = vPosition - vp0f;
  float32x4_t vFractions0 = 1.f - vFractions1;

  vFractions0 = vbslq_f32(sFraction0Mask,
    vminq_f32(vPosition, vdupq_n_f32(1.f)),
    vFractions0
  );
  vFractions1 = vreinterpretq_f32_u32(vbicq_u32(
    vreinterpretq_u32_f32(vFractions1),
    sFraction0Mask
  ));

  float32x4x2_t vFractions = vzipq_f32(vFractions0, vFractions1); 
  vp0 -= *(uint32x4_t *)&sParams[param_dimension_x] * vcvtq_u32_f32(vp0f * sDimensionRecip);
  vp1 -= *(uint32x4_t *)&sParams[param_dimension_x] * vcvtq_u32_f32(vp1f * sDimensionRecip);
  uint32x4_t vPositions0 = vp0 * sDimensions;
  uint32x4_t vPositions1 = vp1 * sDimensions;

  uint32x2x2_t p0 = vzip_u32(vget_low_u32(vPositions0), vget_low_u32(vPositions1));
  uint32x2_t r0 = sParams[param_waveform_start] + p0.val[0];
  uint32x2x2_t p1 = vzip_u32(p0.val[1], p0.val[1]);
  uint32x4_t r1 = vcombine_u32(r0, r0) + vcombine_u32(p1.val[0], p1.val[1]);
  uint32x4_t r2 = r1 + vdupq_lane_u32(vget_high_u32(vPositions0), 0);
  uint32x4_t r3 = vdupq_lane_u32(vget_high_u32(vPositions0), 1);
  uint32x4_t r4 = r1 + vdupq_lane_u32(vget_high_u32(vPositions1), 0);
  uint32x4_t r5 = vdupq_lane_u32(vget_high_u32(vPositions1), 1);

  preloadWaveQT(0, r2 + r3);
  preloadWaveQT(1, r4 + r3);
  preloadWaveQT(2, r2 + r5);
  preloadWaveQT(3, r4 + r5);

  for (; out_p != out_e; out_p += 2) {
    float32x4_t w1 = wave4x(&sWTSamplePtrs[0], *(float32x4_t *)&sWTSizes[0], *(uint32x4_t *)&sWTMasks[0], sNotePhase);
    float32x4_t w2 = wave4x(&sWTSamplePtrs[4], *(float32x4_t *)&sWTSizes[4], *(uint32x4_t *)&sWTMasks[4], sNotePhase);
    float32x4_t w3 = wave4x(&sWTSamplePtrs[8], *(float32x4_t *)&sWTSizes[8], *(uint32x4_t *)&sWTMasks[8], sNotePhase);
    float32x4_t w4 = wave4x(&sWTSamplePtrs[12], *(float32x4_t *)&sWTSizes[12], *(uint32x4_t *)&sWTMasks[12], sNotePhase);

    float32x4_t o1 = vmulq_lane_f32(w1, vget_low_f32(vFractions.val[0]), 0);
    float32x4_t o2 = vmulq_lane_f32(w2, vget_low_f32(vFractions.val[0]), 1);
    float32x4_t o3 = vmulq_lane_f32(w3, vget_low_f32(vFractions.val[0]), 0);
    float32x4_t o4 = vmulq_lane_f32(w4, vget_low_f32(vFractions.val[0]), 1);
    float32x4_t o5 = vmulq_lane_f32(o1 + o2, vget_high_f32(vFractions.val[0]), 0);
    float32x4_t o6 = vmulq_lane_f32(o3 + o4, vget_high_f32(vFractions.val[0]), 1);
    float32x4_t o7 = (o5 + o6) * vcombine_f32(vget_low_f32(vFractions.val[1]), vget_low_f32(vFractions.val[1]));
    float32x2_t o8 = vpadd_f32(vget_low_f32(o7), vget_high_f32(o7)) * vget_high_f32(vFractions.val[1]);
    vst1_f32(out_p, sAmp * vpadd_f32(o8, o8));

    sNotePhase += sNotePhaseIncrement;
    sNotePhase -= (int32_t)sNotePhase;
  }
  return;
}

__unit_callback void unit_set_param_value(uint8_t id, int32_t value) {
  static wave_t w;
  static uint32x4_t mask;
  value = (int16_t)value;
  sParams[id] = value;
  switch (id) {
    case param_lfo_type:
      for (uint32_t i = 0; i < DIMENSION_COUNT; i++) {
        mask[i] = value % 5;
        value /= 5;
      }
      maskLFOTypeOneShot = vceqq_u32(vdupq_n_u32(lfo_type_one_shot), mask);
      maskLFOTypeKeyTrigger = vceqq_u32(vdupq_n_u32(lfo_type_key_trigger), mask);
      maskLFOTypeRandom = vceqq_u32(vdupq_n_u32(lfo_type_random), mask);
      maskLFOTypeFreeRun = vceqq_u32(vdupq_n_u32(lfo_type_free_run), mask);
      maskLFOTypeSnH = vceqq_u32(vdupq_n_u32(lfo_type_sample_and_hold), mask);
      break;
    case param_lfo_overflow:
      for (uint32_t i = 0; i < DIMENSION_COUNT; i++) {
        mask[i] = value % 3;
        value /= 3;
      }
      maskLFOOverflowWrap = vceqq_u32(vdupq_n_u32(lfo_overflow_wrap), mask);
      maskLFOOverflowSaturate = vceqq_u32(vdupq_n_u32(lfo_overflow_saturate), mask);
      maskLFOOverflowFold = vceqq_u32(vdupq_n_u32(lfo_overflow_fold), mask);
      break;
    case param_position_x:
    case param_position_y:
    case param_position_z:
    case param_position_w:
      id &= DIMENSION_COUNT - 1;
      sPosition[id] = calcPosition(value, sParams[param_dimension_x + id]);
      break;
    case param_lfo_rate_x:
    case param_lfo_rate_y:
    case param_lfo_rate_z:
    case param_lfo_rate_w:
      id &= DIMENSION_COUNT - 1;
      if (value >= 0) {
        sLFOPhaseIncrement[id] = value * LFO_RATE_SCALE;
        maskLFORateTempo[id] = 0;
      } else {
        sLFORateTempo[id] = LFO_RATE_SCALE_TEMPO / value;
        maskLFORateTempo[id] = -1;
      }
      sLFOPhaseIncrement = vbslq_f32(maskLFORateTempo, sLFORateTempo * sTempo, sLFOPhaseIncrement);
      break;
    case param_lfo_depth_x:
    case param_lfo_depth_y:
    case param_lfo_depth_z:
    case param_lfo_depth_w:
      id &= DIMENSION_COUNT - 1;
      sLFODepth[id] = calcLfoDepth(value, sParams[param_dimension_x + id]);
      break;
    case param_lfo_waveform_x:
    case param_lfo_waveform_y:
    case param_lfo_waveform_z:
    case param_lfo_waveform_w:
      id &= DIMENSION_COUNT - 1;
      w = LFOs.getWave(value);
      sLFOSamplePtrs[id] = w.sample_ptr;
      sLFOSizes[id] = w.size;
      sLFOMasks[id] = w.size_mask;
      sLFOPhaseMax[id] = w.size_mask / w.size;
      break;
    case param_dimension_x:
      sDimensions[0] = 1;
    case param_dimension_y:
      sDimensions[1] = sDimensions[0] * sParams[param_dimension_x];
    case param_dimension_z:
      sDimensions[2] = sDimensions[1] * sParams[param_dimension_y];
    case param_dimension_w:
      sDimensions[3] = sDimensions[2] * sParams[param_dimension_z];
      id &= DIMENSION_COUNT - 1;
      sPosition[id] = calcPosition(sParams[param_position_x + id], value);
      sLFODepth[id] = calcLfoDepth(sParams[param_lfo_depth_x + id], value);
      sDimension[id] = value;
      sDimensionSaturateLimit[id] = value == 1 ? 1 : value - 1;
      sDimensionMax[id] = value * LFO_DEPTH_SCALE;
      sDimensionRecip[id] = 1.f / value;
      sFraction0Mask[id] = value == 1 ? -1 : 0;
      break;
    default:
      break;
  }
}

__unit_callback int32_t unit_get_param_value(uint8_t id) {
  return sParams[id];
}

__unit_callback const char * unit_get_param_str_value(uint8_t id, int32_t value) {
  value = (int16_t)value;
  static const char * lfoTypes = "OTRFS";
  static const char * lfoOverflows = "SWF";
  static char s[UNIT_PARAM_NAME_LEN + 1];
  static const char * name;
  static uint32_t modes[DIMENSION_COUNT];
  static float seconds;

  switch (id) {
    case param_lfo_type:
      for (uint32_t i = 0; i < DIMENSION_COUNT; i++) {
        modes[i] = value % 5;
        value /= 5;
      }
      sprintf(s, "%c.%c.%c.%c", lfoTypes[modes[0]], lfoTypes[modes[1]], lfoTypes[modes[2]], lfoTypes[modes[3]]);
      break;
    case param_lfo_overflow:
      for (uint32_t i = 0; i < DIMENSION_COUNT; i++) {
        modes[i] = value % 3;
        value /= 3;
      }
      sprintf(s, "%c.%c.%c.%c", lfoOverflows[modes[0]], lfoOverflows[modes[1]], lfoOverflows[modes[2]], lfoOverflows[modes[3]]);
      break;
    case param_position_x:
    case param_position_y:
    case param_position_z:
    case param_position_w:
      id &= DIMENSION_COUNT - 1;
      sprintf(s, "%.*f", sParams[param_dimension_x + id] >= 1000 ? 2 : sParams[param_dimension_x + id] >= 100 ? 3 : 4, calcPosition(value, sParams[param_dimension_x + id]) + (sParams[param_dimension_x + id] == 1 ? 0.f : 1.f));
      break;
    case param_lfo_rate_x:
    case param_lfo_rate_y:
    case param_lfo_rate_z:
    case param_lfo_rate_w:
      id &= DIMENSION_COUNT - 1;
      if (value > 0) {
        if (maskLFOTypeOneShot[id]) {
          seconds = LFO_RATE_SCALE_DISPLAY_RECIP / value;
          sprintf(s, "%.*fs", seconds < 1.f ? 4 : seconds >= 10.f ? 2 : 3, seconds);
        } else 
          sprintf(s, "%.3fHz", value * LFO_RATE_SCALE_DISPLAY);
      } else if (value < 0) {
        value =- value;
        sprintf(s, "%d.%d.%dt", value >> 4, (value >> 2) & 3,  value & 3);
      } else {
        sprintf(s, "%d", value);
      }
      break;
    case param_lfo_depth_x:
    case param_lfo_depth_y:
    case param_lfo_depth_z:
    case param_lfo_depth_w:
      id &= DIMENSION_COUNT - 1;
      sprintf(s, "%.*f", sParams[param_dimension_x + id] >= 1000 ? 2 : sParams[param_dimension_x + id] >= 100 ? 3 : 4, calcLfoDepth(value, sParams[param_dimension_x + id]) * 2.f);
      break;
    case param_waveform_start:
    case param_lfo_waveform_x:
    case param_lfo_waveform_y:
    case param_lfo_waveform_z:
    case param_lfo_waveform_w:
      if ((name = (id == param_waveform_start ? WTs.getName(value) : LFOs.getName(value))) != nullptr)
        return name;
    default:   
      sprintf(s, "%d", value);
      break;
  }
  return s;
}

__unit_callback const uint8_t * unit_get_param_bmp_value(uint8_t id, int32_t value) {
  (void)id;
  (void)value;
  return nullptr;
}

__unit_callback void unit_set_tempo(uint32_t tempo) {
  sTempo = uq16_16_to_f32(tempo);
  sLFOPhaseIncrement = vbslq_f32(maskLFORateTempo, sLFORateTempo * sTempo, sLFOPhaseIncrement);
}

__unit_callback void unit_note_on(uint8_t note, uint8_t velocity) {
  noteOn(note, velocity);
}

__unit_callback void unit_note_off(uint8_t note) {
  noteOff(note);
}

__unit_callback void unit_gate_on(uint8_t velocity) {
  noteOn(sParams[param_gate_note], velocity);
}

__unit_callback void unit_gate_off() {
  noteOff(0);
}

__unit_callback void unit_all_note_off() {
  noteOff(0);
}

__unit_callback void unit_pitch_bend(uint16_t bend) {
  sPitchBend = (bend - PITCH_BEND_CENTER) * PITCH_BEND_SENSITIVITY;
  sNotePhaseIncrement = fastpow2((sNote + sPitchBend + NOTE_FREQ_OFFSET) * OCTAVE_RECIP);
}

__unit_callback void unit_channel_pressure(uint8_t pressure) {
  sAmp = vdup_n_f32(pressure * CONTROL_CHANGE_SENSITIVITY);
}

__unit_callback void unit_aftertouch(uint8_t note, uint8_t aftertouch) {
  (void)note;
  sAmp = vdup_n_f32(aftertouch * AFTERTOUCH_SENSITIVITY);
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
