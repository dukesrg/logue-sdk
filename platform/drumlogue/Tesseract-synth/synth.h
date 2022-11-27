#pragma once
/*
 *  File: synth.h
 *
 *  Tesseract Synth Class.
 *
 *
 *  2022 (c) Oleg Burdaev
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
#define LFO_RATE_SCALE 89.478485f    // 2^32 / 48000 / 1000
//#define LFO_DEPTH_SCALE .001953125f  // 1/512
#define LFO_DEPTH_SCALE 9.0949470e-13f  // 1 / 2^31 / 512
//#define UINTMAX_RECIP 2.3283064e-10f
#define INTMAX_RECIP 4.65661287e-10f

#define LFO_MODE_SAMPLE_AND_HOLD 48

#define lfoMode(a) (a & 3)
#define lfoWave(a) ((a >> 2) & 3)
#define lfoOverflow(a) ((a >> 4) & 3)

//#define u32x4(a,b,c,d) vsetq_lane_u32(d, vsetq_lane_u32(c, vsetq_lane_u32(b, vdupq_n_u32(a), 1), 2), 3)
//#define ldf32(a,b) (*(float*)vgetq_lane_u32(a, b))
//#define vsetq_x4_f32(a,b,c,d) vsetq_lane_f32(d, vsetq_lane_f32(c, vsetq_lane_f32(b, vdupq_n_f32(a), 1) , 2), 3)

#define vaddq_dupq_n_u32(a,b) vaddq_u32(a, vdupq_n_u32(b))
#define ld_f32(a,b) (*(float*)vgetq_lane_u32(a, b))
#define vmovq_x4_f32(a,b,c,d) vsetq_lane_f32(d, vsetq_lane_f32(c, vsetq_lane_f32(b, vdupq_n_f32(a), 1) , 2), 3)
#define vmovq_x4_u32(a,b,c,d) vsetq_lane_u32(d, vsetq_lane_u32(c, vsetq_lane_u32(b, vdupq_n_u32(a), 1) , 2), 3)
#define vmovq_x4_s32(a,b,c,d) vsetq_lane_s32(d, vsetq_lane_s32(c, vsetq_lane_s32(b, vdupq_n_s32(a), 1) , 2), 3)
#define vld1q_f32_indirect(a) vmovq_x4_f32(ld_f32(a, 0), ld_f32(a, 1), ld_f32(a, 2), ld_f32(a, 3))
#define vlinintq_n_f32_indirect(a,b,c,d) vlinintq_n_f32(vld1q_f32_indirect(vaddq_dupq_n_u32(a, b)), vld1q_f32_indirect(vaddq_dupq_n_u32(a, c)), d)

#define vlinintq_n_f32(a,b,c) vmlaq_f32(a, vsubq_f32(b, a), vdupq_n_f32(c))
#define vlinintq_lane_f32(a,b,c,d) vmlaq_lane_f32(a, vsubq_f32(b, a), c, d)
#define vlinint_n_f32(a,b,c) vmla_f32(a, vsub_f32(b, a), vdup_n_f32(c))
#define vlinint_n_f32x2(a,b) (vlinint_n_f32(vget_low_f32(a), vget_high_f32(a), b))
#define linint_f32(a,b,c) (a + (b - a) * c)
#define linint_f32x2(a,b) linint_f32(vget_lane_f32(a, 0), vget_lane_f32(a, 1), b)

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
  param_wave_bank_idx,
  param_wave_sample_idx,
  param_wave_size,
  param_wave_offset,
};

enum {
  lfo_mode_one_shot = 0U,
  lfo_mode_key_trigger,
  lfo_mode_random,
  lfo_mode_free_run,
  lfo_mode_sample_and_hold
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
    sAmp = 0.f;
    sNotePhase = 0;

    get_num_sample_banks = desc->get_num_sample_banks;
    get_num_samples_for_bank = desc->get_num_samples_for_bank;
    get_sample = desc->get_sample;

    sSample = get_sample(0, 0);
    sWaveSize = 1;
    sWaveSizeExp = 0;
    sWaveSizeExpRes = 32;
    sWaveOffset = 0;
    sSample = get_sample(0, 0);
    setSampleOffset();

    sLfoPhase.init();
    vst1q_u32(sDimensionScale, vdupq_n_u32(1));

    vst1q_u32(sDimensionScale, vmulq_n_u32(vld1q_u32(sDimensions), sWaveSize * sSample->channels));

    srand(time(0));

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
    sAmp = 0.f;
    sNotePhase = 0;

    sLfoPhase.init();
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

    int32_t folds;
    float tpos;
    float pos[4];
/*    
    for (uint32_t i = 0; i < DIMENSION_COUNT; i++) {
//      if (sLfoMode[i] == lfo_mode_one_shot && sLfoPhase.getP1(i) > sLfoPhase.getP0(i))
//        sLfoPhase.stop(i);
      switch (sLfoWave[i]) {
        case lfo_wave_sawtooth:
          pos[i] = sLfoPhase.p0[i];
          break;
        case lfo_wave_triangle:
          pos[i] = 2 * (abs(sLfoPhase.p0[i]) - 0x40000000);
          break;
        case lfo_wave_square:
          pos[i] = sLfoPhase.p0[i] < 0 ? 0x80000000 : 0x7FFFFFFF;
          break;
        case lfo_wave_sine:
          pos[i] = 4 * sLfoPhase.p0[i] * (abs(sLfoPhase.p0[i]) + 0x80000000);
          break;
      }
    }
*/
    vst1q_f32(pos, vmulq_f32(vdupq_n_f32(INTMAX_RECIP), vcvtq_f32_s32( // convert LFO output to float
      vbslq_s32(vceqq_u32(vdupq_n_u32(lfo_wave_sine), vld1q_u32(sLfoWave)), // if sine
        vmulq_s32(vdupq_n_s32(4), vmulq_s32(sLfoPhase.p0, vaddq_s32(vqabsq_s32(sLfoPhase.p0), vdupq_n_s32(INT_MIN)))), // sine value = 4 * phase * (abs(phase) - 1)
        vbslq_s32(vceqq_u32(vdupq_n_u32(lfo_wave_square), vld1q_u32(sLfoWave)), // else if square
          vbslq_s32(vcltq_s32(sLfoPhase.p0, vdupq_n_s32(0)), vdupq_n_s32(INT_MIN), vdupq_n_s32(INT_MAX)), // square out = phase < 0 ? -1 : 1
          vbslq_s32(vceqq_u32(vdupq_n_u32(lfo_wave_triangle), vld1q_u32(sLfoWave)), // else if triangle
            vmlaq_n_s32(vdupq_n_s32(INT_MIN), vqabsq_s32(sLfoPhase.p0), 2), // triangle out = 2 * abs(phase) - 1
            sLfoPhase.p0 // else saw out = phase
          )
        )
      )
    )));

    vst1q_f32(pos, vmlaq_f32(vld1q_f32(sPosition), vld1q_f32(sLfoDepth), vld1q_f32(pos)));

    for (uint32_t i = 0; i < DIMENSION_COUNT; i++) {

      switch (sLfoOverflow[i]) {
        case lfo_overflow_saturate:
          if (pos[i] > sDimensionMax[i])
            pos[i] = sDimensionMax[i];
          else if (pos[i] < 0.f)
            pos[i] = 0.f;
          break;
        case lfo_overflow_wrap:
          tpos = pos[i] - sDimension[i] * (int32_t)(pos[i] * sDimensionRecip[i]);
          if (pos[i] >= sDimension[i])
            pos[i] = tpos;
          else if (pos[i] < 0.f)
            pos[i] = sDimension[i] + tpos;
          break;
        case lfo_overflow_fold:
          folds = (int32_t)(pos[i] * sDimensionMaxRecip[i]);
          tpos = pos[i] - sDimensionMax[i] * folds;
          if (pos[i] > sDimensionMax[i]) {
            if (folds & 1)
              pos[i] = sDimensionMax[i] - tpos;
            else
              pos[i] = tpos;
          } else if (pos[i] < 0.f) {
            if (folds & 1)
              pos[i] = -tpos;
            else
              pos[i] = sDimensionMax[i] + tpos;
          }
          break;
      }
    }

    float32x2_t x2;

    float32x4_t vpos = vld1q_f32(pos); // 4 axis wave position
    uint32x4_t vscale = vld1q_u32(sDimensionScale); // 4 axis wave position offset increment (in samples, including channel interleave)
    uint32x4_t vpos0 = vcvtq_u32_f32(vpos); // 4 axis integer wave position
    float32x4_t fractions = vsubq_f32(vpos, vcvtq_f32_u32(vpos0)); // 4 axis wave position fractional part
    uint32x4_t vpos1 = vaddq_u32(vdupq_n_u32(sSampleOffset), vmulq_u32(vscale, vminq_u32(vld1q_u32(sDimensionMax), vaddq_u32(vpos0, vdupq_n_u32(1))))); // 4 axis offset of wave to interpolate with (wave position + 1 limited with sDimensionMax and scaled with offset increment per axis and sample data base offset added)
    vpos0 = vaddq_u32(vdupq_n_u32(sSampleOffset), vmulq_u32(vscale, vpos0)); // 4 axis offset of wave to interpolate with

    uint32x2_t w = vset_lane_u32(vgetq_lane_u32(vpos0, 3), vdup_n_u32(vgetq_lane_u32(vpos1, 3)), 0);
    uint32x4_t z = vaddq_u32(vcombine_u32(w, w), vcombine_u32(vdup_n_u32(vgetq_lane_u32(vpos0, 2)), vdup_n_u32(vgetq_lane_u32(vpos1, 2))));
    uint32x4_t y0 = vaddq_u32(z, vdupq_n_u32(vgetq_lane_u32(vpos0, 1))); 
    uint32x4_t y1 = vaddq_u32(z, vdupq_n_u32(vgetq_lane_u32(vpos1, 1))); 
    uint32x4_t x0 = vdupq_n_u32(vgetq_lane_u32(vpos0, 0));
    uint32x4_t x1 = vdupq_n_u32(vgetq_lane_u32(vpos1, 1));
    uint32_t notePhaseOffs0;
    uint32_t notePhaseOffs1;
    float notePhaseFrac;
    switch (sSample->channels) {
      case 1:
        for (; out_p != out_e; out_p += 2) {
          notePhaseOffs0 = sNotePhase >> sWaveSizeExpRes;
          notePhaseOffs1 = (notePhaseOffs0 + 1) & sWaveSizeMask;
          notePhaseFrac = (sNotePhase & sWaveSizeMask) * sWaveSizeRecip;
          vst1_f32(out_p, vdup_n_f32(sAmp *
            linint_f32x2(
              vlinint_n_f32x2(
                vlinintq_n_f32(
                  vlinintq_n_f32(
                    vlinintq_n_f32_indirect(vaddq_u32(x0, y0), notePhaseOffs0, notePhaseOffs1, notePhaseFrac),
                    vlinintq_n_f32_indirect(vaddq_u32(x1, y0), notePhaseOffs0, notePhaseOffs1, notePhaseFrac),
                    vgetq_lane_f32(fractions, 0)
                  ),
                  vlinintq_n_f32(
                    vlinintq_n_f32_indirect(vaddq_u32(x0, y1), notePhaseOffs0, notePhaseOffs1, notePhaseFrac),
                    vlinintq_n_f32_indirect(vaddq_u32(x1, y1), notePhaseOffs0, notePhaseOffs1, notePhaseFrac),
                    vgetq_lane_f32(fractions, 0)
                  ),
                  vgetq_lane_f32(fractions, 1)
                ),
                vgetq_lane_f32(fractions, 2)
              ),
              vgetq_lane_f32(fractions, 3)
            )
          ));
          sNotePhase += sNotePhaseIncrement;
          sLfoPhase.advance();
        }
        break;
      case 2:
        for (; out_p != out_e; out_p += 2) {
          vst1_f32(out_p, vmul_n_f32(x2, sAmp));
          sNotePhase += sNotePhaseIncrement;
          sLfoPhase.advance();
        }
        break;
      default:
        break;
    }
  }

  inline void setParameter(uint8_t index, int32_t value) {
    sParams[index] = value;
    uint32_t tmp;
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
        sLfoPhase.set(index, value * LFO_RATE_SCALE);
        break;
      case param_lfo_depth_x:
      case param_lfo_depth_y:
      case param_lfo_depth_z:
      case param_lfo_depth_w:
        index &= DIMENSION_COUNT - 1;
        sLfoDepth[index] = value * LFO_DEPTH_SCALE;
        break;
      case param_lfo_mode_x:
      case param_lfo_mode_y:
      case param_lfo_mode_z:
      case param_lfo_mode_w:
        index &= DIMENSION_COUNT - 1;
        if (value != LFO_MODE_SAMPLE_AND_HOLD) {
          sLfoMode[index] = lfoMode(value);
          sLfoWave[index] = lfoWave(value);
          sLfoOverflow[index] = lfoOverflow(value);
        } else
          sLfoMode[index] = lfo_mode_sample_and_hold;
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
        sDimensionMax[index] = sDimension[index] - 1;
        sDimensionRecip[index] = 1.f / sDimension[index];
        sDimensionMaxRecip[index] = 1.f / sDimensionMax[index];
        vst1q_u32(sDimensionScale, vmulq_n_u32(vld1q_u32(sDimensions), sWaveSize * sSample->channels));
        break;
      case param_wave_bank_idx:
      case param_wave_sample_idx:
        tmp = get_num_sample_banks();
        if (sParams[param_wave_bank_idx] >= tmp)
          sParams[param_wave_bank_idx] = tmp;
        tmp = get_num_samples_for_bank(sParams[param_wave_sample_idx]);
        if (sParams[param_wave_sample_idx] >= tmp)
          sParams[param_wave_sample_idx] = tmp;
        sSample = get_sample(sParams[param_wave_bank_idx], sParams[param_wave_sample_idx]);
      case param_wave_size:
        sWaveSizeExp = sParams[param_wave_size];
        sWaveSizeExpRes = 32 - sWaveSizeExp;
        sWaveSizeMask = sWaveSize - 1;
        sWaveSize = 1 << sWaveSizeExp;
        sWaveSizeRecip = 1.f / sWaveSize;
        vst1q_u32(sDimensionScale, vmulq_n_u32(vld1q_u32(sDimensions), sWaveSize * sSample->channels));
      case param_wave_offset:
        sWaveOffset = sParams[param_wave_offset];
        if ((sSample->frames >> sWaveSize) <= sWaveOffset) {
          sWaveOffset = (sSample->frames >> sWaveSize) - 1;
          sParams[param_wave_offset] = sWaveOffset;
        }
        setSampleOffset();
        sWaveMaxIndex = (sSample->frames >> sWaveSizeExp) - sWaveOffset - 1;
        break;
      default:
        break;
    }
  }

  inline int32_t getParameterValue(uint8_t index) const {
    sParams[index] = index;
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
    static const char * lfoModeNames[] = {
        "One",
        "Key",
        "Rnd",
        "Run",
        "S&H",
    };

    static const char * lfoWaveNames[] = {
        "saw",
        "tri",
        "sqr",
        "sin",
    };

    static const char * lfoOverflowNames[] = {
        "sat.",
        "wrap",
        "fold",
    };

    static char s[UNIT_PARAM_NAME_LEN + 1];

    switch (index) {
      case param_lfo_mode_x:
      case param_lfo_mode_y:
      case param_lfo_mode_z:
      case param_lfo_mode_w:
        if (value != LFO_MODE_SAMPLE_AND_HOLD) {
          sprintf(s, "%s %s %s", lfoModeNames[lfoMode(value)], lfoWaveNames[lfoWave(value)], lfoOverflowNames[lfoOverflow(value)]);
          return s;
        } else
          return lfoModeNames[4];
      case param_wave_size:
        sprintf(s, "%4d samples", 1 << value);
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

  inline void NoteOn(uint8_t note, uint8_t velocity) {
    sNote = note;
    sNotePhaseIncrement = (float)UINT_MAX * fastpow2((sNote + sPitchBend + NOTE_FREQ_OFFSET) * OCTAVE_RECIP);  // f = 2^((note - A4)/12), A4 = #69 = 440Hz
    sNotePhase = 0;
    sVelocity = velocity * VELOCITY_SENSITIVITY;
    sAmp = sVelocity * sPressure;
    sLfoPhase.trigger();
/*    
    for (uint32_t i = 0; i < DIMENSION_COUNT; i++) {
      switch (sLfoMode[i]) {
        case lfo_mode_one_shot:
        case lfo_mode_key_trigger:
          sLfoPhase.reset(i);
          break;
        case lfo_mode_random:
          sLfoPhase.reset(i, rand() << 1); // assume GCC RAND_MAX = INT_MAX
          break;
        case lfo_mode_free_run:
        default:
          break;
      }
    }
*/
  }

  inline void NoteOff(uint8_t note) {
    (void)note;
    sAmp = 0.f;
  }

  inline void GateOn(uint8_t velocity) {
    sVelocity = velocity * VELOCITY_SENSITIVITY;
    sAmp = sVelocity * sPressure;
  }

  inline void GateOff() {
    sAmp = 0.f;
  }

  inline void AllNoteOff() {
    sAmp = 0.f;
  }

  inline void PitchBend(uint16_t bend) {
    sPitchBend = (bend - PITCH_BEND_CENTER) * PITCH_BEND_SENSITIVITY;
    sNotePhaseIncrement = (float)UINT_MAX * fastpow2((sNote + sPitchBend + NOTE_FREQ_OFFSET) * OCTAVE_RECIP);
  }

  inline void ChannelPressure(uint8_t pressure) {
    sPressure = pressure * CONTROL_CHANGE_SENSITIVITY;
    sAmp = sVelocity * sPressure;
  }

  inline void Aftertouch(uint8_t note, uint8_t aftertouch) {
    (void)note;
    sVelocity = aftertouch * AFTERTOUCH_SENSITIVITY;
    sAmp = sVelocity * sPressure;
  }

  inline void LoadPreset(uint8_t idx) { (void)idx; }

  inline uint8_t getPresetIndex() const { return 0; }

  /*===========================================================================*/
  /* Static Members. */
  /*===========================================================================*/

  static inline const char * getPresetName(uint8_t idx) {
    (void)idx;
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

  static uint8_t sNote;
  static float sVelocity;
  static float sPressure;
  static float sAmp;
  static uint32_t sNotePhase;
  static uint32_t sNotePhaseIncrement;
  static float sPitchBend;

  static uint32_t sParams[PARAM_COUNT];
  static float sPosition[DIMENSION_COUNT];
  struct lfo_t {
    int32x4_t p0, p1, inc;
//Set LFO phases and increments to initial values
    inline __attribute__((always_inline))
    void init() {
      p0 = vdupq_n_s32(INT_MIN);
      p1 = vdupq_n_s32(INT_MIN);
      inc = vdupq_n_s32(0);
    }
//Set LFO phases to initial values with respect to LFO modes
    inline __attribute__((always_inline))
    void trigger() {
      p1 = p0 = vbslq_s32(vcltq_u32(vdupq_n_u32(lfo_mode_random), vld1q_u32(sLfoMode)), // if one shot or trigger
        vdupq_n_s32(INT_MIN), // reset phase
        vbslq_s32(vceqq_u32(vdupq_n_u32(lfo_mode_random), vld1q_u32(sLfoMode)), // else if random
          vmovq_x4_s32((uint32_t)rand() << 1, (uint32_t)rand() << 1, (uint32_t)rand() << 1, (uint32_t)rand() << 1), // random phase
          p0 // else keep current
        )
      );
    }
//Increment LFO phases values
    inline __attribute__((always_inline))
    void advance() {
      p1 = p0;
      p0 = vaddq_s32(p0, inc);
      uint32x4_t mask = vandq_u32(
        vceqq_u32(vdupq_n_u32(lfo_mode_one_shot), vld1q_u32(sLfoMode)),
        vcltq_s32(p0, p1)
      );
      inc = vbslq_s32(mask, vdupq_n_s32(0), inc);
      p0 = vbslq_s32(mask, vdupq_n_s32(INT_MAX), p0);
      p1 = vbslq_s32(mask, vdupq_n_s32(INT_MAX), p1);
    }
//Ser LFO phase increment value
    inline __attribute__((always_inline))
    void set(const int index, int32_t value) {
      switch (index) {
        case 0:
          vsetq_lane_s32(value, inc, 0);
          break;
        case 1:
          vsetq_lane_s32(value, inc, 1);
          break;
        case 2:
          vsetq_lane_s32(value, inc, 2);
          break;
        case 3:
          vsetq_lane_s32(value, inc, 3);
          break;
      }
    }
  } sLfoPhase;
  static float sLfoDepth[DIMENSION_COUNT];
  static uint32_t sLfoMode[DIMENSION_COUNT];
  static uint32_t sLfoWave[DIMENSION_COUNT];
  static uint32_t sLfoOverflow[DIMENSION_COUNT];
  static uint32_t sDimension[DIMENSION_COUNT];
  static uint32_t sDimensionMax[DIMENSION_COUNT];
  static float sDimensionRecip[DIMENSION_COUNT];
  static float sDimensionMaxRecip[DIMENSION_COUNT];
  static uint32_t sDimensions[DIMENSION_COUNT];
  static uint32_t sDimensionScale[DIMENSION_COUNT];

  unit_runtime_get_num_sample_banks_ptr get_num_sample_banks;
  unit_runtime_get_num_samples_for_bank_ptr get_num_samples_for_bank;
  unit_runtime_get_sample_ptr get_sample;
  static const sample_wrapper_t * sSample;
  static uint32_t sSampleOffset;
  static uint32_t sWaveSize;
  static uint32_t sWaveSizeExp;
  static uint32_t sWaveSizeExpRes;
  static uint32_t sWaveSizeMask;
  static float sWaveSizeRecip;
  static uint32_t sWaveOffset;
  static uint32_t sWaveMaxIndex;

  /*===========================================================================*/
  /* Private Methods. */
  /*===========================================================================*/
/*
static inline float getSample(float *samples, uint32_t phase, uint32_t sizeExpRes, uint32_t sizeExpMask, float sizeRecip) {
  uint32_t x0 = phase >> sizeExpRes;
  uint32_t x1 = (x0 + 1) & sizeExpMask;
  float frac = (phase & sizeExpMask) * sizeRecip;
  float out = samples[x0] + (samples[x1] - samples[x0]) * frac;
  return out;
}

static inline float getSample(float *samples, float phase, uint32_t size, uint32_t sizeExp, uint32_t sizeExpRes, uint32_t sizeExpMask, float sizeRecip) {
  float x = phase * size;
  uint32_t x0 = (uint32_t)x;
  uint32_t x1 = (x0 + 1) & sizeExpMask;
  float frac = x - x0;
  float out = samples[x0] + (samples[x1] - samples[x0]) * frac;
  return out;
}
*/
static inline void setSampleOffset(){
  sSampleOffset = (uint32_t)sSample->sample_ptr + sWaveOffset * sWaveSize * sSample->channels * sizeof(float);
}

  /*===========================================================================*/
  /* Constants. */
  /*===========================================================================*/
};
