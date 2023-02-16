#pragma once
/*
 *  File: synth.h
 *
 *  Tesseract Synth Class.
 *
 *
 *  2022-2023 (c) Oleg Burdaev
 *  mailto: dukesrg@gmail.com
 *
 */

#include <atomic>
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
#include "fastexp.h"

#define SAMPLERATE_RECIP 2.0833333e-5f  // 1/48000
#define OCTAVE_RECIP .083333333f        // 1/12
//#define NOTE_A4 69
//#define SAMPLERATE_RECIP_X_440HZ .009166666f // 440/48000
#define NOTE_FREQ_OFFSET -150.232645f  // 12 * log2(440/48000) - 69

//#define TO_F32 4.65661287e-10f

#define VELOCITY_SENSITIVITY .0078740157f        // 1/127
#define CONTROL_CHANGE_SENSITIVITY .0078740157f  // 1/127
#define AFTERTOUCH_SENSITIVITY .0078740157f      // 1/127
#define PITCH_BEND_CENTER 8192
#define PITCH_BEND_SENSITIVITY .0001220703125f  // 24/8192
#define DIMENSION_COUNT 4
#define POSITION_SCALE .001953125f  // 1/512
//#define LFO_RATE_SCALE .001f
//#define LFO_RATE_SCALE 89.478485f    // 2^32 / 48000 / 1000
#define LFO_RATE_SCALE 699.05067f    // 2^32 / 48000 / 128
#define LFO_RATE_SCALE_TEMPO .022755555f // 2^32 / 48000 / 60 / 2^16 - for 1/4
//#define LFO_DEPTH_SCALE .001953125f  // 1/512
#define LFO_DEPTH_SCALE 9.0949470e-13f  // 1 / 2^31 / 512
//#define UINTMAX_RECIP 2.3283064e-10f
#define INTMAX_RECIP 4.65661287e-10f

#define LFO_MODE_SAMPLE_AND_HOLD 48

#define USER_BANK 5

#define lfoType(a) (a & 3)
#define lfoWave(a) ((a >> 2) & 3)
#define lfoOverflow(a) ((a >> 4) & 3)

#define ldq_f32(a,b) (*(float*)((a)[b]))
#define vmovq_x4_f32(f,a,b,c,d) vsetq_lane_f32(d, vsetq_lane_f32(c, vsetq_lane_f32(b, vsetq_lane_f32(a, f, 0), 1), 2), 3);
#define vld1q_f32_indirect(f,a) vmovq_x4_f32(f, ldq_f32(a, 0), ldq_f32(a, 1), ldq_f32(a, 2), ldq_f32(a, 3))
#define vlinintq_f32(a,b,c) (a + (b - a) * c) //vfmaq_f32
#define vlinint_f32(a,b,c) (a + (b - a) * c) //vfma_f32
#define vlinintq_lane_f32(a,b,c,d) vmlaq_lane_f32(a, b - a, c, d)
#define vlinint_lane_f32(a,b,c,d) vmla_lane_f32(a, b - a, c, d)

#define ld_f32(a,b) vld1_f32((const float*)((a)[b]))
#define vld2q_f32_indirect(a) vuzpq_f32(vcombine_f32(ld_f32(a, 0), ld_f32(a, 1)), vcombine_f32(ld_f32(a, 2), ld_f32(a, 3)))

//#define vaddq_dupq_n_u32(a,b) vaddq_u32(a, vdupq_n_u32(b))
//#define u32x4(a,b,c,d) vsetq_lane_u32(d, vsetq_lane_u32(c, vsetq_lane_u32(b, vdupq_n_u32(a), 1), 2), 3)
//#define ldf32(a,b) (*(float*)vgetq_lane_u32(a, b))
//#define vsetq_x4_f32(a,b,c,d) vsetq_lane_f32(d, vsetq_lane_f32(c, vsetq_lane_f32(b, vdupq_n_f32(a), 1) , 2), 3)
//#define vmovq_x4_f32(a,b,c,d) vsetq_lane_f32(d, vsetq_lane_f32(c, vsetq_lane_f32(b, vdupq_n_f32(a), 1) , 2), 3)
//#define vmovq_x4_u32(a,b,c,d) vsetq_lane_u32(d, vsetq_lane_u32(c, vsetq_lane_u32(b, vdupq_n_u32(a), 1) , 2), 3)
//#define vmovq_x4_s32(a,b,c,d) vsetq_lane_s32(d, vsetq_lane_s32(c, vsetq_lane_s32(b, vdupq_n_s32(a), 1) , 2), 3)
//#define vld1q_f32_indirect(a) vmovq_x4_f32(ld_f32(a, 0), ld_f32(a, 1), ld_f32(a, 2), ld_f32(a, 3))
//#define vlinintq_n_f32_indirect(a,b,c,d) vlinintq_n_f32(vld1q_f32_indirect(vaddq_dupq_n_u32(a, b)), vld1q_f32_indirect(vaddq_dupq_n_u32(a, c)), d)
//#define vlinintq_n_f32(a,b,c) vmlaq_f32(a, vsubq_f32(b, a), vdupq_n_f32(c))
//#define vlinintq_lane_f32(a,b,c,d) vmlaq_lane_f32(a, vsubq_f32(b, a), c, d)
//#define vlinint_n_f32(a,b,c) vmla_f32(a, vsub_f32(b, a), vdup_n_f32(c))
//#define vlinint_n_f32x2(a,b) (vlinint_n_f32(vget_low_f32(a), vget_high_f32(a), b))
//#define linint_f32(a,b,c) (a + (b - a) * c)
//#define linint_f32x2(a,b) linint_f32(vget_lane_f32(a, 0), vget_lane_f32(a, 1), b)
//#define veorshlq_u32(a,b) veorq_u32(a, vshlq_n_u32(a, b))
//#define veorshrq_u32(a,b) veorq_u32(a, vshrq_n_u32(a, b))
//#define veorshlq_s32(a,b) veorq_s32(a, vshlq_n_s32(a, b))
//#define veorshrq_s32(a,b) veorq_s32(a, vshrq_n_s32(a, b))

enum {
  param_position_x = 0U,
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
  param_lfo_mode_x,
  param_lfo_mode_y,
  param_lfo_mode_z,
  param_lfo_mode_w,
  param_dimension_x,
  param_dimension_y,
  param_dimension_z,
  param_dimension_w,
//  param_wave_bank_idx,
  param_wave_sample_idx,
  param_wave_size,
  param_wave_offset,
};

enum {
  lfo_type_one_shot = 0U,
  lfo_type_key_trigger,
  lfo_type_random,
  lfo_type_free_run,
  lfo_type_sample_and_hold
};

enum {
  lfo_wave_sawtooth = 0U,
  lfo_wave_triangle,
  lfo_wave_square,
  lfo_wave_sine,
};

enum {
  lfo_overflow_saturate = 0U,
  lfo_overflow_wrap,
  lfo_overflow_fold,
};

class Synth {
 public:
  /*===========================================================================*/
  /* Public Data Structures/Types. */
  /*===========================================================================*/

  /*===========================================================================*/
  /* Lifecycle Methods. */
  /*===========================================================================*/

  Synth(void) {}
  ~Synth(void) {}

  inline int8_t Init(const unit_runtime_desc_t * desc) {
    // Check compatibility of samplerate with unit, for drumlogue should be 48000
    if (desc->samplerate != 48000)
      return k_unit_err_samplerate;

    // Check compatibility of frame geometry
    if (desc->output_channels != 2)  // should be stereo output
      return k_unit_err_geometry;

    sVelocity = 0.f;
    sPressure = 1.f;
    sAmp = vdup_n_f32(0.f);
    sNotePhase = 0;

//    get_num_sample_banks = desc->get_num_sample_banks;
//    get_num_samples_for_bank = desc->get_num_samples_for_bank;
    get_sample = desc->get_sample;

    sWaveSize = 1;
    sWaveSizes = vdupq_n_u32(1);
    sWaveSizeExp = 0;
    sWaveSizeExpRes = 32;
    sWaveOffset = 0;
    sSample = get_sample(USER_BANK, 0);
    setSampleOffset();

    sLfo.init();
    sDimensionScale = vdupq_n_u32(1);

    // Note: if need to allocate some memory can do it here and return k_unit_err_memory if getting allocation errors

    return k_unit_err_none;
  }

  inline void Teardown() {
    // Note: cleanup and release resources if any
  }

  inline void Reset() {
    // Note: Reset synth state. I.e.: Clear filter memory, reset oscillator
    // phase etc.
    sVelocity = 0.f;
    sPressure = 1.f;
    sAmp = vdup_n_f32(0.f);
    sNotePhase = 0;

    sLfo.init();
  }

  inline void Resume() {
    // Note: Synth will resume and exit suspend state. Usually means the synth
    // was selected and the render callback will be called again
  }

  inline void Suspend() {
    // Note: Synth will enter suspend state. Usually means another synth was
    // selected and thus the render callback will not be called
  }

  /*===========================================================================*/
  /* Other Public Methods. */
  /*===========================================================================*/

  fast_inline void Render(float * out, size_t frames) {
    float * __restrict out_p = out;
    const float * out_e = out_p + (frames << 1);  // assuming stereo output

//    if (sSample == NULL) {
      for (; out_p != out_e; out_p += 2) {
//        vst1_f32(out_p, vdup_n_f32(0.f));
        vst1_f32(out_p, vdup_lane_f32(vget_low_f32(sLfo.get()), 0));
        sLfo.advance();
      }
      return;
//    }
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

    float32x4_t vpos = sPosition + sLfoDepth * sLfo.get();
//    float32x4_t posSaturate = vmaxq_f32(vminq_f32(sDimensionMax, vpos), vdup_n_f32(0.f));
    int32x4_t folds = vcvtq_s32_f32(vmulq_f32(vpos, sDimensionRecip));
    float32x4_t tpos = vmlsq_f32(vpos, sDimension, vcvtq_f32_s32(folds));
    float32x4_t posWrap = vbslq_f32(
      vcltq_s32(folds, vdupq_n_s32(0)), // if folds < 0
      vaddq_f32(sDimension, tpos), // select max - remainder
      tpos // else - use current
    );

    folds = vcvtq_s32_f32(vmulq_f32(vpos, sDimensionMaxRecip));
    tpos = vmlsq_f32(vpos, sDimensionMax, vcvtq_f32_s32(folds));
    tpos = vbslq_f32(vcltq_f32(tpos, vdupq_n_f32(0.f)), vnegq_f32(tpos), tpos);
    float32x4_t posFold = vbslq_f32(
      vtstq_s32(folds, vdupq_n_s32(1)), // folds & 1
      vsubq_f32(sDimensionMax, tpos), // select max - remainder
      tpos // else - use current
    );          

    vpos = vbslq_f32(sLfo.maskOverflowFold, // if fold
      posFold, // use folded
      vbslq_f32(sLfo.maskOverflowWrap, // else if wrap
        posWrap, // use wrapped
        vpos // else use saturated, limits applied to integers
      )
    );

    uint32x4_t vpos0 = vcvtq_u32_f32(vpos); // 4 axis integer wave position
    float32x4_t fractions = vsubq_f32(vpos, vcvtq_f32_u32(vpos0)); // 4 axis wave position fractional part
//ToDo: wrap mode pos+1 overflow to 0
//ToDo: remove sDimensionMax conversion
//    uint32x4_t vpos1 = vaddq_u32(vdupq_n_u32(sSampleOffset), vmulq_u32(sDimensionScale, vmaxq_u32(vdupq_n_u32(0), vminq_u32(vcvtq_u32_f32(sDimensionMax), vaddq_u32(vpos0, vdupq_n_u32(1)))))); // 4 axis offset of wave to interpolate with (wave position + 1 limited with sDimensionMax and scaled with offset increment per axis and sample data base offset added)
    uint32x4_t vpos1 = vmaxq_u32(vdupq_n_u32(0), vminq_u32(vcvtq_u32_f32(sDimensionMax), vaddq_u32(vpos0, vdupq_n_u32(1)))); // 4 axis offset of wave to interpolate with (wave position + 1 limited with sDimensionMax and scaled with offset increment per axis and sample data base offset added)
    vpos0 = sSampleOffset + sDimensionScale * vpos0; // 4 axis offset of wave to interpolate with
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
  }

  inline void setParameter(uint8_t index, int32_t value) {
    sParams[index] = value;
//    uint32_t tmp;
    switch (index) {
      case param_position_x:
      case param_position_y:
      case param_position_z:
      case param_position_w:
        index &= DIMENSION_COUNT - 1;
        sPosition[index] = value * POSITION_SCALE;
        break;
      case param_lfo_rate_x:
      case param_lfo_rate_y:
      case param_lfo_rate_z:
      case param_lfo_rate_w:
        index &= DIMENSION_COUNT - 1;
        sLfo.setIncrement(index, value * LFO_RATE_SCALE);
        break;
      case param_lfo_depth_x:
        sLfoDepth = vsetq_lane_f32(value * LFO_DEPTH_SCALE, sLfoDepth, 0);
        break;
      case param_lfo_depth_y:
        sLfoDepth = vsetq_lane_f32(value * LFO_DEPTH_SCALE, sLfoDepth, 1);
        break;
      case param_lfo_depth_z:
        sLfoDepth = vsetq_lane_f32(value * LFO_DEPTH_SCALE, sLfoDepth, 2);
        break;
      case param_lfo_depth_w:
        sLfoDepth = vsetq_lane_f32(value * LFO_DEPTH_SCALE, sLfoDepth, 3);
        break;
      case param_lfo_mode_x:
      case param_lfo_mode_y:
      case param_lfo_mode_z:
      case param_lfo_mode_w:
        index &= DIMENSION_COUNT - 1;
        sLfo.setMode(index, value);
/*
        if (value != LFO_MODE_SAMPLE_AND_HOLD) {
          sLfoMode[index] = lfoMode(value);
          sLfoWave[index] = lfoWave(value);
          sLfoOverflow[index] = lfoOverflow(value);
        } else
          sLfoMode[index] = lfo_typsample_and_hold;
*/
        break;
      case param_dimension_x:
        sDimensions[0] = 1;
      case param_dimension_y:
        sDimensions[1] = sDimensions[0] * sParams[param_dimension_x];
      case param_dimension_z:
        sDimensions[2] = sDimensions[1] * sParams[param_dimension_y];
      case param_dimension_w:
        sDimensions[3] = sDimensions[2] * sParams[param_dimension_z];
        index &= DIMENSION_COUNT - 1;
        sDimension[index] = value;
        sDimensionMax = sDimension - 1.f;
        sDimensionRecip = 1.f / sDimension;
        sDimensionMaxRecip = 1.f / sDimensionMax;
        sDimensionScale = sDimensions * sWaveSizes * sSample->channels * sizeof(float);
        break;
//      case param_wave_bank_idx:
      case param_wave_sample_idx:
//        tmp = get_num_sample_banks();
//        if (sParams[param_wave_bank_idx] >= tmp)
//          sParams[param_wave_bank_idx] = tmp;
//        tmp = get_num_samples_for_bank(sParams[param_wave_sample_idx]);
//        if (sParams[param_wave_sample_idx] >= tmp)
//          sParams[param_wave_sample_idx] = tmp;
//        sSample = get_sample(sParams[param_wave_bank_idx], sParams[param_wave_sample_idx]);
        sSample = get_sample(USER_BANK + (sParams[param_wave_sample_idx] >> 7), sParams[param_wave_sample_idx] & 0x7F);
      case param_wave_size:
        sWaveSizeExp = sParams[param_wave_size];
        sWaveSizeExpRes = 32 - sWaveSizeExp;
        sWaveSizeMask = sWaveSize - 1;
        sWaveSize = 1 << sWaveSizeExp;
        sWaveSizeRecip = 1.f / sWaveSize;
        sWaveSizes = vsetq_lane_u32(1, vdupq_n_u32(sWaveSize), 0);
        sDimensionScale = sDimensions * sWaveSizes * (sSample == NULL ? 1 : sSample->channels) * sizeof(float);
      case param_wave_offset:
        sWaveOffset = sParams[param_wave_offset];
        if ((sSample != NULL) && (sSample->frames >> sWaveSize) <= sWaveOffset) {
          sWaveOffset = (sSample->frames >> sWaveSize) - 1;
          sParams[param_wave_offset] = sWaveOffset;
        }
        setSampleOffset();
        sWaveMaxIndex = sSample == NULL ? 0 : (sSample->frames >> sWaveSizeExp) - sWaveOffset - 1;
        break;
      default:
        break;
    }
  }

  inline int32_t getParameterValue(uint8_t index) const {
    /*
        switch (index) {
            break;
          default:
            break;
        }
    */
    return sParams[index];
  }

  inline const char * getParameterStrValue(uint8_t index, int32_t value) const {
    static const char * lfoTypeNames[] = {
        "One",
        "Key",
        "Rnd",
        "Run",
        "S&H",
    };

    static const char * lfoWaveNames[] = {
        "Sw",
        "Tr",
        "Sq",
        "Si",
    };

    static const char * lfoOverflowNames[] = {
        "S",
        "W",
        "F",
    };

    static char s[UNIT_PARAM_NAME_LEN + 1];

    switch (index) {
      case param_lfo_mode_x:
      case param_lfo_mode_y:
      case param_lfo_mode_z:
      case param_lfo_mode_w:
        if (value != LFO_MODE_SAMPLE_AND_HOLD) {
          sprintf(s, "%s.%s.%s", lfoTypeNames[lfoType(value)], lfoWaveNames[lfoWave(value)], lfoOverflowNames[lfoOverflow(value)]);
          return s;
        } else
          return lfoTypeNames[4];
      case param_wave_sample_idx:
        return sSample == NULL ? "---" : sSample->name;
      case param_wave_size:
        sprintf(s, "%d", 1 << value);
        return s;
      default:
        break;
    }
    return nullptr;
  }

  inline const uint8_t * getParameterBmpValue(uint8_t index, int32_t value) const {
    (void)value;
    switch (index) {
      // Note: Bitmap memory must be accessible even after function returned.
      //       It can be assumed that caller will have copied or used the bitmap
      //       before the next call to getParameterBmpValue
      // Note: Not yet implemented upstream
      default:
        break;
    }
    return nullptr;
  }

  inline void setTempo(uint32_t tempo) {
    float ftempo = (float)tempo * LFO_RATE_SCALE_TEMPO;    
  } 

  inline void NoteOn(uint8_t note, uint8_t velocity) {
    sNote = note;
    sNotePhaseIncrement = (float)UINT_MAX * fastpow2((sNote + sPitchBend + NOTE_FREQ_OFFSET) * OCTAVE_RECIP);  // f = 2^((note - A4)/12), A4 = #69 = 440Hz
    sNotePhase = 0;
    sVelocity = velocity * VELOCITY_SENSITIVITY;
    sAmp = vdup_n_f32(sVelocity * sPressure);
    sLfo.trigger();
/*    
    for (uint32_t i = 0; i < DIMENSION_COUNT; i++) {
      switch (sLfoMode[i]) {
        case lfo_type_one_shot:
        case lfo_type_key_trigger:
          sLfo.reset(i);
          break;
        case lfo_type_random:
          sLfo.reset(i, rand() << 1); // assume GCC RAND_MAX = INT_MAX
          break;
        case lfo_type_free_run:
        default:
          break;
      }
    }
*/
  }

  inline void NoteOff(uint8_t note) {
    (void)note;
    sAmp = vdup_n_f32(0.f);
  }

  inline void GateOn(uint8_t velocity) {
    sVelocity = velocity * VELOCITY_SENSITIVITY;
    sAmp = vdup_n_f32(sVelocity * sPressure);
  }

  inline void GateOff() {
    sAmp = vdup_n_f32(0.f);
  }

  inline void AllNoteOff() {
    sAmp = vdup_n_f32(0.f);
  }

  inline void PitchBend(uint16_t bend) {
    sPitchBend = (bend - PITCH_BEND_CENTER) * PITCH_BEND_SENSITIVITY;
    sNotePhaseIncrement = (float)UINT_MAX * fastpow2((sNote + sPitchBend + NOTE_FREQ_OFFSET) * OCTAVE_RECIP);
  }

  inline void ChannelPressure(uint8_t pressure) {
    sPressure = pressure * CONTROL_CHANGE_SENSITIVITY;
    sAmp = vdup_n_f32(sVelocity * sPressure);
  }

  inline void Aftertouch(uint8_t note, uint8_t aftertouch) {
    (void)note;
    sVelocity = aftertouch * AFTERTOUCH_SENSITIVITY;
    sAmp = vdup_n_f32(sVelocity * sPressure);
  }

  inline void LoadPreset(uint8_t idx) { (void)idx; }

  inline uint8_t getPresetIndex() const { return 0; }

  /*===========================================================================*/
  /* Static Members. */
  /*===========================================================================*/

  static inline const char * getPresetName(uint8_t idx) {
    // Note: String memory must be accessible even after function returned.
    //       It can be assumed that caller will have copied or used the string
    //       before the next call to getPresetName
    return nullptr;
  }

 private:
  /*===========================================================================*/
  /* Private Member Variables. */
  /*===========================================================================*/

  std::atomic_uint_fast32_t flags_;

  uint8_t sNote;
  float sVelocity;
  float sPressure;
  float32x2_t sAmp;
  uint32_t sNotePhase;
  uint32_t sNotePhaseIncrement;
  float sPitchBend;

  uint32_t sParams[PARAM_COUNT];
  float32x4_t sPosition;
  struct lfo_t {
    int32x4_t p0, p1, inc;
    uint32x4_t type, wave, overflow;
    uint32x4_t maskTypeOneShot, maskTypeTriggerOrOneShot, maskTypeRandom, maskTypeSnH;
    uint32x4_t maskWaveTriangle, maskWaveSquare, maskWaveSine;
    uint32x4_t maskOverflowSaturate, maskOverflowWrap, maskOverflowFold;
    uint32x4_t maskPhaseOverflow;
    int32x4_t SnHOut;
    uint32x4_t seed = vsetq_lane_u32(0x363812fd, vsetq_lane_u32(0xbe183480, vsetq_lane_u32(0x4740aef7, vdupq_n_u32(0x2a507c5d), 1) , 2), 3);

//get next random vector
    fast_inline int32x4_t random() {
      seed = veorq_u32(seed, vshlq_n_u32(seed, 13));
      seed = veorq_u32(seed, vshrq_n_u32(seed, 17));
      seed = veorq_u32(seed, vshlq_n_u32(seed, 5));
      return vreinterpretq_s32_u32(seed);
    }
//Set LFO phases and increments to initial values
    force_inline void init() {
      p0 = vdupq_n_s32(INT_MIN);
      p1 = vdupq_n_s32(INT_MIN);
      inc = vdupq_n_s32(0);
    }
//Set LFO phases to initial values with respect to LFO modes
    force_inline void trigger() {
      p1 = p0 = vbslq_s32(maskTypeTriggerOrOneShot, // if one shot or trigger
        vdupq_n_s32(INT_MIN), // reset phase
        vbslq_s32(maskTypeRandom, // else if random
//          vmovq_x4_s32((uint32_t)rand() << 1, (uint32_t)rand() << 1, (uint32_t)rand() << 1, (uint32_t)rand() << 1),
          random(),
          p0 // else keep current
        )
      );
    }
//Increment LFO phases values
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
//Ser LFO phase increment value
    force_inline void setIncrement(const int index, int32_t value) {
      inc[index] = value;
    }
//Set LFO mode
    force_inline void setMode(uint32_t index, uint32_t value){
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
//Get LFO outputs
    force_inline float32x4_t get() {
      int32x4_t p0abs = vqabsq_s32(p0);
      return vmulq_f32(vdupq_n_f32(INTMAX_RECIP), vcvtq_f32_s32( // convert LFO output to float
        vbslq_s32(maskTypeSnH, SnHOut,
          vbslq_s32(maskWaveSine, // if sine
            vmulq_s32(vdupq_n_s32(4), vmulq_s32(p0, vaddq_s32(p0abs, vdupq_n_s32(INT_MIN)))), // sine value = 4 * phase * (abs(phase) - 1)
            vbslq_s32(maskWaveSquare, // else if square
              vbslq_s32(vcltq_s32(p0, vdupq_n_s32(0)), vdupq_n_s32(INT_MIN), vdupq_n_s32(INT_MAX)), // square out = phase < 0 ? -1 : 1
              vbslq_s32(maskWaveTriangle, // else if triangle
                vmlaq_n_s32(vdupq_n_s32(INT_MIN), p0abs, 2), // triangle out = 2 * abs(phase) - 1
                p0 // else saw out = phase
              )
            )
          )
        )
      ));
    }
  } sLfo;
  float32x4_t sLfoDepth;
//  uint32_t sLfoMode[DIMENSION_COUNT];
//  uint32_t sLfoWave[DIMENSION_COUNT];
//  uint32_t sLfoOverflow[DIMENSION_COUNT];
  float32x4_t sDimension;
  float32x4_t sDimensionMax;
  float32x4_t sDimensionRecip;
  float32x4_t sDimensionMaxRecip;
  uint32x4_t sDimensions;
  uint32x4_t sDimensionScale;

  unit_runtime_get_num_sample_banks_ptr get_num_sample_banks;
  unit_runtime_get_num_samples_for_bank_ptr get_num_samples_for_bank;
  unit_runtime_get_sample_ptr get_sample;
  const sample_wrapper_t * sSample;
  uint32_t sSampleOffset;
  uint32_t sWaveSize;
  uint32x4_t sWaveSizes;
  uint32_t sWaveSizeExp;
  uint32_t sWaveSizeExpRes;
  uint32_t sWaveSizeMask;
  float sWaveSizeRecip;
  uint32_t sWaveOffset;
  uint32_t sWaveMaxIndex;

  /*===========================================================================*/
  /* Private Methods. */
  /*===========================================================================*/
/*
inline float getSample(float *samples, uint32_t phase, uint32_t sizeExpRes, uint32_t sizeExpMask, float sizeRecip) {
  uint32_t x0 = phase >> sizeExpRes;
  uint32_t x1 = (x0 + 1) & sizeExpMask;
  float frac = (phase & sizeExpMask) * sizeRecip;
  float out = samples[x0] + (samples[x1] - samples[x0]) * frac;
  return out;
}

inline float getSample(float *samples, float phase, uint32_t size, uint32_t sizeExp, uint32_t sizeExpRes, uint32_t sizeExpMask, float sizeRecip) {
  float x = phase * size;
  uint32_t x0 = (uint32_t)x;
  uint32_t x1 = (x0 + 1) & sizeExpMask;
  float frac = x - x0;
  float out = samples[x0] + (samples[x1] - samples[x0]) * frac;
  return out;
}
*/
fast_inline void setSampleOffset() {
  sSampleOffset = sSample == NULL ? 0 : ((uint32_t)sSample->sample_ptr + sWaveOffset * sWaveSize * sSample->channels * sizeof(float));
}
/*
fast_inline void setDimensions(uint32_t index, uint32_t value) {
  sDimension[index] = value;
  sDimensionMax = sDimension - 1.f;
  sDimensionRecip = 1.f / sDimension;
  sDimensionMaxRecip = 1.f / sDimensionMax;
  sDimensionScale = sDimensions * sWaveSize * sSample->channels;
}
*/

  /*===========================================================================*/
  /* Constants. */
  /*===========================================================================*/
};
