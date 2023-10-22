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
// #define NOTE_A4 69
// #define SAMPLERATE_RECIP_X_440HZ .009166666f // 440/48000
#define NOTE_FREQ_OFFSET -150.232645f  // 12 * log2(440/48000) - 69

// #define TO_F32 4.65661287e-10f

#define VELOCITY_SENSITIVITY 7.8740157e-3f        // 1/127
#define CONTROL_CHANGE_SENSITIVITY 7.8740157e-3f  // 1/127
#define AFTERTOUCH_SENSITIVITY 7.8740157e-3f      // 1/127
#define PITCH_BEND_CENTER 8192
#define PITCH_BEND_SENSITIVITY 1.220703125e-4f  // 24/8192
#define DIMENSION_COUNT 4

//#define POSITION_MAX 2048.f
//#define POSITION_MAX_RECIP 2.44140625e-4f // 1/4096
#define POSITION_MAX 128
#define POSITION_MAX_RECIP 3.90625e-3f // 1/256

#define LFO_DEPTH_MAX 128
#define LFO_DEPTH_MAX_RECIP 7.8125e-3f // 1/128

// #define LFO_RATE_SCALE .001f
// #define LFO_RATE_SCALE 89.478485f    // 2^32 / 48000 / 1000
//#define LFO_RATE_SCALE 699.05067f         // 2^32 / 48000 / 128
#define LFO_RATE_SCALE 1.62760417e-7f // 1 / 48000 / 128
#define LFO_RATE_SCALE_DISPLAY 7.8125e-3f // 1 / 128
#define LFO_RATE_SCALE_TEMPO .022755555f  // 2^32 / 48000 / 60 / 2^16 - for 1/4
// #define LFO_DEPTH_SCALE .001953125f  // 1/512
// #define LFO_DEPTH_SCALE 9.0949470e-13f  // 1 / 2^31 / 512
#define LFO_DEPTH_SCALE 1
// #define UINTMAX_RECIP 2.3283064e-10f
//#define INTMAX_RECIP 4.65661287e-10f

/*
#define LFO_MODE_SAMPLE_AND_HOLD 48

#define USER_BANK 5

#define lfoType(a) (a & 3)
#define lfoWave(a) ((a >> 2) & 3)
#define lfoOverflow(a) ((a >> 4) & 3)

#define ldq_f32(a, b) (*(float *)((a)[b]))
#define vmovq_x4_f32(f, a, b, c, d) vsetq_lane_f32(d, vsetq_lane_f32(c, vsetq_lane_f32(b, vsetq_lane_f32(a, f, 0), 1), 2), 3);
#define vld1q_f32_indirect(f, a) vmovq_x4_f32(f, ldq_f32(a, 0), ldq_f32(a, 1), ldq_f32(a, 2), ldq_f32(a, 3))
#define vlinintq_f32(a, b, c) (a + (b - a) * c)  // vfmaq_f32
#define vlinint_f32(a, b, c) (a + (b - a) * c)   // vfma_f32
#define vlinintq_lane_f32(a, b, c, d) vmlaq_lane_f32(a, b - a, c, d)
#define vlinint_lane_f32(a, b, c, d) vmla_lane_f32(a, b - a, c, d)

#define ld_f32(a, b) vld1_f32((const float *)((a)[b]))
#define vld2q_f32_indirect(a) vuzpq_f32(vcombine_f32(ld_f32(a, 0), ld_f32(a, 1)), vcombine_f32(ld_f32(a, 2), ld_f32(a, 3)))
*/

// #define vaddq_dupq_n_u32(a,b) vaddq_u32(a, vdupq_n_u32(b))
// #define u32x4(a,b,c,d) vsetq_lane_u32(d, vsetq_lane_u32(c, vsetq_lane_u32(b, vdupq_n_u32(a), 1), 2), 3)
// #define ldf32(a,b) (*(float*)vgetq_lane_u32(a, b))
// #define vsetq_x4_f32(a,b,c,d) vsetq_lane_f32(d, vsetq_lane_f32(c, vsetq_lane_f32(b, vdupq_n_f32(a), 1) , 2), 3)
// #define vmovq_x4_f32(a,b,c,d) vsetq_lane_f32(d, vsetq_lane_f32(c, vsetq_lane_f32(b, vdupq_n_f32(a), 1) , 2), 3)
// #define vmovq_x4_u32(a,b,c,d) vsetq_lane_u32(d, vsetq_lane_u32(c, vsetq_lane_u32(b, vdupq_n_u32(a), 1) , 2), 3)
// #define vmovq_x4_s32(a,b,c,d) vsetq_lane_s32(d, vsetq_lane_s32(c, vsetq_lane_s32(b, vdupq_n_s32(a), 1) , 2), 3)
// #define vld1q_f32_indirect(a) vmovq_x4_f32(ld_f32(a, 0), ld_f32(a, 1), ld_f32(a, 2), ld_f32(a, 3))
// #define vlinintq_n_f32_indirect(a,b,c,d) vlinintq_n_f32(vld1q_f32_indirect(vaddq_dupq_n_u32(a, b)), vld1q_f32_indirect(vaddq_dupq_n_u32(a, c)), d)
// #define vlinintq_n_f32(a,b,c) vmlaq_f32(a, vsubq_f32(b, a), vdupq_n_f32(c))
// #define vlinintq_lane_f32(a,b,c,d) vmlaq_lane_f32(a, vsubq_f32(b, a), c, d)
// #define vlinint_n_f32(a,b,c) vmla_f32(a, vsub_f32(b, a), vdup_n_f32(c))
// #define vlinint_n_f32x2(a,b) (vlinint_n_f32(vget_low_f32(a), vget_high_f32(a), b))
// #define linint_f32(a,b,c) (a + (b - a) * c)
// #define linint_f32x2(a,b) linint_f32(vget_lane_f32(a, 0), vget_lane_f32(a, 1), b)
// #define veorshlq_u32(a,b) veorq_u32(a, vshlq_n_u32(a, b))
// #define veorshrq_u32(a,b) veorq_u32(a, vshrq_n_u32(a, b))
// #define veorshlq_s32(a,b) veorq_s32(a, vshlq_n_s32(a, b))
// #define veorshrq_s32(a,b) veorq_s32(a, vshrq_n_s32(a, b))

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
  lfo_overflow_wrap = 0U,
  lfo_overflow_saturate,
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

static float sNote;
static float32x2_t sAmp;
static float sNotePhase;
static float sNotePhaseIncrement;
static float sPitchBend;

static int32_t sParams[PARAM_COUNT];
static float32x4_t sPosition;

// static uint32_t sLfoMode[DIMENSION_COUNT];
// static uint32_t sLfoWave[DIMENSION_COUNT];
// uint32_t sLfoOverflow[DIMENSION_COUNT];
static float32x4_t sDimension;
static float32x4_t sDimensionSaturateLimit;
static float32x4_t sDimensionMax;
static float32x4_t sDimensionRecip;
//static float32x4_t sDimensionMaxRecip;
static uint32x4_t sDimensions;
//static uint32x4_t sDimensionScale;
static uint32x4_t sFraction0Mask;

static uint32x4_t seed = {0x363812fd, 0xbe183480, 0x4740aef7, 0x2a507c5d};

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

/*
static struct lfo_t {
  int32x4_t p0, p1, inc;
  uint32x4_t type, wave, overflow;
  uint32x4_t maskTypeOneShot, maskTypeTriggerOrOneShot, maskTypeRandom, maskTypeSnH;
  uint32x4_t maskWaveTriangle, maskWaveSquare, maskWaveSine;
  uint32x4_t maskOverflowSaturate, maskOverflowWrap, maskOverflowFold;
  uint32x4_t maskPhaseOverflow;
  int32x4_t SnHOut;
  uint32x4_t seed = vsetq_lane_u32(0x363812fd, vsetq_lane_u32(0xbe183480, vsetq_lane_u32(0x4740aef7, vdupq_n_u32(0x2a507c5d), 1), 2), 3);

  // get next random vector
  fast_inline int32x4_t random() {
    seed = veorq_u32(seed, vshlq_n_u32(seed, 13));
    seed = veorq_u32(seed, vshrq_n_u32(seed, 17));
    seed = veorq_u32(seed, vshlq_n_u32(seed, 5));
    return vreinterpretq_s32_u32(seed);
  }
  // Set LFO phases and increments to initial values
  force_inline void init() {
    p0 = vdupq_n_s32(INT_MIN);
    p1 = vdupq_n_s32(INT_MIN);
    inc = vdupq_n_s32(0);
  }
  // Set LFO phases to initial values with respect to LFO modes
  force_inline void trigger() {
    p1 = p0 = vbslq_s32(maskTypeTriggerOrOneShot,  // if one shot or trigger
                        vdupq_n_s32(INT_MIN),      // reset phase
                        vbslq_s32(maskTypeRandom,  // else if random
                                                   //          vmovq_x4_s32((uint32_t)rand() << 1, (uint32_t)rand() << 1, (uint32_t)rand() << 1, (uint32_t)rand() << 1),
                                  random(),
                                  p0  // else keep current
                                  ));
  }
  // Increment LFO phases values
  force_inline void advance() {
    p1 = p0;
    p0 = vaddq_s32(p0, inc);
    maskPhaseOverflow = vcltq_s32(p0, p1);
    SnHOut = vbslq_s32(maskPhaseOverflow, random(), SnHOut);
    uint32x4_t mask = vandq_u32(maskTypeOneShot, maskPhaseOverflow);
    inc = vbslq_s32(mask, vdupq_n_s32(0), inc);
    p0 = vbslq_s32(mask, vdupq_n_s32(INT_MAX), p0);
    p1 = vbslq_s32(mask, vdupq_n_s32(INT_MAX), p1);
  }
  // Ser LFO phase increment value
  force_inline void setIncrement(const int index, int32_t value) {
    inc[index] = value;
  }
  // Set LFO mode
  force_inline void setMode(uint32_t index, uint32_t value) {
    type[index] = lfoType(value);
    wave[index] = lfoWave(value);
    overflow[index] = lfoOverflow(value);
    maskTypeSnH[index] = value == LFO_MODE_SAMPLE_AND_HOLD ? 0xFFFFFFFF : 0;
    maskTypeOneShot = vceqq_u32(vdupq_n_u32(lfo_type_one_shot), type);
    maskTypeTriggerOrOneShot = vcltq_u32(vdupq_n_u32(lfo_type_random), type);
    maskTypeRandom = vceqq_u32(vdupq_n_u32(lfo_type_random), type);
    maskWaveSine = vceqq_u32(vdupq_n_u32(lfo_wave_sine), wave);
    maskWaveSquare = vceqq_u32(vdupq_n_u32(lfo_wave_square), wave);
    maskWaveTriangle = vceqq_u32(vdupq_n_u32(lfo_wave_triangle), wave);
    maskOverflowSaturate = vceqq_u32(vdupq_n_u32(lfo_overflow_saturate), overflow);
    maskOverflowWrap = vceqq_u32(vdupq_n_u32(lfo_overflow_wrap), overflow);
    maskOverflowFold = vceqq_u32(vdupq_n_u32(lfo_overflow_fold), overflow);
    //        sLfoOverflow[index] = lfoOverflow(value);
  }
  // Get LFO outputs
  force_inline float32x4_t get() {
    int32x4_t p0abs = vqabsq_s32(p0);
    return vmulq_f32(vdupq_n_f32(INTMAX_RECIP), vcvtq_f32_s32(  // convert LFO output to float
                                                    vbslq_s32(maskTypeSnH, SnHOut,
                                                              vbslq_s32(maskWaveSine,                                                                                    // if sine
                                                                        vmulq_s32(vdupq_n_s32(4), vmulq_s32(p0, vaddq_s32(p0abs, vdupq_n_s32(INT_MIN)))),                // sine value = 4 * phase * (abs(phase) - 1)
                                                                        vbslq_s32(maskWaveSquare,                                                                        // else if square
                                                                                  vbslq_s32(vcltq_s32(p0, vdupq_n_s32(0)), vdupq_n_s32(INT_MIN), vdupq_n_s32(INT_MAX)),  // square out = phase < 0 ? -1 : 1
                                                                                  vbslq_s32(maskWaveTriangle,                                                            // else if triangle
                                                                                            vmlaq_n_s32(vdupq_n_s32(INT_MIN), p0abs, 2),                                 // triangle out = 2 * abs(phase) - 1
                                                                                            p0                                                                           // else saw out = phase
                                                                                            ))))));
  }
} sLfo;
*/

/*===========================================================================*/
/* Private Methods. */
/*===========================================================================*/

/*fast_inline void setSampleOffset() {
  sSampleOffset = sSample == NULL ? 0 : ((uint32_t)sSample->sample_ptr + sWaveOffset * sWaveSize * sSample->channels * sizeof(float));
}*/
/*fast_inline void setDimensions(uint32_t index, uint32_t value) {
  sDimension[index] = value;
  sDimensionMax = sDimension - 1.f;
  sDimensionRecip = 1.f / sDimension;
  sDimensionMaxRecip = 1.f / sDimensionMax;
  sDimensionScale = sDimensions * sWaveSize * sSample->channels;
}
*/

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

/*
static const float *s_wave_sample_ptr[DIMENSION_COUNT][2];
static float s_wave_size[DIMENSION_COUNT][2];
static uint32_t s_wave_size_mask[DIMENSION_COUNT][2];
static uint32_t s_wave_size_exp[DIMENSION_COUNT][2];
static uint32_t s_wave_size_frac[DIMENSION_COUNT][2];
*/
/*
static fast_inline void setWavePosition(uint32_t index, float position) {
  if (position < 0.f) //vclrq_f32 vdupq_n_f32 vbslq_f32 vdupa_n_f32 vaddq_f32  
    position += sParams[param_dimension_x + index];
  uint32_t p0 = ((uint32_t)position) % sParams[param_dimension_x + index];
  uint32_t p1 = ((p0 + 1) % sParams[param_dimension_x + index]);
  sPositions[index][0] = p0 * sDimensions[index]; //vmulq_f32 vmulq_f32
  sPositions[index][1] = p1 * sDimensions[index];
  sFractions[index][1] = position - (uint32_t)position; // vcvtq_f32_u32, vsubq_f32
  sFractions[index][0] = 1.f - sFractions[index][1];
//  sFractions[index][0] = sParams[param_dimension_x + index] == 1 ? 0.f : (1.f - sFractions[index][1]); //vceqq_u32 vdupq_n_u32 vbslq_s32 vdupq_n_f32 vsubq_f32
}
*/
/*
fast_inline float wave(wave_t *w, float x) {
  const float x0f = x * w->size;
  const uint32_t x0i = x0f;
  const uint32_t x0 = x0i & w->size_mask;
//  const uint32_t x1 = (x0 + 1) & w->size_mask;
  const uint32_t x1 = (x0i + 1) & w->size_mask;
//  const float res = w->sample_ptr[x0] + (x0f - x0i) * (w->sample_ptr[x1] - w->sample_ptr[x0]);
  const float frac1 = x0f - x0i;
  const float frac0 = 1.f - frac1;
  const float res = w->sample_ptr[x0] * frac0 + w->sample_ptr[x1] * frac1;
  return res;
}
*/
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
  const float * out_e = out_p + (frames << 1);  // assuming stereo output
    
//  float32x4_t vLFOOut = wave4x(sLFOSamplePtrs, *(float32x4_t *)&sLFOSizes, *(uint32x4_t *)&sLFOMasks, sLFOPhase);
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
  vPosition += vreinterpretq_f32_u32(vbicq_u32(
    vreinterpretq_u32_f32(sDimensionMax),
    vcgeq_f32(vPosition, vdupq_n_f32(0.f))
  )); // Underflow wrap
  uint32x4_t vp0 = vcvtq_u32_f32(vPosition);
  uint32x4_t vp1 = vp0 + 1;
  float32x4_t vp0f = vcvtq_f32_u32(vp0);
  float32x4_t vp1f = vcvtq_f32_u32(vp1);
  float32x4_t vFractions1 = vPosition - vp0f;
  float32x4_t vFractions0 = 1.f - vFractions1;
/*
  float32x4_t vFractions0 = vreinterpretq_f32_u32(vbicq_u32(
    vreinterpretq_u32_f32(1.f - vFractions1),
    vtstq_u32(vreinterpretq_u32_f32(sDimension), vreinterpretq_u32_f32(vdupq_n_f32(1.f)))
//    vceqq_f32(sDimension, vdupq_n_f32(1.f))
  )); // Dimention = 1 workaround
*/
/*  float32x4_t vFractions1 = vbslq_f32(vandq_u32(sFraction0Mask, vceqq_f32(vPosition, vp0f)),
    vdupq_n_f32(1.f),
    vPosition - vp0f
  );
  float32x4_t vFractions0 = vreinterpretq_f32_u32(vbicq_u32(
    vreinterpretq_u32_f32(1.f - vFractions1),
    sFraction0Mask
  )); // Dimention = 1 workaround
*/
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
  /*
      int32_t folds;
      float tpos;
      float pos[4];
      for (uint32_t i = 0; i < DIMENSION_COUNT; i++) {
  //      if (sLfoMode[i] == lfo_type_one_shot && sLfo.getx1y0(i) > sLfo.getp0(i))
  //        sLfo.stop(i);
        switch (sLfoWave[i]) {
          case lfo_wave_sawtooth:
            pos[i] = sLfo.p0[i];
            break;
          case lfo_wave_triangle:
            pos[i] = 2 * (abs(sLfo.p0[i]) - 0x40000000);
            break;
          case lfo_wave_square:
            pos[i] = sLfo.p0[i] < 0 ? 0x80000000 : 0x7FFFFFFF;
            break;
          case lfo_wave_sine:
            pos[i] = 4 * sLfo.p0[i] * (abs(sLfo.p0[i]) + 0x80000000);
            break;
        }
      }
  */
  /*
      vst1q_f32(pos, vmulq_f32(vdupq_n_f32(INTMAX_RECIP), vcvtq_f32_s32( // convert LFO output to float
        vbslq_s32(maskWaveSine, // if sine
          vmulq_s32(vdupq_n_s32(4), vmulq_s32(sLfo.p0, vaddq_s32(vqabsq_s32(sLfo.p0), vdupq_n_s32(INT_MIN)))), // sine value = 4 * phase * (abs(phase) - 1)
          vbslq_s32(maskWaveSquare, // else if square
            vbslq_s32(vcltq_s32(sLfo.p0, vdupq_n_s32(0)), vdupq_n_s32(INT_MIN), vdupq_n_s32(INT_MAX)), // square out = phase < 0 ? -1 : 1
            vbslq_s32(maskWaveTriangle, // else if triangle
              vmlaq_n_s32(vdupq_n_s32(INT_MIN), vqabsq_s32(sLfo.p0), 2), // triangle out = 2 * abs(phase) - 1
              sLfo.p0 // else saw out = phase
  //To do: S&H
            )
          )
        )
      )));
  */
  //    vst1q_f32(pos, vmlaq_f32(vld1q_f32(sPosition), vld1q_f32(sLfoDepth), vld1q_f32(pos)));
  /*
      vst1q_f32(pos, vpos);

      for (uint32_t i = 0; i < DIMENSION_COUNT; i++) {
        switch (sLfoOverflow[i]) {
          case lfo_overflow_saturate:
            if (pos[i] > sDimensionMax[i])
              pos[i] = sDimensionMax[i];
            else if (pos[i] < 0.f)
              pos[i] = 0.f;
            break;
          case lfo_overflow_wrap:
            folds = (int32_t)(pos[i] * sDimensionRecip[i]);
            tpos = pos[i] - sDimension[i] * folds;
  //todo: pos+1 might overflow and needs to be handled here
            if (pos[i] >= sDimension[i])
              pos[i] = tpos;
            else if (pos[i] < 0.f)
              pos[i] = sDimension[i] + tpos;
            break;
          case lfo_overflow_fold:
            folds = (int32_t)(pos[i] * sDimensionMaxRecip[i]);
            tpos = pos[i] - sDimensionMax[i] * folds;
            if (pos[i] > sDimensionMax[i]) {
  //todo: single fold pos+1 and fraction needs to be inverted
              if (folds & 1)
                pos[i] = sDimensionMax[i] - tpos;
              else
                pos[i] = tpos;
            } else if (pos[i] < 0.f) {
              if (folds & 1)
                pos[i] = sDimensionMax[i] + tpos;
              else
                pos[i] = -tpos;
            }
            break;
        }
      }
  */
/*
  float32x4_t vpos = sPosition + sLfoDepth * sLfo.get();
  //    float32x4_t posSaturate = vmaxq_f32(vminq_f32(sDimensionMax, vpos), vdup_n_f32(0.f));
  int32x4_t folds = vcvtq_s32_f32(vmulq_f32(vpos, sDimensionRecip));
  float32x4_t tpos = vmlsq_f32(vpos, sDimension, vcvtq_f32_s32(folds));
  float32x4_t posWrap = vbslq_f32(
      vcltq_s32(folds, vdupq_n_s32(0)),  // if folds < 0
      vaddq_f32(sDimension, tpos),       // select max - remainder
      tpos                               // else - use current
  );

  folds = vcvtq_s32_f32(vmulq_f32(vpos, sDimensionMaxRecip));
  tpos = vmlsq_f32(vpos, sDimensionMax, vcvtq_f32_s32(folds));
  tpos = vbslq_f32(vcltq_f32(tpos, vdupq_n_f32(0.f)), vnegq_f32(tpos), tpos);
  float32x4_t posFold = vbslq_f32(
      vtstq_s32(folds, vdupq_n_s32(1)),  // folds & 1
      vsubq_f32(sDimensionMax, tpos),    // select max - remainder
      tpos                               // else - use current
  );

  vpos = vbslq_f32(sLfo.maskOverflowFold,            // if fold
                   posFold,                          // use folded
                   vbslq_f32(sLfo.maskOverflowWrap,  // else if wrap
                             posWrap,                // use wrapped
                             vpos                    // else use saturated, limits applied to integers
                             ));

  uint32x4_t vpos0 = vcvtq_u32_f32(vpos);                         // 4 axis integer wave position
  float32x4_t fractions = vsubq_f32(vpos, vcvtq_f32_u32(vpos0));  // 4 axis wave position fractional part
                                                                  // ToDo: wrap mode pos+1 overflow to 0
  // ToDo: remove sDimensionMax conversion
  //     uint32x4_t vpos1 = vaddq_u32(vdupq_n_u32(sSampleOffset), vmulq_u32(sDimensionScale, vmaxq_u32(vdupq_n_u32(0), vminq_u32(vcvtq_u32_f32(sDimensionMax), vaddq_u32(vpos0, vdupq_n_u32(1)))))); // 4 axis offset of wave to interpolate with (wave position + 1 limited with sDimensionMax and scaled with offset increment per axis and sample data base offset added)
  uint32x4_t vpos1 = vmaxq_u32(vdupq_n_u32(0), vminq_u32(vcvtq_u32_f32(sDimensionMax), vaddq_u32(vpos0, vdupq_n_u32(1))));  // 4 axis offset of wave to interpolate with (wave position + 1 limited with sDimensionMax and scaled with offset increment per axis and sample data base offset added)
  vpos0 = sSampleOffset + sDimensionScale * vpos0;                                                                          // 4 axis offset of wave to interpolate with
  vpos1 = sSampleOffset + sDimensionScale * vpos1;

  uint32x2_t w = {vpos0[3], vpos1[3]};
  uint32x4_t z = vaddq_u32(vcombine_u32(w, w), vcombine_u32(vdup_lane_u32(vget_high_u32(vpos0), 0), vdup_lane_u32(vget_high_u32(vpos1), 0)));
  uint32x4_t y0 = vaddq_u32(z, vdupq_lane_u32(vget_low_u32(vpos0), 1));
  uint32x4_t y1 = vaddq_u32(z, vdupq_lane_u32(vget_low_u32(vpos1), 1));
  uint32x4_t x0 = vdupq_lane_u32(vget_low_u32(vpos0), 0);
  uint32x4_t x1 = vdupq_lane_u32(vget_low_u32(vpos1), 0);
  switch (sSample->channels) {
    case 1:
      for (; out_p != out_e; out_p += 2) {
        float notePhaseFrac = (sNotePhase & sWaveSizeMask) * sWaveSizeRecip;
        uint32_t notePhaseOffs0 = sNotePhase >> sWaveSizeExpRes;
        uint32_t notePhaseOffs1 = (notePhaseOffs0 + 1) & sWaveSizeMask;
        notePhaseOffs0 <<= 2;
        notePhaseOffs1 <<= 2;
*/  
        /*
                  vst1_f32(out_p, vdup_n_f32(sAmp *
                    linint_f32x2(
                      vlinint_n_f32x2(
                        vlinintq_n_f32(
                          vlinintq_n_f32(
                            vlinintq_n_f32_indirect(x0y0, notePhaseOffs0, notePhaseOffs1, notePhaseFrac),
                            vlinintq_n_f32_indirect(x1y0, notePhaseOffs0, notePhaseOffs1, notePhaseFrac),
                            vgetq_lane_f32(fractions, 0)
                          ),
                          vlinintq_n_f32(
                            vlinintq_n_f32_indirect(x0y1, notePhaseOffs0, notePhaseOffs1, notePhaseFrac),
                            vlinintq_n_f32_indirect(x1y1, notePhaseOffs0, notePhaseOffs1, notePhaseFrac),
                            vgetq_lane_f32(fractions, 0)
                          ),
                          vgetq_lane_f32(fractions, 1)
                        ),
                        vgetq_lane_f32(fractions, 2)
                      ),
                      vgetq_lane_f32(fractions, 3)
                    )
                  ));
        */
/*
        float32x4_t f0, f1, f2, f3, f4, f5, f6, f7;
        float32x4_t l0, l1, l2, l3, l4, l5, l6;
        float32x2_t l7, ll8;
        f0 = vld1q_f32_indirect(f0, x0 + y0 + notePhaseOffs0);
        f1 = vld1q_f32_indirect(f1, x0 + y0 + notePhaseOffs1);
        f2 = vld1q_f32_indirect(f2, x1 + y0 + notePhaseOffs0);
        f3 = vld1q_f32_indirect(f3, x1 + y0 + notePhaseOffs1);
        f4 = vld1q_f32_indirect(f4, x0 + y1 + notePhaseOffs0);
        f5 = vld1q_f32_indirect(f5, x0 + y1 + notePhaseOffs1);
        f6 = vld1q_f32_indirect(f6, x1 + y1 + notePhaseOffs0);
        f7 = vld1q_f32_indirect(f7, x1 + y1 + notePhaseOffs1);
        l0 = vlinintq_f32(f0, f1, notePhaseFrac);
        l1 = vlinintq_f32(f2, f3, notePhaseFrac);
        l2 = vlinintq_f32(f4, f5, notePhaseFrac);
        l3 = vlinintq_f32(f6, f7, notePhaseFrac);
        l4 = vlinintq_lane_f32(l0, l1, vget_low_f32(fractions), 0);
        l5 = vlinintq_lane_f32(l2, l3, vget_low_f32(fractions), 0);
        l6 = vlinintq_lane_f32(l4, l5, vget_low_f32(fractions), 1);
        l7 = vlinint_lane_f32(vget_low_f32(l6), vget_high_f32(l6), vget_high_f32(fractions), 0);
        ll8 = vlinint_lane_f32(vtrn_f32(l7, l7).val[0], vtrn_f32(l7, l7).val[1], vget_high_f32(fractions), 1);
        vst1_f32(out_p, sAmp * ll8);
        sNotePhase += sNotePhaseIncrement;
        sLfo.advance();
      }
      break;
    case 2:
      for (; out_p != out_e; out_p += 2) {
        float notePhaseFrac = (sNotePhase & sWaveSizeMask) * sWaveSizeRecip;
        uint32_t notePhaseOffs0 = sNotePhase >> sWaveSizeExpRes;
        uint32_t notePhaseOffs1 = (notePhaseOffs0 + 1) & sWaveSizeMask;
        notePhaseOffs0 <<= 3;
        notePhaseOffs1 <<= 3;
        float32x4x2_t f0, f1, f2, f3, f4, f5, f6, f7;
        float32x4_t l0, l1, l2, l3, l4, l5, l6;
        float32x4_t r0, r1, r2, r3, r4, r5, r6;
        float32x4_t lr7;
        float32x2_t lr8;
        f0 = vld2q_f32_indirect(x0 + y0 + notePhaseOffs0);
        f1 = vld2q_f32_indirect(x0 + y0 + notePhaseOffs1);
        f2 = vld2q_f32_indirect(x1 + y0 + notePhaseOffs0);
        f3 = vld2q_f32_indirect(x1 + y0 + notePhaseOffs1);
        f4 = vld2q_f32_indirect(x0 + y1 + notePhaseOffs0);
        f5 = vld2q_f32_indirect(x0 + y1 + notePhaseOffs1);
        f6 = vld2q_f32_indirect(x1 + y1 + notePhaseOffs0);
        f7 = vld2q_f32_indirect(x1 + y1 + notePhaseOffs1);
        l0 = vlinintq_f32(f0.val[0], f1.val[0], notePhaseFrac);
        l1 = vlinintq_f32(f2.val[0], f3.val[0], notePhaseFrac);
        l2 = vlinintq_f32(f4.val[0], f5.val[0], notePhaseFrac);
        l3 = vlinintq_f32(f6.val[0], f7.val[0], notePhaseFrac);
        r0 = vlinintq_f32(f0.val[1], f1.val[1], notePhaseFrac);
        r1 = vlinintq_f32(f2.val[1], f3.val[1], notePhaseFrac);
        r2 = vlinintq_f32(f4.val[1], f5.val[1], notePhaseFrac);
        r3 = vlinintq_f32(f6.val[1], f7.val[1], notePhaseFrac);
        l4 = vlinintq_lane_f32(l0, l1, vget_low_f32(fractions), 0);
        l5 = vlinintq_lane_f32(l2, l3, vget_low_f32(fractions), 0);
        r4 = vlinintq_lane_f32(r0, r1, vget_low_f32(fractions), 0);
        r5 = vlinintq_lane_f32(r2, r3, vget_low_f32(fractions), 0);
        l6 = vlinintq_lane_f32(l4, l5, vget_low_f32(fractions), 1);
        r6 = vlinintq_lane_f32(r4, r5, vget_low_f32(fractions), 1);
        lr7 = vlinintq_lane_f32(vcombine_f32(vget_low_f32(l6), vget_low_f32(r6)), vcombine_f32(vget_high_f32(l6), vget_high_f32(r6)), vget_high_f32(fractions), 0);
        lr8 = vlinint_lane_f32(vget_low_f32(lr7), vget_high_f32(lr7), vget_high_f32(fractions), 1);
        vst1_f32(out_p, sAmp * lr8);
        sNotePhase += sNotePhaseIncrement;
        sLfo.advance();
      }
      break;
    default:
      break;
  }
*/  
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
      sLFOPhaseIncrement[id] = value * LFO_RATE_SCALE;
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
      sDimensionSaturateLimit[id] = value - 1;
      sDimensionMax[id] = value * LFO_DEPTH_SCALE;
      sDimensionRecip[id] = 1.f / value;
      sFraction0Mask[id] = value == 1 ? 0xFFFFFFFF : 0;
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
  static const char * lfoOverflows = "WSF";
  static char s[UNIT_PARAM_NAME_LEN + 1];
  static const char * name;
  static uint32_t modes[DIMENSION_COUNT];

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
      sprintf(s, "%.3fHz", value * LFO_RATE_SCALE_DISPLAY);
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
  __attribute__((used)) static float ftempo = uq16_16_to_f32(tempo);
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
