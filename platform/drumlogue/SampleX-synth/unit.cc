/*
 *  File: unit.cc
 *
 *  SampleX Synth unit.
 *
 *
 *  2023 (c) Oleg Burdaev
 *  mailto: dukesrg@gmail.com
 */

#include "unit.h"  // Note: Include common definitions for all units

#include <cstddef>
#include <cstdint>
#include <arm_neon.h>
#include <string.h>
#include <stdio.h>

#include "runtime.h"
#include "attributes.h"
#include "fastpow.h"
#include "arm.h"

#define OCTAVE_RECIP .083333333f // 1/12
#define NOTE_FREQ_OFFSET -150.232645f // 12 * log2(440/48000) - 69
#define PITCH_BEND_CENTER 8192
#define PITCH_BEND_SENSITIVITY 1.220703125e-4f // 24/8192
#define PITCH_TUNE_CENTER 6000
#define PITCH_TUNE_SENSITIVITY .01f
#define BPM_TUNE_SENSITIVITY .01f
#define VELOCITY_SENSITIVITY 7.8740157e-3f // 1/127
#define CONTROL_CHANGE_SENSITIVITY 7.8740157e-3f // 1/127
#define AFTERTOUCH_SENSITIVITY 7.8740157e-3f // 1/127

enum {
  param_gate_note = 0U,
  param_track_mode,
  param_playback_mode,
  param_trigger_mode,
  param_sample_1,
  param_sample_2,
  param_sample_3,
  param_sample_4,
  param_level_1,
  param_level_2,
  param_level_3,
  param_level_4,
  param_tune_1,
  param_tune_2,
  param_tune_3,
  param_tune_4,
  param_thd_low_1,
  param_thd_low_2,
  param_thd_low_3,
  param_thd_low_4,
  param_thd_high_1,
  param_thd_high_2,
  param_thd_high_3,
  param_thd_high_4
};

enum {
  track_mode_layers = 0U,
  track_mode_sequence,
  track_mode_random
};

enum {
  playback_mode_oneshot = 0U,
  playback_mode_loop,
  playback_mode_count
};

enum {
  trigger_mode_normal = 0U,
  trigger_mode_keep,
  trigger_mode_sustain,
  trigger_mode_ignore,
  trigger_mode_count,
  trigger_mode_latch,
  trigger_mode_pause,
//  trigger_mode_count
};

static int32_t sParams[PARAM_COUNT];

static float sNote;
static float sPitchBend;
static float sTempo;
static float32x4_t sLevel;
static float32x2_t sAmp;

static uint32x4_t sNoteThdLow;
static uint32x4_t sNoteThdHigh;
static uint32x4_t sVelocityThdLow;
static uint32x4_t sVelocityThdHigh;

static uint32x4_t maskNoSample;
static uint32x4_t maskSampleBPM;
static uint32x4_t maskRcvNoteOn;
static uint32x4_t maskRcvNoteOff;
static uint32x4_t maskOneShot;
static uint32x4_t maskStopped;
static uint32x4_t maskLatched;
static uint32x4_t maskPause;

static float32x4_t sSampleBPM;
static float32x4_t sBPMTune;
static float32x4_t sSampleBPMRecip;
static float32x4_t sPitchTune;

static float32x4_t sSampleCounter;
static float32x4_t sSampleCounterIncrementPitch;
static float32x4_t sSampleCounterIncrementBPM;
static float32x4_t sSampleSize;
static uint32x4_t sSampleChannels;
static uint32x4_t sSampleChannelOffset2;
static const float *sSamplePtr[4];

static uint32_t sSeqIndex;

const unit_runtime_desc_t *sDesc;

static uint32_t seed = 0x363812fd;

/*===========================================================================*/
/* Private Methods. */
/*===========================================================================*/

fast_inline uint32_t random() {
  seed ^= seed >> 13;
  seed ^= seed >> 17;
  seed ^= seed << 5;
  return seed;
}

fast_inline const sample_wrapper_t * getSampleWrapper(const unit_runtime_desc_t *desc, uint32_t index) {
  uint32_t bankcount = desc->get_num_sample_banks();
  for (uint32_t bank_idx = 0; bank_idx < bankcount; bank_idx++) {
    uint32_t samplecount = bank_idx < 5 ? desc->get_num_samples_for_bank(bank_idx) : 128;
    if (samplecount > index) {
      return desc->get_sample(bank_idx, index);
    } else {
      index -= samplecount;
    }
  }
  return nullptr;
}

fast_inline const char * getSampleName(const unit_runtime_desc_t *desc, uint32_t index) {
  const sample_wrapper_t * sample;;
  if (index == 0 || (sample = getSampleWrapper(desc, index - 1)) == nullptr)
    return "---";
  return sample->name;
}

fast_inline void setPitch() {
  const float32x4_t pitch = (sNote + sPitchBend - sPitchTune) * OCTAVE_RECIP;
  sSampleCounterIncrementPitch[0] = fastpow2(pitch[0]);
  sSampleCounterIncrementPitch[1] = fastpow2(pitch[1]);
  sSampleCounterIncrementPitch[2] = fastpow2(pitch[2]);
  sSampleCounterIncrementPitch[3] = fastpow2(pitch[3]);
}

fast_inline void nextSeq() {
  if (sParams[param_track_mode] == track_mode_random)
    sSeqIndex = random();
  maskStopped = vdupq_n_u32(-1);
  for (uint32_t i = 0; i < 4; i++) {
    sSeqIndex++;
    sSeqIndex &= 3;
    if (!maskNoSample[sSeqIndex]) {
      maskStopped[sSeqIndex] = 0;
      break;
    }
  }
  sSampleCounter = vdupq_n_f32(0.f);
}

fast_inline void noteOn(uint8_t note, uint8_t velocity) {
  sNote = note;
  setPitch();
  sAmp = vdup_n_f32(velocity * VELOCITY_SENSITIVITY);
  maskStopped = maskLatched & ~maskStopped;
  sSampleCounter = vreinterpretq_f32_u32(vbicq_u32(vreinterpretq_u32_f32(sSampleCounter), maskRcvNoteOn));
  if (sParams[param_track_mode] != track_mode_layers)
    nextSeq();
  uint32x4_t vNote = vdupq_n_u32(note);
  uint32x4_t vVelocity = vdupq_n_u32(velocity);
  maskStopped |= vcltq_u32(vNote, sNoteThdLow) | vcgtq_u32(vNote, sNoteThdHigh) | vcltq_u32(vVelocity, sVelocityThdLow) | vcgtq_u32(vVelocity, sVelocityThdHigh);
}

fast_inline void noteOff(uint8_t note) {
  (void)note;
  maskStopped |= maskRcvNoteOff;
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

  sDesc = desc;
  maskStopped = vdupq_n_u32(-1);
  sSeqIndex = -1;

  return k_unit_err_none;
}

__unit_callback void unit_teardown() {
}

__unit_callback void unit_reset() {
  maskStopped = vdupq_n_u32(-1);
  sSeqIndex = -1;
}

__unit_callback void unit_resume() {
}

__unit_callback void unit_suspend() {
}

__unit_callback void unit_render(const float * in, float * out, uint32_t frames) {
  (void)in;
  const float32x4_t vSampleCounterIncrement = vreinterpretq_f32_u32(vbicq_u32(
    vreinterpretq_u32_f32(vbslq_f32(maskSampleBPM, sSampleCounterIncrementBPM, sSampleCounterIncrementPitch)),
    maskPause & maskStopped)
  );
  float * __restrict out_p = out;
  const float * out_e = out_p + (frames << 1);
  for (; out_p != out_e; out_p += 2) {
    uint32x4_t maskSampleCounterOverflow = vcgeq_f32(sSampleCounter, sSampleSize);
    sSampleCounter = vbslq_f32(maskSampleCounterOverflow, sSampleCounter - sSampleSize, sSampleCounter);
    maskStopped |= maskOneShot & maskSampleCounterOverflow;
//    if ((!maskOneShot[sSeqIndex]) & maskSampleCounterOverflow[sSeqIndex] && sParams[param_track_mode] != track_mode_layers)
//      nextSeq();
    uint32x4_t vSampleOffset1 = vcvtq_u32_f32(sSampleCounter) * sSampleChannels;
    uint32x4_t vSampleOffset2 = vSampleOffset1 + sSampleChannelOffset2;
    float32x4_t vOut1 = {sSamplePtr[0][vSampleOffset1[0]], sSamplePtr[1][vSampleOffset1[1]], sSamplePtr[2][vSampleOffset1[2]], sSamplePtr[3][vSampleOffset1[3]]};
    float32x4_t vOut2 = {sSamplePtr[0][vSampleOffset2[0]], sSamplePtr[1][vSampleOffset2[1]], sSamplePtr[2][vSampleOffset2[2]], sSamplePtr[3][vSampleOffset2[3]]};
    vOut1 = sLevel * vreinterpretq_f32_u32(vbicq_u32(vreinterpretq_u32_f32(vOut1), maskStopped | maskNoSample));
    vOut2 = sLevel * vreinterpretq_f32_u32(vbicq_u32(vreinterpretq_u32_f32(vOut2), maskStopped | maskNoSample));
    vst1_f32(out_p, sAmp * vpadd_f32(vpadd_f32(vget_low_f32(vOut1), vget_high_f32(vOut1)), vpadd_f32(vget_low_f32(vOut2), vget_high_f32(vOut2))));

    sSampleCounter += vSampleCounterIncrement;
  }
  return;
}

__unit_callback void unit_set_param_value(uint8_t id, int32_t value) {
  static uint32x4_t mask;
  static char * lastspace;
  const sample_wrapper_t * sample;
  value = (int16_t)value;
  sParams[id] = value;
  switch (id) {
    case param_playback_mode:
      for (uint32_t i = 0; i < 4; i++) {
        mask[i] = value % playback_mode_count;
        value /= playback_mode_count;
      }
      maskOneShot = vceqq_u32(vdupq_n_u32(playback_mode_oneshot), mask);
      break;    
    case param_trigger_mode:
      for (uint32_t i = 0; i < 4; i++) {
        mask[i] = value % trigger_mode_count;
        value /= trigger_mode_count;
      }
      maskRcvNoteOff = vceqq_u32(vdupq_n_u32(trigger_mode_normal), mask);
      maskLatched = vceqq_u32(vdupq_n_u32(trigger_mode_latch), mask);
      maskPause = vceqq_u32(vdupq_n_u32(trigger_mode_pause), mask);
      maskRcvNoteOn = vceqq_u32(vdupq_n_u32(trigger_mode_sustain), mask) | maskRcvNoteOff | maskLatched;
      maskRcvNoteOff |= vceqq_u32(vdupq_n_u32(trigger_mode_keep), mask);
      maskLatched |= maskPause;
      break;
    case param_sample_1:
    case param_sample_2:
    case param_sample_3:
    case param_sample_4:
      id &= 3;
      if (value == 0 || (sample = getSampleWrapper(sDesc, value - 1)) == nullptr) {
        sample = getSampleWrapper(sDesc, 0);
        maskNoSample[id] = -1;
      } else {
        maskNoSample[id] = 0;
      }
      if (strcmp(&sample->name[strlen(sample->name) - 3], "BPM") == 0 && (lastspace = strrchr((char *)sample->name, ' ')) != NULL && sscanf(lastspace, "%f", &sSampleBPM[id]) == 1) {
        sSampleBPMRecip[id] = 1.f / (sSampleBPM[id] + sBPMTune[id]);
        sSampleCounterIncrementBPM[id] = sTempo * sSampleBPMRecip[id];
        maskSampleBPM[id] = -1;
      } else {
        maskSampleBPM[id] = 0;
      }
      sSampleSize[id] = sample->frames;
      sSampleChannels[id] = sample->channels;
      sSamplePtr[id] = sample->sample_ptr;
      sSampleChannelOffset2[id] = sample->channels - 1; 
      break;
    case param_level_1:
    case param_level_2:
    case param_level_3:
    case param_level_4:
      sLevel[id & 3] = fastpow(10.f, value * 5.e-3f);
      break;
    case param_tune_1:
    case param_tune_2:
    case param_tune_3:
    case param_tune_4:
      id &= 3;
      sBPMTune[id] = value * BPM_TUNE_SENSITIVITY;
      sPitchTune[id] = (value + PITCH_TUNE_CENTER) * PITCH_TUNE_SENSITIVITY;
      sSampleBPMRecip[id] = 1.f / (sSampleBPM[id] + sBPMTune[id]);
      sSampleCounterIncrementBPM[id] = sTempo * sSampleBPMRecip[id];
      sSampleCounterIncrementPitch[id] = fastpow2((sNote + sPitchBend - sPitchTune[id]) * OCTAVE_RECIP);
      setPitch();
      break;
    case param_thd_low_1:
    case param_thd_low_2:
    case param_thd_low_3:
    case param_thd_low_4:
      id &= 3;
      if (value < 0) {
        sNoteThdLow[id] = 0;
        sVelocityThdLow[id] = value += 128;
      } else {
        sNoteThdLow[id] = value;
        sVelocityThdLow[id] = 0;
      }
      break;
    case param_thd_high_1:
    case param_thd_high_2:
    case param_thd_high_3:
    case param_thd_high_4:
      id &= 3;
      if (value < 0) {
        sNoteThdHigh[id] = 127;
        sVelocityThdHigh[id] = value += 128;
      } else {
        sNoteThdHigh[id] = value;
        sVelocityThdHigh[id] = 127;
      }
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
  static const char trackModes[][7] = {"All", "Seq.", "Random"};
  static const char * playbackModes = "OL";
  static const char * triggerModes = "NKSILP";
  static const char noteNames[][3] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
  static char s[UNIT_PARAM_NAME_LEN + 1];
  static uint32_t cents;
  static uint32_t modes[4];
  
  switch (id) {
    case param_track_mode:
      return trackModes[value];
      break;
    case param_playback_mode:
      for (uint32_t i = 0; i < 4; i++) {
        modes[i] = value % playback_mode_count;
        value /= playback_mode_count;
      }
      sprintf(s, "%c.%c.%c.%c", playbackModes[modes[0]], playbackModes[modes[1]], playbackModes[modes[2]], playbackModes[modes[3]]);
      break;
    case param_trigger_mode:
      for (uint32_t i = 0; i < 4; i++) {
        modes[i] = value % trigger_mode_count;
        value /= trigger_mode_count;
      }
      sprintf(s, "%c.%c.%c.%c", triggerModes[modes[0]], triggerModes[modes[1]], triggerModes[modes[2]], triggerModes[modes[3]]);
      break;
    case param_sample_1:
    case param_sample_2:
    case param_sample_3:
    case param_sample_4:
      return getSampleName(sDesc, value);
      break;
    case param_tune_1:
    case param_tune_2:
    case param_tune_3:
    case param_tune_4:
      id &= 3;
      if (maskSampleBPM[id]) {
        sprintf(s, "%+.2f", value * BPM_TUNE_SENSITIVITY);
      } else {
        value += PITCH_TUNE_CENTER;
        cents = value % 100;
        value /= 100;
        sprintf(s, "%s%d.%02d", noteNames[value % 12], value / 12 - 1, cents);
      }
      break;
    case param_thd_low_1:
    case param_thd_low_2:
    case param_thd_low_3:
    case param_thd_low_4:
    case param_thd_high_1:
    case param_thd_high_2:
    case param_thd_high_3:
    case param_thd_high_4:
      if (value < 0 ) {
        value += 128;
      } else {
        sprintf(s, "%s%d", noteNames[value % 12], value / 12 - 1);
        break;
      }
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
  sSampleCounterIncrementBPM = sTempo * sSampleBPMRecip;
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
  noteOff(sParams[param_gate_note]);
}

__unit_callback void unit_all_note_off() {
  maskStopped = vdupq_n_u32(-1);
}

__unit_callback void unit_pitch_bend(uint16_t bend) {
  sPitchBend = (bend - PITCH_BEND_CENTER) * PITCH_BEND_SENSITIVITY;
  setPitch();
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
