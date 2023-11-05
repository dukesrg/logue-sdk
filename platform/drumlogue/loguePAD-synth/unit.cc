/*
 *  File: unit.cc
 *
 *  loguePAD Synth unit.
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

#define TRACK_COUNT 16
#define OCTAVE_RECIP .083333333f // 1/12
#define PITCH_BEND_CENTER 8192
#define PITCH_BEND_SENSITIVITY 1.220703125e-4f // 24/8192
#define TRACK_1_NOTE 60
#define BPM_TUNE_SENSITIVITY .01f
#define VELOCITY_SENSITIVITY 7.8740157e-3f // 1/127
#define CONTROL_CHANGE_SENSITIVITY 7.8740157e-3f // 1/127
#define AFTERTOUCH_SENSITIVITY 7.8740157e-3f // 1/127

enum {
  param_gate_note = 0U,
  param_track_mode_1,
  param_split_point,
  param_track_mode_2,
  param_playback_mode_1_4,
  param_playback_mode_5_8,
  param_playback_mode_9_12,
  param_playback_mode_13_16,
  param_sample_1,
  param_sample_2,
  param_sample_3,
  param_sample_4,
  param_sample_5,
  param_sample_6,
  param_sample_7,
  param_sample_8,
  param_sample_9,
  param_sample_10,
  param_sample_11,
  param_sample_12,
  param_sample_13,
  param_sample_14,
  param_sample_15,
  param_sample_16,
};

enum {
  track_mode_pad = 0U,
  track_mode_queue,
  track_mode_chain,
  track_mode_random,
  track_mode_count,  
};

enum {
  playback_mode_oneshot = 0U,
  playback_mode_sustain,
  playback_mode_repeat,
  playback_mode_latch,
  playback_mode_count
};

static int32_t sParams[PARAM_COUNT];

//static float sNote;
static float sTempo;
static float32x2_t sChannelPressure;
static float32x4_t sAmp[TRACK_COUNT >> 2];

static uint32x4_t maskNoSample[TRACK_COUNT >> 2];
static uint32x4_t maskSampleBPM[TRACK_COUNT >> 2];
static uint32x4_t maskRcvNoteOff[TRACK_COUNT >> 2];
static uint32x4_t maskOneShot[TRACK_COUNT >> 2];
static uint32x4_t maskStopped[TRACK_COUNT >> 2];
static uint32x4_t maskLatched[TRACK_COUNT >> 2];

static float32x4_t sSampleBPMRecip[TRACK_COUNT >> 2];

static float32x4_t sSampleCounter[TRACK_COUNT >> 2];
static float32x4_t sSampleCounterIncrementPitch;
static float32x4_t sSampleCounterIncrementBPM[TRACK_COUNT >> 2];
static float32x4_t sSampleSize[TRACK_COUNT >> 2];
static uint32x4_t sSampleChannels[TRACK_COUNT >> 2];
static uint32x4_t sSampleChannelOffset2[TRACK_COUNT >> 2];
static const float *sSamplePtr[TRACK_COUNT];

//static uint32_t sSeqIndex;

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
/*
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
*/
fast_inline void noteOn(uint8_t note, uint8_t velocity) {
  note = (note - TRACK_1_NOTE) & (TRACK_COUNT - 1);
  ((uint32_t *)maskStopped)[note] = ((uint32_t *)maskLatched)[note] & ~((uint32_t *)maskStopped)[note];
  ((float *)sSampleCounter)[note] = 0.f;
  ((float *)sAmp)[note] = velocity * VELOCITY_SENSITIVITY;
//  if (sParams[param_track_mode] != track_mode_layers)
//    nextSeq();
}

fast_inline void noteOff(uint8_t note) {
  note = (note - TRACK_1_NOTE) & (TRACK_COUNT - 1);
  ((uint32_t *)maskStopped)[note] |= ((uint32_t *)maskRcvNoteOff)[note];
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
  sChannelPressure = vdup_n_f32(1.f);
  maskStopped[0] = vdupq_n_u32(-1);
  maskStopped[1] = vdupq_n_u32(-1);
  maskStopped[2] = vdupq_n_u32(-1);
  maskStopped[3] = vdupq_n_u32(-1);
//  sSeqIndex = -1;

  return k_unit_err_none;
}

__unit_callback void unit_teardown() {
}

__unit_callback void unit_reset() {
  maskStopped[0] = vdupq_n_u32(-1);
  maskStopped[1] = vdupq_n_u32(-1);
  maskStopped[2] = vdupq_n_u32(-1);
  maskStopped[3] = vdupq_n_u32(-1);
//  sSeqIndex = -1;
}

__unit_callback void unit_resume() {
}

__unit_callback void unit_suspend() {
}

__unit_callback void unit_render(const float * in, float * out, uint32_t frames) {
  (void)in;
  float32x4_t vSampleCounterIncrement[TRACK_COUNT >> 2];
  vSampleCounterIncrement[0] = vbslq_f32(maskSampleBPM[0], sSampleCounterIncrementBPM[0], sSampleCounterIncrementPitch);
  vSampleCounterIncrement[1] = vbslq_f32(maskSampleBPM[1], sSampleCounterIncrementBPM[1], sSampleCounterIncrementPitch);
  vSampleCounterIncrement[2] = vbslq_f32(maskSampleBPM[2], sSampleCounterIncrementBPM[2], sSampleCounterIncrementPitch);
  vSampleCounterIncrement[3] = vbslq_f32(maskSampleBPM[3], sSampleCounterIncrementBPM[3], sSampleCounterIncrementPitch);

  float * __restrict out_p = out;
  const float * out_e = out_p + (frames << 1);
  for (; out_p != out_e; out_p += 2) {
    uint32x4_t maskSampleCounterOverflow[TRACK_COUNT >> 2];
    maskSampleCounterOverflow[0] = vcgeq_f32(sSampleCounter[0], sSampleSize[0]);
    maskSampleCounterOverflow[1] = vcgeq_f32(sSampleCounter[1], sSampleSize[1]);
    maskSampleCounterOverflow[2] = vcgeq_f32(sSampleCounter[2], sSampleSize[2]);
    maskSampleCounterOverflow[3] = vcgeq_f32(sSampleCounter[3], sSampleSize[3]);
    sSampleCounter[0] = vbslq_f32(maskSampleCounterOverflow[0], sSampleCounter[0] - sSampleSize[0], sSampleCounter[0]);
    sSampleCounter[1] = vbslq_f32(maskSampleCounterOverflow[1], sSampleCounter[1] - sSampleSize[1], sSampleCounter[1]);
    sSampleCounter[2] = vbslq_f32(maskSampleCounterOverflow[2], sSampleCounter[2] - sSampleSize[2], sSampleCounter[2]);
    sSampleCounter[3] = vbslq_f32(maskSampleCounterOverflow[3], sSampleCounter[3] - sSampleSize[3], sSampleCounter[3]);    
    maskStopped[0] |= maskOneShot[0] & maskSampleCounterOverflow[0];
    maskStopped[1] |= maskOneShot[1] & maskSampleCounterOverflow[1];
    maskStopped[2] |= maskOneShot[2] & maskSampleCounterOverflow[2];
    maskStopped[3] |= maskOneShot[3] & maskSampleCounterOverflow[3];

//    if ((!maskOneShot[sSeqIndex]) & maskSampleCounterOverflow[sSeqIndex] && sParams[param_track_mode] != track_mode_layers)
//      nextSeq();
    uint32x4_t vSampleOffset1[TRACK_COUNT >> 2];
    uint32x4_t vSampleOffset2[TRACK_COUNT >> 2];
    vSampleOffset1[0] = vcvtq_u32_f32(sSampleCounter[0]) * sSampleChannels[0];
    vSampleOffset1[1] = vcvtq_u32_f32(sSampleCounter[1]) * sSampleChannels[1];
    vSampleOffset1[2] = vcvtq_u32_f32(sSampleCounter[2]) * sSampleChannels[2];
    vSampleOffset1[3] = vcvtq_u32_f32(sSampleCounter[3]) * sSampleChannels[3];
    vSampleOffset2[0] = vSampleOffset1[0] + sSampleChannelOffset2[0];
    vSampleOffset2[1] = vSampleOffset1[1] + sSampleChannelOffset2[1];
    vSampleOffset2[2] = vSampleOffset1[2] + sSampleChannelOffset2[2];
    vSampleOffset2[3] = vSampleOffset1[3] + sSampleChannelOffset2[3];


    float32x4_t vOut1[TRACK_COUNT >> 2];
    float32x4_t vOut2[TRACK_COUNT >> 2];
    vOut1[0] = (float32x4_t){sSamplePtr[0][vSampleOffset1[0][0]], sSamplePtr[1][vSampleOffset1[0][1]], sSamplePtr[2][vSampleOffset1[0][2]], sSamplePtr[3][vSampleOffset1[0][3]]};
    vOut1[1] = (float32x4_t){sSamplePtr[4][vSampleOffset1[1][0]], sSamplePtr[5][vSampleOffset1[1][1]], sSamplePtr[6][vSampleOffset1[1][2]], sSamplePtr[7][vSampleOffset1[1][3]]};
    vOut1[2] = (float32x4_t){sSamplePtr[8][vSampleOffset1[2][0]], sSamplePtr[9][vSampleOffset1[2][1]], sSamplePtr[10][vSampleOffset1[2][2]], sSamplePtr[11][vSampleOffset1[2][3]]};
    vOut1[3] = (float32x4_t){sSamplePtr[12][vSampleOffset1[3][0]], sSamplePtr[13][vSampleOffset1[3][1]], sSamplePtr[14][vSampleOffset1[3][2]], sSamplePtr[15][vSampleOffset1[3][3]]};

    vOut2[0] = (float32x4_t){sSamplePtr[0][vSampleOffset2[0][0]], sSamplePtr[1][vSampleOffset2[0][1]], sSamplePtr[2][vSampleOffset2[0][2]], sSamplePtr[3][vSampleOffset2[0][3]]};
    vOut2[1] = (float32x4_t){sSamplePtr[4][vSampleOffset2[1][0]], sSamplePtr[5][vSampleOffset2[1][1]], sSamplePtr[6][vSampleOffset2[1][2]], sSamplePtr[7][vSampleOffset2[1][3]]};
    vOut2[2] = (float32x4_t){sSamplePtr[8][vSampleOffset2[2][0]], sSamplePtr[9][vSampleOffset2[2][1]], sSamplePtr[10][vSampleOffset2[2][2]], sSamplePtr[11][vSampleOffset2[2][3]]};
    vOut2[3] = (float32x4_t){sSamplePtr[12][vSampleOffset2[3][0]], sSamplePtr[13][vSampleOffset2[3][1]], sSamplePtr[14][vSampleOffset2[3][2]], sSamplePtr[15][vSampleOffset2[3][3]]};

    uint32x4_t maskNoSound[TRACK_COUNT >> 2];
    maskNoSound[0] = maskStopped[0] | maskNoSample[0];
    maskNoSound[1] = maskStopped[1] | maskNoSample[1];
    maskNoSound[2] = maskStopped[2] | maskNoSample[2];
    maskNoSound[3] = maskStopped[3] | maskNoSample[3];

    float32x4_t out1 = sAmp[0] * vreinterpretq_f32_u32(vbicq_u32(vreinterpretq_u32_f32(vOut1[0]), maskNoSound[0]));
    out1 += sAmp[1] * vreinterpretq_f32_u32(vbicq_u32(vreinterpretq_u32_f32(vOut1[1]), maskNoSound[1]));
    out1 += sAmp[2] * vreinterpretq_f32_u32(vbicq_u32(vreinterpretq_u32_f32(vOut1[2]), maskNoSound[2]));
    out1 += sAmp[3] * vreinterpretq_f32_u32(vbicq_u32(vreinterpretq_u32_f32(vOut1[3]), maskNoSound[3]));

    float32x4_t out2 = sAmp[0] * vreinterpretq_f32_u32(vbicq_u32(vreinterpretq_u32_f32(vOut2[0]), maskNoSound[0]));
    out2 += sAmp[1] * vreinterpretq_f32_u32(vbicq_u32(vreinterpretq_u32_f32(vOut2[1]), maskNoSound[1]));
    out2 += sAmp[2] * vreinterpretq_f32_u32(vbicq_u32(vreinterpretq_u32_f32(vOut2[2]), maskNoSound[2]));
    out2 += sAmp[3] * vreinterpretq_f32_u32(vbicq_u32(vreinterpretq_u32_f32(vOut2[3]), maskNoSound[3]));

    vst1_f32(out_p, sChannelPressure * vpadd_f32(vpadd_f32(vget_low_f32(out1), vget_high_f32(out1)), vpadd_f32(vget_low_f32(out2), vget_high_f32(out2))));

    sSampleCounter[0] += vSampleCounterIncrement[0];
    sSampleCounter[1] += vSampleCounterIncrement[1];
    sSampleCounter[2] += vSampleCounterIncrement[2];
    sSampleCounter[3] += vSampleCounterIncrement[3];
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
    case param_playback_mode_1_4:
    case param_playback_mode_5_8:
    case param_playback_mode_9_12:
    case param_playback_mode_13_16:
      id &= 3;
      for (uint32_t i = 0; i < 4; i++) {
        mask[i] = value % playback_mode_count;
        value /= playback_mode_count;
      }
      maskOneShot[id] = vcltq_u32(mask, vdupq_n_u32(playback_mode_repeat));
      maskRcvNoteOff[id] = vceqq_u32(mask, vdupq_n_u32(playback_mode_oneshot)) | vceqq_u32(mask, vdupq_n_u32(playback_mode_repeat));
      maskLatched[id] = vcgeq_u32(mask, vdupq_n_u32(playback_mode_latch));
      break;
    case param_sample_1:
    case param_sample_2:
    case param_sample_3:
    case param_sample_4:
    case param_sample_5:
    case param_sample_6:
    case param_sample_7:
    case param_sample_8:
    case param_sample_9:
    case param_sample_10:
    case param_sample_11:
    case param_sample_12:
    case param_sample_13:
    case param_sample_14:
    case param_sample_15:
    case param_sample_16:
      id -= param_sample_1;
      if (value == 0 || (sample = getSampleWrapper(sDesc, value - 1)) == nullptr) {
        sample = getSampleWrapper(sDesc, 0);
        ((uint32_t *)maskNoSample)[id] = -1;
      } else {
        ((uint32_t *)maskNoSample)[id] = 0;
      }
      if (strcmp(&sample->name[strlen(sample->name) - 3], "BPM") == 0 && (lastspace = strrchr((char *)sample->name, ' ')) != NULL && sscanf(lastspace, "%f", &((float *)sSampleBPMRecip)[id]) == 1) {
        ((float *)sSampleBPMRecip)[id] = 1.f / ((float *)sSampleBPMRecip)[id];
        ((float *)sSampleCounterIncrementBPM)[id] = sTempo * ((float *)sSampleBPMRecip)[id];
        ((uint32_t *)maskSampleBPM)[id] = -1;
      } else {
        ((uint32_t *)maskSampleBPM)[id] = 0;
      }
      ((float *)sSampleSize)[id] = sample->frames;
      ((uint32_t *)sSampleChannels)[id] = sample->channels;
      sSamplePtr[id] = sample->sample_ptr;
      ((uint32_t *)sSampleChannelOffset2)[id] = sample->channels - 1; 
      break;
  }
}

__unit_callback int32_t unit_get_param_value(uint8_t id) {
  return sParams[id];
}

__unit_callback const char * unit_get_param_str_value(uint8_t id, int32_t value) {
  value = (int16_t)value;
  static const char trackModes[][7] = {"Pad", "Queue", "Chain", "Random"};
  static const char * playbackModes = "OSRL";
  static char s[UNIT_PARAM_NAME_LEN + 1];
  static uint32_t modes[4];

  switch (id) {
    case param_split_point:
      sprintf(s, "%d     %d", TRACK_COUNT - value, value);
      break;
    case param_track_mode_1:
    case param_track_mode_2:
      return trackModes[value];
      break;
    case param_playback_mode_1_4:
    case param_playback_mode_5_8:
    case param_playback_mode_9_12:
    case param_playback_mode_13_16:
      for (uint32_t i = 0; i < 4; i++) {
        modes[i] = value % playback_mode_count;
        value /= playback_mode_count;
      }
      sprintf(s, "%c.%c.%c.%c", playbackModes[modes[0]], playbackModes[modes[1]], playbackModes[modes[2]], playbackModes[modes[3]]);
      break;
    case param_sample_1:
    case param_sample_2:
    case param_sample_3:
    case param_sample_4:
    case param_sample_5:
    case param_sample_6:
    case param_sample_7:
    case param_sample_8:
    case param_sample_9:
    case param_sample_10:
    case param_sample_11:
    case param_sample_12:
    case param_sample_13:
    case param_sample_14:
    case param_sample_15:
    case param_sample_16:
      return getSampleName(sDesc, value);
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
  float32x4_t vTempo = vdupq_n_f32(sTempo);
  sSampleCounterIncrementBPM[0] = vTempo * sSampleBPMRecip[0];
  sSampleCounterIncrementBPM[1] = vTempo * sSampleBPMRecip[1];
  sSampleCounterIncrementBPM[2] = vTempo * sSampleBPMRecip[2];
  sSampleCounterIncrementBPM[3] = vTempo * sSampleBPMRecip[3];
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
  maskStopped[0] = vdupq_n_u32(-1);
  maskStopped[1] = vdupq_n_u32(-1);
  maskStopped[2] = vdupq_n_u32(-1);
  maskStopped[3] = vdupq_n_u32(-1);
}

__unit_callback void unit_pitch_bend(uint16_t bend) {
  sSampleCounterIncrementPitch = vdupq_n_f32(fastpow2((bend - PITCH_BEND_CENTER) * PITCH_BEND_SENSITIVITY * OCTAVE_RECIP));
}

__unit_callback void unit_channel_pressure(uint8_t pressure) {
  sChannelPressure = vdup_n_f32(pressure * CONTROL_CHANGE_SENSITIVITY);
}

__unit_callback void unit_aftertouch(uint8_t note, uint8_t aftertouch) {
  ((float *)sAmp)[(note - TRACK_1_NOTE) & (TRACK_COUNT - 1)] = aftertouch * AFTERTOUCH_SENSITIVITY;
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
