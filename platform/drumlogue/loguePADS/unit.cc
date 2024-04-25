/*
 *  File: uint.cc
 *
 *  loguePADS Synth unit.
 *
 *
 *  2023-2024 (c) Oleg Burdaev
 *  mailto: dukesrg@gmail.com
 */

#include "unit.h"

#include <cstddef>
#include <cstdint>
#include <arm_neon.h>
#include <string.h>
#include <stdio.h>

#include "runtime.h"
#include "attributes.h"
#include "fastpow.h"
#include "arm.h"


#define TRACK_COUNT LOGUEPAD
#if LOGUEPAD == 2
#define VECTOR_COUNT 1
#else 
#define VECTOR_COUNT (TRACK_COUNT >> 2)
#endif
#if LOGUEPAD == 2 || LOGUEPAD == 4
#define LAYER_XFADE_RATE_BITMASK ((1 << LAYER_XFADE_RATE_BITS) - 1)
#define GROUP_COUNT 1
#elif LOGUEPAD == 8 || LOGUEPAD == 16
#define GROUP_COUNT 4
#endif

#define OCTAVE_RECIP .083333333f // 1/12
#define PITCH_BEND_CENTER 8192
#define PITCH_BEND_SENSITIVITY 1.220703125e-4f // 24/8192
#define PITCH_TUNE_CENTER 6000
#define PITCH_TUNE_SENSITIVITY .01f
#define CENTER_NOTE 60.f
#define TRACK_1_NOTE 60 // C4
#define BPM_TUNE_SENSITIVITY .01f
#define VELOCITY_SENSITIVITY 7.8740157e-3f // 1/127
#define CONTROL_CHANGE_SENSITIVITY 7.8740157e-3f // 1/127
#define AFTERTOUCH_SENSITIVITY 7.8740157e-3f // 1/127

#if LOGUEPAD == 2 || LOGUEPAD == 4
#define DECAY_RATE -6.25e-4f // 0.05 * -60dB / 48000 * 1 / 0.1
#define RELEASE_RATE_MAX -33600.f // 0.05 * -1680dB
#define XFADE_DB_MIN -1680.f
#if LAYER_XFADE_RATE_BITS == 6
#define XFADE_DB_RATE -.01875f // 0.05 * -24dB / 64 
#endif
#endif

enum {
  param_gate_note = 0U,
  param_group_mode_1,
#if LOGUEPAD == 8
  param_unused_1,
  param_unused_2,
#endif
  param_sample_mode_1,
  param_sample_mode_2,
#if LOGUEPAD == 8 || LOGUEPAD == 16
  param_sample_mode_3,
  param_sample_mode_4,
#endif
#if LOGUEPAD == 16
  param_sample_mode_5,
  param_sample_mode_6,
#endif
  param_sample_1,
  param_sample_2,
#if LOGUEPAD != 2
  param_sample_3,
  param_sample_4,
#endif
#if LOGUEPAD == 2 || LOGUEPAD == 4
  param_tune_1,
  param_tune_2,
#if LOGUEPAD == 4
  param_tune_3,
  param_tune_4,
#endif
  param_level_1,
  param_level_2,
#if LOGUEPAD == 4
  param_level_3,
  param_level_4,
#endif
#else
  param_sample_5,
  param_sample_6,
  param_sample_7,
  param_sample_8,
#endif
#if LOGUEPAD == 2
  param_decay_1,
  param_decay_2,
  param_start_1,
  param_start_2,
  param_end_1,
  param_end_2,
  param_thd_note_low_1,
  param_thd_note_low_2,
  param_thd_note_high_1,
  param_thd_note_high_2,
  param_thd_vel_low_1,
  param_thd_vel_low_2,
  param_thd_vel_high_1,
  param_thd_vel_high_2
#elif LOGUEPAD == 4
  param_thd_low_1,
  param_thd_low_2,
  param_thd_low_3,
  param_thd_low_4,
  param_thd_high_1,
  param_thd_high_2,
  param_thd_high_3,
  param_thd_high_4
#elif LOGUEPAD == 8
  param_tune_1,
  param_tune_2,
  param_tune_3,
  param_tune_4,
  param_tune_5,
  param_tune_6,
  param_tune_7,
  param_tune_8
#elif LOGUEPAD == 16
  param_sample_9,
  param_sample_10,
  param_sample_11,
  param_sample_12,
  param_sample_13,
  param_sample_14,
  param_sample_15,
  param_sample_16
#endif
};

enum {
#if LOGUEPAD == 2 || LOGUEPAD == 4
  group_mode_layers = 0U,
  group_mode_gate_chain,
#elif LOGUEPAD == 8 || LOGUEPAD == 16
  group_mode_gate_chain = 0U,
#endif
  group_mode_sustain_chain,
  group_mode_chain,
  group_mode_gate_random,
  group_mode_sustain_random,
  group_mode_chain_random,
  group_mode_count
};

enum {
  sample_mode_gate = 0U,
  sample_mode_sustain,
  sample_mode_repeat,
  sample_mode_toggle,
#if LOGUEPAD == 8 || LOGUEPAD == 16
  sample_mode_group_1,
  sample_mode_group_2,
  sample_mode_group_3,
  sample_mode_group_4,
#endif
  sample_mode_count
};

const unit_runtime_desc_t *sDesc;
static int32_t sParams[PARAM_COUNT];
static const float *sSamplePtr[VECTOR_COUNT << 2];
static uint32_t seed = 0x363812fd;

static uint32_t sSeqIndex[GROUP_COUNT];
static uint32_t sGroupMode[GROUP_COUNT];
static uint32_t sSampleMode[TRACK_COUNT];
static uint32_t sSampleGroup[TRACK_COUNT];
static uint32_t sGroupSamplesCount[GROUP_COUNT];
static uint32_t sGroupSamples[GROUP_COUNT][TRACK_COUNT];
static uint32_t maskSeqChain[GROUP_COUNT];
static uint32_t maskSeqStopped[GROUP_COUNT];

static float sNote;
static float sPitchBend;
static float sTempo;
static float32x2_t sChannelPressure;

#if LOGUEPAD == 2 || LOGUEPAD == 4
static float32x4_t sSampleStartPoint[VECTOR_COUNT];
static float32x4_t sSampleEndPoint[VECTOR_COUNT];
static float32x4_t sLoopStartPoint[VECTOR_COUNT];
static float32x4_t sSampleStart[VECTOR_COUNT];
static float32x4_t sLoopEnd[VECTOR_COUNT];
static float32x4_t sLoopSize[VECTOR_COUNT];
static float32x4_t sDecayRate[VECTOR_COUNT];
static float32x4_t sReleaseRate[VECTOR_COUNT];
static float32x4_t sDecay[VECTOR_COUNT];
static float32x4_t sDecayLevel[VECTOR_COUNT];
static uint32x4_t sDecayMask[VECTOR_COUNT];
static float32x4_t sLevel;
static float32x4_t sXfade;
static float32x4_t sNoteThdLow;
static float32x4_t sNoteThdHigh;
static float32x4_t sVelocityThdLow;
static float32x4_t sVelocityThdHigh;
static float32x4_t sNoteRateLow;
static float32x4_t sNoteRateHigh;
static float32x4_t sVelocityRateLow;
static float32x4_t sVelocityRateHigh;
#endif

static float32x4_t sBPMTune[VECTOR_COUNT];
static float32x4_t sPitchTune[VECTOR_COUNT];
static float32x4_t sAmp[VECTOR_COUNT];
static uint32x4_t maskNoSample[VECTOR_COUNT];
static uint32x4_t maskSampleBPM[VECTOR_COUNT];
static uint32x4_t maskRcvNoteOff[VECTOR_COUNT];
static uint32x4_t maskOneShot[VECTOR_COUNT];
static uint32x4_t maskStopped[VECTOR_COUNT];
static uint32x4_t maskLatched[VECTOR_COUNT];
static float32x4_t sSampleBPM[VECTOR_COUNT];
static float32x4_t sSampleBPMRecip[VECTOR_COUNT];
static float32x4_t sSampleCounter[VECTOR_COUNT];
static float32x4_t sSampleCounterIncrementPitch[VECTOR_COUNT];
static float32x4_t sSampleCounterIncrementBPM[VECTOR_COUNT];
static float32x4_t sSampleSize[VECTOR_COUNT];
static uint32x4_t sSampleChannels[VECTOR_COUNT];
static uint32x4_t sSampleChannelOffset2[VECTOR_COUNT];

fast_inline uint32_t random() {
  seed ^= seed >> 13;
  seed ^= seed >> 17;
  seed ^= seed << 5;
  return seed;
}

#if LOGUEPAD == 2 || LOGUEPAD == 4
fast_inline static void setSampleCounters(uint32_t track) {
  if (((float *)sSampleStartPoint)[track] < ((float *)sSampleEndPoint)[track]) {
    ((float *)sSampleStart)[track] = ((float *)sSampleStartPoint)[track];
  } else {
    ((float *)sSampleStart)[track] = 0.f;
  }

  if (((uint32_t *)maskOneShot)[track] != 0 || ((float *)sLoopStartPoint)[track] >= ((float *)sSampleEndPoint)[track]) {
    ((float *)sLoopSize)[track] = ((float *)sSampleEndPoint)[track] - ((float *)sSampleStart)[track];
  } else {
    ((float *)sLoopSize)[track] = ((float *)sSampleEndPoint)[track] - ((float *)sLoopStartPoint)[track];
  }
  
  ((float *)sSampleStart)[track] *= ((float *)sSampleSize)[track];
  ((float *)sLoopEnd)[track] = ((float *)sSampleEndPoint)[track] * ((float *)sSampleSize)[track];
  ((float *)sLoopSize)[track] *= ((float *)sSampleSize)[track];
}
#endif

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

fast_inline void getSampleData(const unit_runtime_desc_t *desc, uint32_t index, uint32_t track) {
  const sample_wrapper_t * sample;
  static char * lastspace;
  if (index == 0 || (sample = getSampleWrapper(desc, index - 1)) == nullptr) {
    sample = getSampleWrapper(desc, 0);
    ((uint32_t *)maskNoSample)[track] = -1;
  } else {
    ((uint32_t *)maskNoSample)[track] = 0;
  }
  if (strcmp(&sample->name[strlen(sample->name) - 3], "BPM") == 0 && (lastspace = strrchr((char *)sample->name, ' ')) != NULL && sscanf(lastspace, "%f", &((float *)sSampleBPM)[track]) == 1) {
    ((float *)sSampleBPMRecip)[track] = 1.f / (((float *)sSampleBPM)[track] + ((float *)sBPMTune)[track]);
    ((float *)sSampleCounterIncrementBPM)[track] = sTempo * ((float *)sSampleBPMRecip)[track];
    ((uint32_t *)maskSampleBPM)[track] = -1;
  } else {
    ((uint32_t *)maskSampleBPM)[track] = 0;
  }
  ((float *)sSampleSize)[track] = sample->frames;
  ((uint32_t *)sSampleChannels)[track] = sample->channels;
  sSamplePtr[track] = sample->sample_ptr;
  ((uint32_t *)sSampleChannelOffset2)[track] = sample->channels - 1;
#if LOGUEPAD == 2 || LOGUEPAD == 4
  setSampleCounters(track);
#endif
}

fast_inline static char * joinParams(const char * lut, uint32_t value, uint32_t count, uint32_t positions) {
  static uint32_t modes[4];
  static char s[UNIT_PARAM_NAME_LEN + 1];
  static const char * formats[4] = {
    "%c",
    "%c.%c",
    "%c.%c.%c",
    "%c.%c.%c.%c"
  };
  for (uint32_t i = 0; i < positions; i++) {
    modes[i] = value % count;
    value /= count;
  }
  sprintf(s, formats[positions - 1], lut[modes[0]], lut[modes[1]], lut[modes[2]], lut[modes[3]]);
  return s;
};

fast_inline static void setPitch() {
#if LOGUEPAD == 2 || LOGUEPAD == 4 || LOGUEPAD == 8
  float32x4_t pitch[VECTOR_COUNT];
  for (uint32_t i = 0; i < VECTOR_COUNT; i++)
    pitch[i] = (sNote + sPitchBend - sPitchTune[i]) * OCTAVE_RECIP;
  for (uint32_t i = 0; i < TRACK_COUNT; i++)
    ((float *)sSampleCounterIncrementPitch)[i] = fastpow2(((float *)pitch)[i]);
#else
  float32x4_t pitch = vdupq_n_f32(fastpow2(sPitchBend * OCTAVE_RECIP));
  for (uint32_t i = 0; i < VECTOR_COUNT; i++)
    sSampleCounterIncrementPitch[i] = pitch;
#endif
}

fast_inline static void setSampleMode(uint32_t track) {
  uint32_t mode = sSampleMode[track];
#if LOGUEPAD == 2 || LOGUEPAD == 4
  if (sGroupMode[0] != group_mode_layers)
    return; 
#endif
  ((uint32_t *)maskOneShot)[track] = mode < sample_mode_repeat ? -1 : 0;
  ((uint32_t *)maskRcvNoteOff)[track] = mode == sample_mode_gate || mode == sample_mode_repeat ? -1 : 0;
  ((uint32_t *)maskLatched)[track] = mode == sample_mode_toggle ? -1 : 0;
}

fast_inline static void setSampleGroup(uint32_t track, uint32_t group) {
  uint32_t oldGroup = sSampleGroup[track];
  if (oldGroup == group)
    return;
  sSampleGroup[track] = group;
  if (oldGroup < GROUP_COUNT) {
    sGroupSamplesCount[oldGroup] = 0;
    for (uint32_t i = 0; i < TRACK_COUNT; i++) {
      if (sSampleGroup[i] == oldGroup) {
        sGroupSamples[oldGroup][sGroupSamplesCount[oldGroup]++] = i;
      }
    }  
  }
  if (group < GROUP_COUNT) {
    sGroupSamplesCount[group] = 0;
    for (uint32_t i = 0; i < TRACK_COUNT; i++) {
      if (sSampleGroup[i] == group) {
        sGroupSamples[group][sGroupSamplesCount[group]++] = i;
      }
    }
  }
}

fast_inline static void setGroupMode(uint32_t group) {
  uint32_t mode = sGroupMode[group];

  maskSeqChain[group] = mode == group_mode_chain || mode == group_mode_chain_random ? -1 : 0;
  if (maskSeqChain[group] != 0)
    maskSeqStopped[group] = -1;

#if LOGUEPAD == 2 || LOGUEPAD == 4
  if (mode == group_mode_layers) {
    for (uint32_t i = 0; i < TRACK_COUNT; i++) {
      setSampleGroup(i, GROUP_COUNT); 
      setSampleMode(i);
    }
    return;
  } else {
    for (uint32_t i = 0; i < TRACK_COUNT; i++)
      setSampleGroup(i, 0);
  }
#endif

  for (uint32_t i = 0; i < sGroupSamplesCount[group]; i++) { 
    ((uint32_t *)maskOneShot)[sGroupSamples[group][i]] = -1;
    ((uint32_t *)maskRcvNoteOff)[sGroupSamples[group][i]] = mode == group_mode_gate_chain || mode == group_mode_gate_random ? -1 : 0;
    ((uint32_t *)maskLatched)[sGroupSamples[group][i]] = 0;
  }
}

fast_inline static void nextSeq(uint32_t track) {  
  uint32_t group = sSampleGroup[track];
  if (group == GROUP_COUNT)
    return;

  ((uint32_t *)maskStopped)[sGroupSamples[group][sSeqIndex[group]]] = -1;

  if (sGroupMode[group] == group_mode_chain_random || sGroupMode[group] == group_mode_gate_random || sGroupMode[group] == group_mode_sustain_random)
    sSeqIndex[group] = random();
  else if (maskSeqChain[group] != 0 && maskSeqStopped[group] != 0)
    sSeqIndex[group] = -2;

  for (uint32_t i = 0; i < sGroupSamplesCount[group]; i++) {
    sSeqIndex[group] = (sSeqIndex[group] + 1) % sGroupSamplesCount[group];
    if (((uint32_t *)maskNoSample)[sGroupSamples[group][sSeqIndex[group]]] == 0) {
      if (maskSeqChain[group] == 0 || maskSeqStopped[group] == 0) {
#if LOGUEPAD == 2 || LOGUEPAD == 4
        ((float *)sDecay)[sGroupSamples[group][sSeqIndex[group]]] = 0.f;
        ((float *)sDecayLevel)[sGroupSamples[group][sSeqIndex[group]]] = 1.f;
        ((uint32_t *)sDecayMask)[sGroupSamples[group][sSeqIndex[group]]] = -1;
        ((float *)sSampleCounter)[sGroupSamples[group][sSeqIndex[group]]] = ((float *)sSampleStart)[sGroupSamples[group][sSeqIndex[group]]];
#elif LOGUEPAD == 8 || LOGUEPAD == 16
        ((float *)sSampleCounter)[sGroupSamples[group][sSeqIndex[group]]] = 0.f;
#endif
        ((uint32_t *)maskStopped)[sGroupSamples[group][sSeqIndex[group]]] = 0;
      }
      break;
    }
  }
}

fast_inline void noteOn(uint8_t note, uint8_t velocity) {
  uint32_t track = (note - TRACK_1_NOTE) & (TRACK_COUNT - 1);
  uint32_t group = sSampleGroup[track];
#if LOGUEPAD == 2 || LOGUEPAD == 4
  sNote = note;
  setPitch();  
  if (group == GROUP_COUNT) {
    float32x4_t vNote = vdupq_n_f32(sNote); 
    float32x4_t vVelocity = vdupq_n_f32(velocity); 
    sXfade = sNoteRateLow * vreinterpretq_f32_u32(vbicq_u32(vreinterpretq_u32_f32(sNoteThdLow - vNote), vcgtq_f32(vNote, sNoteThdLow)));
    sXfade += sNoteRateHigh * vreinterpretq_f32_u32(vbicq_u32(vreinterpretq_u32_f32(vNote - sNoteThdHigh), vcltq_f32(vNote, sNoteThdHigh)));
    sXfade += sVelocityRateLow * vreinterpretq_f32_u32(vbicq_u32(vreinterpretq_u32_f32(sVelocityThdLow - vVelocity), vcgtq_f32(vVelocity, sVelocityThdLow)));
    sXfade += sVelocityRateHigh * vreinterpretq_f32_u32(vbicq_u32(vreinterpretq_u32_f32(vVelocity - sVelocityThdHigh), vcltq_f32(vVelocity, sVelocityThdHigh)));
    for (uint32_t i = 0; i < TRACK_COUNT; i++) {
      sXfade[i] = fasterpow(10.f, sXfade[i]);
    }    
    maskStopped[0] = maskLatched[0] & ~maskStopped[0];
    sDecay[0] = vdupq_n_f32(0.f);
    sDecayLevel[0] = vdupq_n_f32(1.f);
    sDecayMask[0] = vdupq_n_u32(-1);
    sSampleCounter[0] = sSampleStart[0];
  } else {
    sXfade = vdupq_n_f32(1.f);
    maskSeqStopped[group] = maskSeqChain[group] & ~maskSeqStopped[group];
    nextSeq(track);
  }
  sAmp[0] = vdupq_n_f32(velocity * VELOCITY_SENSITIVITY);
#elif LOGUEPAD == 8 || LOGUEPAD == 16
  if (group == GROUP_COUNT) {
    ((uint32_t *)maskStopped)[track] = ((uint32_t *)maskLatched)[track] & ~((uint32_t *)maskStopped)[track];
    ((float *)sSampleCounter)[track] = 0.f;
    ((float *)sAmp)[track] = velocity * VELOCITY_SENSITIVITY;
  } else {
    maskSeqStopped[group] = maskSeqChain[group] & ~maskSeqStopped[group];
    nextSeq(track);
    for (uint32_t i = 0; i < sGroupSamplesCount[group]; i++) { 
      ((float *)sAmp)[sGroupSamples[group][i]] = velocity * VELOCITY_SENSITIVITY;
    }
  }
#endif
}

fast_inline void noteOff(uint8_t note) {
  uint32_t track = (note - TRACK_1_NOTE) & (TRACK_COUNT - 1);
  uint32_t group = sSampleGroup[track];
  if (group == GROUP_COUNT) {
#if LOGUEPAD == 2 || LOGUEPAD == 4
    sDecayMask[0] = vbicq_u32(sDecayMask[0], maskRcvNoteOff[0]);
#elif LOGUEPAD == 8 || LOGUEPAD == 16
    ((uint32_t *)maskStopped)[track] |= ((uint32_t *)maskRcvNoteOff)[track];
#endif
  } else {
#if LOGUEPAD == 2 || LOGUEPAD == 4
    ((uint32_t *)sDecayMask)[sGroupSamples[group][sSeqIndex[group]]] &= ~((uint32_t *)maskRcvNoteOff)[sGroupSamples[group][sSeqIndex[group]]];
#elif LOGUEPAD == 8 || LOGUEPAD == 16
    for (uint32_t i = 0; i < sGroupSamplesCount[group]; i++) { 
      ((uint32_t *)maskStopped)[sGroupSamples[group][i]] |= ((uint32_t *)maskRcvNoteOff)[sGroupSamples[group][i]];
    }
#endif
  }
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
  for (uint32_t i = 0; i < VECTOR_COUNT; i++)
    maskStopped[i] = vdupq_n_u32(-1);
  for (uint32_t i = 0; i < GROUP_COUNT; i++) {
    sSeqIndex[i] = -1;
    maskSeqStopped[i] = -1;
  }

#if LOGUEPAD == 2
  for (uint32_t i = TRACK_COUNT; i < (VECTOR_COUNT << 2); i++) {
    ((float *)sSampleStartPoint)[i] = 0.f;
    ((float *)sLoopStartPoint)[i] = 0.f;
    ((float *)sSampleEndPoint)[i] = 1.f;
    ((float *)sDecayRate)[i] = 0.f;
    ((float *)sReleaseRate)[i] = 0.f;
    getSampleData(sDesc, 0, i);
  }
#elif LOGUEPAD == 4
  for (uint32_t i = 0; i < VECTOR_COUNT; i++) {
    sSampleStartPoint[i] = vdupq_n_f32(0.f);
    sLoopStartPoint[i] = vdupq_n_f32(0.f);
    sSampleEndPoint[i] = vdupq_n_f32(1.f);
    sDecayRate[i] = vdupq_n_f32(0.f);
    sReleaseRate[i] = vdupq_n_f32(RELEASE_RATE_MAX);
  }
#elif LOGUEPAD == 8
  sNote = CENTER_NOTE;
#else
  sNote = CENTER_NOTE;
  for (uint32_t i = 0; i < VECTOR_COUNT; i++) {
    sBPMTune[i] = vdupq_n_f32(0.f);
    sPitchTune[i] = vdupq_n_f32(PITCH_TUNE_CENTER * PITCH_BEND_SENSITIVITY);
  }
#endif

  return k_unit_err_none;
}

__unit_callback void unit_teardown() {
}

__unit_callback void unit_reset() {
  for (uint32_t i = 0; i < VECTOR_COUNT; i++)
    maskStopped[i] = vdupq_n_u32(-1);
  for (uint32_t i = 0; i < GROUP_COUNT; i++) {
    sSeqIndex[i] = -1;
    maskSeqStopped[i] = -1;
  }
}

__unit_callback void unit_resume() {
}

__unit_callback void unit_suspend() {
}

__unit_callback void unit_render(const float * in, float * out, uint32_t frames) {
  (void)in;

#if LOGUEPAD == 2 || LOGUEPAD == 4
  float32x4_t vDecayLevel = sDecayLevel[0];
  sDecay[0] += vbslq_f32(sDecayMask[0], sDecayRate[0], sReleaseRate[0]) * (float)frames;
  sDecayLevel[0] = (float32x4_t){fasterpow(10.f, sDecay[0][0]), fasterpow(10.f, sDecay[0][1]), fasterpow(10.f, sDecay[0][2]), fasterpow(10.f, sDecay[0][3])};
  const float32x4_t vDecayDelta = (sDecayLevel[0] - vDecayLevel) * (1.f / frames);
#endif

  float32x4_t vSampleCounterIncrement[VECTOR_COUNT];
  for (uint32_t i = 0; i < VECTOR_COUNT; i++) {
    vSampleCounterIncrement[i] = vbslq_f32(maskSampleBPM[i], sSampleCounterIncrementBPM[i], sSampleCounterIncrementPitch[i]);
  }

  float * __restrict out_p = out;
  const float * out_e = out_p + (frames << 1);
  for (; out_p != out_e; out_p += 2) {
    float32x4_t vAmp;
    float32x4_t vOut1 = vdupq_n_f32(0.f);
    float32x4_t vOut2 = vdupq_n_f32(0.f);

    for (uint32_t i = 0; i < VECTOR_COUNT; i++) {
      uint32x4_t maskSampleCounterOverflow;
#if LOGUEPAD == 2 || LOGUEPAD == 4
      vAmp = sAmp[i] * sLevel * sXfade * vDecayLevel;
      vDecayLevel += vDecayDelta;
      maskSampleCounterOverflow = vcgeq_f32(sSampleCounter[i], sLoopEnd[i]);
      sSampleCounter[i] = vbslq_f32(maskSampleCounterOverflow, sSampleCounter[i] - sLoopSize[i], sSampleCounter[i]);
#elif LOGUEPAD == 8 || LOGUEPAD == 16
      vAmp = sAmp[i];
      maskSampleCounterOverflow = vcgeq_f32(sSampleCounter[i], sSampleSize[i]);
      sSampleCounter[i] = vbslq_f32(maskSampleCounterOverflow, sSampleCounter[i] - sSampleSize[i], sSampleCounter[i]);
#endif
      maskStopped[i] |= maskOneShot[i] & maskSampleCounterOverflow;
      for (uint32_t j = 0; j < 4; j++) {
        uint32_t track = i * 4 + j;
        if (maskSampleCounterOverflow[j] && sSampleGroup[track] < GROUP_COUNT && sSeqIndex[sSampleGroup[track]] == track && maskSeqChain[sSampleGroup[track]] != 0)
          nextSeq(track);
      }
      uint32x4_t vSampleOffset1 = vcvtq_u32_f32(sSampleCounter[i]) * sSampleChannels[i];
      uint32x4_t vSampleOffset2 = vSampleOffset1 + sSampleChannelOffset2[i];
      vOut1 += vAmp * vreinterpretq_f32_u32(vbicq_u32(vreinterpretq_u32_f32((float32x4_t){sSamplePtr[i * 4][vSampleOffset1[0]], sSamplePtr[i * 4 + 1][vSampleOffset1[1]], sSamplePtr[i * 4 + 2][vSampleOffset1[2]], sSamplePtr[i * 4 + 3][vSampleOffset1[3]]}), maskStopped[i] | maskNoSample[i]));
      vOut2 += vAmp * vreinterpretq_f32_u32(vbicq_u32(vreinterpretq_u32_f32((float32x4_t){sSamplePtr[i * 4][vSampleOffset2[0]], sSamplePtr[i * 4 + 1][vSampleOffset2[1]], sSamplePtr[i * 4 + 2][vSampleOffset2[2]], sSamplePtr[i * 4 + 3][vSampleOffset2[3]]}), maskStopped[i] | maskNoSample[i]));
      sSampleCounter[i] += vSampleCounterIncrement[i];
    }
    vst1_f32(out_p, sChannelPressure * vpadd_f32(vpadd_f32(vget_low_f32(vOut1), vget_high_f32(vOut1)), vpadd_f32(vget_low_f32(vOut2), vget_high_f32(vOut2))));
  }
  return;
}

__unit_callback void unit_set_param_value(uint8_t id, int32_t value) {
  static uint32_t mask;
  value = (int16_t)value;
  static uint32_t groupModePositions;
  static uint32_t groupModeOffset;
  static uint32_t sampleModePositions;
  static uint32_t sampleModeOffset;
  sParams[id] = value;
  switch (id) {
    case param_group_mode_1:
#if LOGUEPAD == 2 || LOGUEPAD == 4
      groupModePositions = 1;
#elif LOGUEPAD == 8 || LOGUEPAD == 16
      groupModePositions = 4;
#endif
      id -= param_group_mode_1;
      groupModeOffset = id * groupModePositions;

      for (uint32_t i = groupModeOffset; i < groupModeOffset + groupModePositions; i++) {
        mask = value % group_mode_count;
        value /= group_mode_count;
        sGroupMode[i] = mask;
        setGroupMode(i);
      }
      break;
    case param_sample_mode_1:
    case param_sample_mode_2:
#if LOGUEPAD == 8
    case param_sample_mode_3:
    case param_sample_mode_4:
#endif
#if LOGUEPAD == 2
      sampleModePositions = 1;
#else
      sampleModePositions = 2;      
#endif
      sampleModeOffset = (id - param_sample_mode_1) * sampleModePositions;
#if LOGUEPAD == 16
goto set_sample_mode;
    case param_sample_mode_3:
    case param_sample_mode_4:
    case param_sample_mode_5:
    case param_sample_mode_6:
      sampleModePositions = 3;
      sampleModeOffset = (id - param_sample_mode_3) * sampleModePositions + 4;
set_sample_mode:
#endif
      for (uint32_t i = sampleModeOffset; i < sampleModeOffset + sampleModePositions; i++) {
        mask = value % sample_mode_count;
        value /= sample_mode_count;
        sSampleMode[i] = mask;
#if LOGUEPAD == 2 || LOGUEPAD == 4
        setSampleMode(i);
        setSampleCounters(i);
#elif LOGUEPAD == 8 || LOGUEPAD == 16
        if (mask < sample_mode_group_1) {
          setSampleGroup(i, GROUP_COUNT);
          setSampleMode(i);
        } else {
          setSampleGroup(i, mask - sample_mode_group_1);
          setGroupMode(mask - sample_mode_group_1);
        }
#endif        
      }
      break;
    case param_sample_1:
    case param_sample_2:
#if LOGUEPAD == 4 || LOGUEPAD == 8 || LOGUEPAD == 16
    case param_sample_3:
    case param_sample_4:
#endif
#if LOGUEPAD == 8 || LOGUEPAD == 16
    case param_sample_5:
    case param_sample_6:
    case param_sample_7:
    case param_sample_8:
#endif
#if LOGUEPAD == 16
    case param_sample_9:
    case param_sample_10:
    case param_sample_11:
    case param_sample_12:
    case param_sample_13:
    case param_sample_14:
    case param_sample_15:
    case param_sample_16:
#endif
      id -= param_sample_1;
      getSampleData(sDesc, value, id);
      break;
#if LOGUEPAD == 2 || LOGUEPAD == 4 || LOGUEPAD == 8
    case param_tune_1:
    case param_tune_2:
#if LOGUEPAD == 4 || LOGUEPAD == 8
    case param_tune_3:
    case param_tune_4:
#if LOGUEPAD == 8
    case param_tune_5:
    case param_tune_6:
    case param_tune_7:
    case param_tune_8:
#endif
#endif
      id -= param_tune_1;
      ((float *)sBPMTune)[id] = value * BPM_TUNE_SENSITIVITY;
      ((float *)sPitchTune)[id] = (value + PITCH_TUNE_CENTER) * PITCH_TUNE_SENSITIVITY;
      ((float *)sSampleBPMRecip)[id] = 1.f / (((float *)sSampleBPM)[id] + ((float *)sBPMTune)[id]);
      ((float *)sSampleCounterIncrementBPM)[id] = sTempo * ((float *)sSampleBPMRecip)[id];
      ((float *)sSampleCounterIncrementPitch)[id] = fastpow2((sNote + sPitchBend - ((float *)sPitchTune)[id]) * OCTAVE_RECIP);
      break;
#endif
#if LOGUEPAD == 2 || LOGUEPAD == 4
    case param_level_1:
    case param_level_2:
#if LOGUEPAD == 4
    case param_level_3:
    case param_level_4:
#endif
      id -= param_level_1;
      sLevel[id] = fastpow(10.f, value * 5.e-3f);
      break;
#endif
#if LOGUEPAD == 2
    case param_decay_1:
    case param_decay_2:
      id -= param_decay_1;
      if (value < 0 ) {
        ((float *)sReleaseRate)[id] = - DECAY_RATE / value;
        ((float *)sDecayRate)[id] = 0.f;
      } else if (value > 0 ) {
        ((float *)sReleaseRate)[id] = RELEASE_RATE_MAX;
        ((float *)sDecayRate)[id] = DECAY_RATE / value;
      } else {
        ((float *)sDecayRate)[id] = 0.f;
        ((float *)sReleaseRate)[id] = RELEASE_RATE_MAX;
      }
      break;
    case param_start_1:
    case param_start_2:
      id -= param_start_1;
      if (value < 0 ) {
        ((float *)sSampleStartPoint)[id] = 0.f;
        ((float *)sLoopStartPoint)[id] = 1.f + value * .001f;
      } else {
        ((float *)sSampleStartPoint)[id] = value * .001f;
        ((float *)sLoopStartPoint)[id] = value * .001f;
      }
      setSampleCounters(id);
      break;
    case param_end_1:
    case param_end_2:
      id -= param_end_1;
      ((float *)sSampleEndPoint)[id] = value * .001f;
      setSampleCounters(id);
      break;
    case param_thd_note_low_1:
    case param_thd_note_low_2:
      id -= param_thd_note_low_1;
      sNoteThdLow[id] = value >> LAYER_XFADE_RATE_BITS;
      sNoteRateLow[id] = (value = -value & LAYER_XFADE_RATE_BITMASK) == 0 ? XFADE_DB_MIN : value * XFADE_DB_RATE;
      break;
    case param_thd_note_high_1:
    case param_thd_note_high_2:
      id -= param_thd_note_high_1;
      sNoteThdHigh[id] = value >> LAYER_XFADE_RATE_BITS;
      sNoteRateHigh[id] = (value = -value & LAYER_XFADE_RATE_BITMASK) == 0 ? XFADE_DB_MIN : value * XFADE_DB_RATE;
      break;
    case param_thd_vel_low_1:
    case param_thd_vel_low_2:
      id -= param_thd_vel_low_1;
      sVelocityThdLow[id] = value >> LAYER_XFADE_RATE_BITS;
      sVelocityRateLow[id] = (value = -value & LAYER_XFADE_RATE_BITMASK) == 0 ? XFADE_DB_MIN : value * XFADE_DB_RATE;
      break;
    case param_thd_vel_high_1:
    case param_thd_vel_high_2:
      id -= param_thd_vel_high_1;
      sVelocityThdHigh[id] = value >> LAYER_XFADE_RATE_BITS;
      sVelocityRateHigh[id] = (value = -value & LAYER_XFADE_RATE_BITMASK) == 0 ? XFADE_DB_MIN : value * XFADE_DB_RATE;
      break;
#elif LOGUEPAD == 4
    case param_thd_low_1:
    case param_thd_low_2:
    case param_thd_low_3:
    case param_thd_low_4:
      id -= param_thd_low_1;
      if (value < 0) {
        value += 1 << (LAYER_XFADE_RATE_BITS + 7);
        sNoteThdLow[id] = 0.f;
        sNoteRateLow[id] = 0.f;
        sVelocityThdLow[id] = value >> LAYER_XFADE_RATE_BITS;
        sVelocityRateLow[id] = (value = -value & LAYER_XFADE_RATE_BITMASK) == 0 ? XFADE_DB_MIN : value * XFADE_DB_RATE;
      } else {
        sNoteThdLow[id] = value >> LAYER_XFADE_RATE_BITS;
        sNoteRateLow[id] = (value = -value & LAYER_XFADE_RATE_BITMASK) == 0 ? XFADE_DB_MIN : value * XFADE_DB_RATE;
        sVelocityThdLow[id] = 0.f;
        sVelocityRateLow[id] = 0.f;
      }
      break;
    case param_thd_high_1:
    case param_thd_high_2:
    case param_thd_high_3:
    case param_thd_high_4:
      id -= param_thd_high_1;
      if (value < 0) {
        value += 1 << (LAYER_XFADE_RATE_BITS + 7);
        sNoteThdHigh[id] = 127.f;
        sNoteRateHigh[id] = 0.f;
        sVelocityThdHigh[id] = value >> LAYER_XFADE_RATE_BITS;
        sVelocityRateHigh[id] = (value = -value & LAYER_XFADE_RATE_BITMASK) == 0 ? XFADE_DB_MIN : value * XFADE_DB_RATE;
      } else {
        sNoteThdHigh[id] = value >> LAYER_XFADE_RATE_BITS;
        sNoteRateHigh[id] = (value = -value & LAYER_XFADE_RATE_BITMASK) == 0 ? XFADE_DB_MIN : value * XFADE_DB_RATE;
        sVelocityThdHigh[id] = 127.f;
        sVelocityRateHigh[id] = 0.f;
      }
      break;
#endif
  }
}

__unit_callback int32_t unit_get_param_value(uint8_t id) {
  return sParams[id];
}

__unit_callback const char * unit_get_param_str_value(uint8_t id, int32_t value) {
  value = (int16_t)value;
  static uint32_t groupModePositions;
  static uint32_t sampleModePositions;
  static char s[UNIT_PARAM_NAME_LEN + 1];
#if LOGUEPAD == 2 || LOGUEPAD == 4 || LOGUEPAD == 8
  static uint32_t cents;
  static const char noteNames[][3] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
#endif
#if LOGUEPAD == 2
  static const char * prefix;
#endif
#if LOGUEPAD == 2 || LOGUEPAD == 4
  static const char * groupModes = "LGSCgsc";
  static const char * sampleModes = "GSRT";
#elif LOGUEPAD == 8 || LOGUEPAD == 16
  static const char * groupModes = "GSCgsc";
  static const char * sampleModes = "GSRT1234";
#endif

  switch (id) {
    case param_group_mode_1:
#if LOGUEPAD == 2 || LOGUEPAD == 4
      groupModePositions = 1;
#elif LOGUEPAD == 8 || LOGUEPAD == 16
      groupModePositions = 4;
#endif
      return joinParams(groupModes, value, group_mode_count, groupModePositions); 
    case param_sample_mode_1:
    case param_sample_mode_2:
#if LOGUEPAD == 8
    case param_sample_mode_3:
    case param_sample_mode_4:
#endif
#if LOGUEPAD == 2
      sampleModePositions = 1;
#else
      sampleModePositions = 2;      
#endif
#if LOGUEPAD == 16
      goto get_str_sample_mode;
    case param_sample_mode_3:
    case param_sample_mode_4:
    case param_sample_mode_5:
    case param_sample_mode_6:
      sampleModePositions = 3;
get_str_sample_mode:
#endif
      return joinParams(sampleModes, value, sample_mode_count, sampleModePositions); 
    case param_sample_1:
    case param_sample_2:
#if LOGUEPAD == 4 || LOGUEPAD == 8 || LOGUEPAD == 16
    case param_sample_3:
    case param_sample_4:
#endif
#if LOGUEPAD == 8 || LOGUEPAD == 16
    case param_sample_5:
    case param_sample_6:
    case param_sample_7:
    case param_sample_8:
#endif
#if LOGUEPAD == 16
    case param_sample_9:
    case param_sample_10:
    case param_sample_11:
    case param_sample_12:
    case param_sample_13:
    case param_sample_14:
    case param_sample_15:
    case param_sample_16:
#endif
      return getSampleName(sDesc, value);
      break;
#if LOGUEPAD == 2 || LOGUEPAD == 4 || LOGUEPAD == 8  
    case param_tune_1:
    case param_tune_2:
#if LOGUEPAD == 4 || LOGUEPAD == 8  
    case param_tune_3:
    case param_tune_4:
#if LOGUEPAD == 8  
    case param_tune_5:
    case param_tune_6:
    case param_tune_7:
    case param_tune_8:
#endif
#endif  
      id -= param_tune_1;
      if (((uint32_t *)maskSampleBPM)[id]) {
        sprintf(s, "%+.2f", value * BPM_TUNE_SENSITIVITY);
      } else {
        value += PITCH_TUNE_CENTER;
        cents = value % 100;
        value /= 100;
        sprintf(s, "%s%d.%02d", noteNames[value % 12], value / 12 - 1, cents);
      }
      break;
#endif
#if LOGUEPAD == 2
    case param_decay_1:
    case param_decay_2:
      if (value < 0 ) {
        prefix = "R";
        value = -value;
      } else if (value > 0 ) {
        prefix = "D";
      } else {
        return "Off";
      }
      sprintf(s, "%s%.1fs", prefix, value * .1f);
      break;
    case param_end_1:
    case param_end_2:
    case param_start_1:
    case param_start_2:
      if (value < 0 ) {
        prefix = "L";
        value = 1000 + value;
      } else {
        prefix = "";
      }
      sprintf(s, "%s%.1f%%", prefix, value * .1f);
      break;
    case param_thd_vel_low_1:
    case param_thd_vel_low_2:
    case param_thd_vel_high_1:
    case param_thd_vel_high_2:
      value -= 1 << (LAYER_XFADE_RATE_BITS + 7);
    case param_thd_note_low_1:
    case param_thd_note_low_2:
    case param_thd_note_high_1:
    case param_thd_note_high_2:
#elif LOGUEPAD == 4
    case param_thd_low_1:
    case param_thd_low_2:
    case param_thd_low_3:
    case param_thd_low_4:
    case param_thd_high_1:
    case param_thd_high_2:
    case param_thd_high_3:
    case param_thd_high_4:
#endif
#if LOGUEPAD == 2 || LOGUEPAD == 4
      if (value < 0 ) {
        value += 1 << (LAYER_XFADE_RATE_BITS + 7);
        sprintf(s, "%d.%02d", value >> LAYER_XFADE_RATE_BITS, value & LAYER_XFADE_RATE_BITMASK);
      } else {
        sprintf(s, "%s%d.%02d", noteNames[(value >> LAYER_XFADE_RATE_BITS) % 12], (value >> LAYER_XFADE_RATE_BITS) / 12 - 1, value & LAYER_XFADE_RATE_BITMASK);
      }
#endif
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
  for (uint32_t i = 0; i < VECTOR_COUNT; i++)
    sSampleCounterIncrementBPM[i] = vTempo * sSampleBPMRecip[i];
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
  for (uint32_t i = 0; i < VECTOR_COUNT; i++)
    maskStopped[i] = vdupq_n_u32(-1);
}

__unit_callback void unit_pitch_bend(uint16_t bend) {
  sPitchBend = (bend - PITCH_BEND_CENTER) * PITCH_BEND_SENSITIVITY;
  setPitch();
}

__unit_callback void unit_channel_pressure(uint8_t pressure) {
  sChannelPressure = vdup_n_f32(pressure * CONTROL_CHANGE_SENSITIVITY);
}

__unit_callback void unit_aftertouch(uint8_t note, uint8_t aftertouch) {
#if LOGUEPAD == 2 || LOGUEPAD == 4
  (void)note;
  sAmp[0] = vdupq_n_f32(aftertouch * AFTERTOUCH_SENSITIVITY);
#elif LOGUEPAD == 8 || LOGUEPAD == 16
  uint32_t track = (note - TRACK_1_NOTE) & (TRACK_COUNT - 1);
  uint32_t group = sSampleGroup[track];
  if (group == GROUP_COUNT) {
    ((float *)sAmp)[track] = aftertouch * AFTERTOUCH_SENSITIVITY;
  } else {
    for (uint32_t i = 0; i < sGroupSamplesCount[group]; i++) { 
      ((float *)sAmp)[sGroupSamples[group][i]] = aftertouch * AFTERTOUCH_SENSITIVITY;
    }
  }
#endif
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
