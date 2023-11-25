/*
 *  File: uint.cc
 *
 *  Hyperloop FX unit.
 *
 *
 *  2023 (c) Oleg Burdaev
 *  mailto: dukesrg@gmail.com
 */

#include "unit.h"

#include <cstddef>
#include <cstdint>
#include <arm_neon.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "attributes.h"
#include "arm.h"
#include "glyphs.xbm"

#define TRACK_COUNT 8
#define LENGTH_STEPS_MAX 11
#define LENGTH_STEPS_DEFAULT 7
#define CHUNK_COUNT (1 << LENGTH_STEPS_MAX)
#define CHUNK_COUNT_DEFAULT (1 << LENGTH_STEPS_DEFAULT)
#define CHUNK_SIZE_EXP 12
#define CHUNK_SIZE (1 << CHUNK_SIZE_EXP)
#define CHNUK_SIZE_MASK (CHUNK_SIZE - 1)
#define CHANNEL_COUNT 2
#define FRAME_SIZE (CHANNEL_COUNT * sizeof(float))
#define MIN_TEMPO 43.9453125f // (60 * 48000 / (CHUNK_SIZE * 16))
#define MIN_TEMPO_RECIP .022755555f
#define PLAY_MASK 1
#define RECORD_MASK 2

enum {
  param_play_1 = 0U,
  param_play_2,
  param_play_3,
  param_play_4,
  param_play_5,
  param_play_6,
  param_play_7,
  param_play_8,
  param_record_1,
  param_record_2,
  param_record_3,
  param_record_4,
  param_record_5,
  param_record_6,
  param_record_7,
  param_record_8,
  param_length_1,
  param_length_2,
  param_length_3,
  param_length_4,
  param_length_5,
  param_length_6,
  param_length_7,
  param_length_8
};

static int32_t sParams[PARAM_COUNT];
static float * chunks[TRACK_COUNT][CHUNK_COUNT];
static bool chunkOK[TRACK_COUNT];
static uint32_t maskMute[TRACK_COUNT][CHANNEL_COUNT];
static uint32_t maskRecord[TRACK_COUNT][CHANNEL_COUNT];
static uint32_t maskRestart[TRACK_COUNT];
static float32x4_t sSampleSize[TRACK_COUNT >> 2];
static float32x4_t sSampleCounter[TRACK_COUNT >> 2];
static float32x4_t sSampleCounterIncrement;
static uint32_t sSampleRepeat;

/*===========================================================================*/
/* Private Methods. */
/*===========================================================================*/

fast_inline bool allocateChunks(uint32_t track, uint32_t count) {
  static uint32_t allocated[TRACK_COUNT];
  for (uint32_t i = count; i < allocated[track]; i++)
    free(chunks[track][i]);
  for (uint32_t i = allocated[track]; i < count; i++) {
    chunks[track][i] = (float *)malloc(CHUNK_SIZE * FRAME_SIZE);
    if (chunks[track][i] == NULL) {
      while(i > allocated[track])
        free(chunks[track][--i]);
      return false;
    }
  }
  allocated[track] = count;
  ((float*)sSampleSize)[track] = CHUNK_SIZE * count;
  return true;
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

  for (uint32_t i = 0; i < TRACK_COUNT; i++)
    chunkOK[i] = allocateChunks(i, CHUNK_COUNT_DEFAULT);

  return k_unit_err_none;
}

__unit_callback void unit_teardown() {
  for (uint32_t i = 0; i < TRACK_COUNT; i++)
    chunkOK[i] = allocateChunks(i, 0);
}

__unit_callback void unit_reset() {
  for (uint32_t i = 0; i < TRACK_COUNT; i++)
    ((float *)sSampleCounter)[i] = 0.f;
}

__unit_callback void unit_resume() {
}

__unit_callback void unit_suspend() {
}

__unit_callback void unit_render(const float * in, float * out, uint32_t frames) {
  const float * __restrict in_p = in;
  float * __restrict out_p = out;
  const float * out_e = out_p + (frames << 1);

  sSampleCounter[0] = vreinterpretq_f32_u32(vbicq_u32(vreinterpretq_u32_f32(sSampleCounter[0]), ((uint32x4_t *)maskRestart)[0]));
  sSampleCounter[1] = vreinterpretq_f32_u32(vbicq_u32(vreinterpretq_u32_f32(sSampleCounter[1]), ((uint32x4_t *)maskRestart)[1]));
  ((uint32x4_t *)maskRestart)[0] = vdupq_n_u32(0);
  ((uint32x4_t *)maskRestart)[1] = vdupq_n_u32(0);

  for (; out_p != out_e; in_p += 2, out_p += 2) {
    uint32x4_t maskSampleCounterOverflow[TRACK_COUNT >> 2];
    uint32x4_t vSampleCounter[TRACK_COUNT >> 2];
    uint32x4_t vSampleChunkIndex[TRACK_COUNT >> 2];
    uint32x4_t vSampleChunkOffset[TRACK_COUNT >> 2];
    float32x2_t * vSampleOffset[TRACK_COUNT];
    float32x4_t vSampleData[TRACK_COUNT >> 1];

    maskSampleCounterOverflow[0] = vcgeq_f32(sSampleCounter[0], sSampleSize[0]);
    maskSampleCounterOverflow[1] = vcgeq_f32(sSampleCounter[1], sSampleSize[1]);
    sSampleCounter[0] = vbslq_f32(maskSampleCounterOverflow[0], sSampleCounter[0] - sSampleSize[0], sSampleCounter[0]);
    sSampleCounter[1] = vbslq_f32(maskSampleCounterOverflow[1], sSampleCounter[1] - sSampleSize[1], sSampleCounter[1]);

    vSampleCounter[0] = vcvtq_u32_f32(sSampleCounter[0]);
    vSampleCounter[1] = vcvtq_u32_f32(sSampleCounter[1]);
    vSampleChunkIndex[0] = vSampleCounter[0] >> CHUNK_SIZE_EXP;
    vSampleChunkIndex[1] = vSampleCounter[1] >> CHUNK_SIZE_EXP;
    vSampleChunkOffset[0] = vSampleCounter[0] & CHNUK_SIZE_MASK;
    vSampleChunkOffset[1] = vSampleCounter[1] & CHNUK_SIZE_MASK;

    vSampleOffset[0] = &((float32x2_t *)chunks[0][((uint32_t *)vSampleChunkIndex)[0]])[((uint32_t *)vSampleChunkOffset)[0]];
    vSampleOffset[1] = &((float32x2_t *)chunks[1][((uint32_t *)vSampleChunkIndex)[1]])[((uint32_t *)vSampleChunkOffset)[1]];
    vSampleOffset[2] = &((float32x2_t *)chunks[2][((uint32_t *)vSampleChunkIndex)[2]])[((uint32_t *)vSampleChunkOffset)[2]];
    vSampleOffset[3] = &((float32x2_t *)chunks[3][((uint32_t *)vSampleChunkIndex)[3]])[((uint32_t *)vSampleChunkOffset)[3]];
    vSampleOffset[4] = &((float32x2_t *)chunks[4][((uint32_t *)vSampleChunkIndex)[4]])[((uint32_t *)vSampleChunkOffset)[4]];
    vSampleOffset[5] = &((float32x2_t *)chunks[5][((uint32_t *)vSampleChunkIndex)[5]])[((uint32_t *)vSampleChunkOffset)[5]];
    vSampleOffset[6] = &((float32x2_t *)chunks[6][((uint32_t *)vSampleChunkIndex)[6]])[((uint32_t *)vSampleChunkOffset)[6]];
    vSampleOffset[7] = &((float32x2_t *)chunks[7][((uint32_t *)vSampleChunkIndex)[7]])[((uint32_t *)vSampleChunkOffset)[7]];

    vSampleData[0] = vcombine_f32(*vSampleOffset[0], *vSampleOffset[1]);
    vSampleData[1] = vcombine_f32(*vSampleOffset[2], *vSampleOffset[3]);
    vSampleData[2] = vcombine_f32(*vSampleOffset[4], *vSampleOffset[5]);
    vSampleData[3] = vcombine_f32(*vSampleOffset[6], *vSampleOffset[7]);

    float32x4_t vOut1 = vreinterpretq_f32_u32(vbicq_u32(vreinterpretq_u32_f32(vSampleData[0]), ((uint32x4_t *)maskMute)[0]));
    float32x4_t vOut2 = vreinterpretq_f32_u32(vbicq_u32(vreinterpretq_u32_f32(vSampleData[1]), ((uint32x4_t *)maskMute)[1]));
    float32x4_t vOut3 = vreinterpretq_f32_u32(vbicq_u32(vreinterpretq_u32_f32(vSampleData[2]), ((uint32x4_t *)maskMute)[2]));
    float32x4_t vOut4 = vreinterpretq_f32_u32(vbicq_u32(vreinterpretq_u32_f32(vSampleData[3]), ((uint32x4_t *)maskMute)[3]));

    float32x4_t vOut = vOut1 + vOut2 + vOut3 + vOut4;

    float32x2_t vIn = *(float32x2_t *)in_p;
    vst1_f32(out_p, vIn + vget_low_f32(vOut) + vget_high_f32(vOut));

    float32x4_t vIn2x = vcombine_f32(vIn, vIn);
    float32x4_t vRecord[TRACK_COUNT >> 1];
    vRecord[0] = vbslq_f32(((uint32x4_t *)maskRecord)[0], vIn2x, vSampleData[0]);
    vRecord[1] = vbslq_f32(((uint32x4_t *)maskRecord)[1], vIn2x, vSampleData[1]);
    vRecord[2] = vbslq_f32(((uint32x4_t *)maskRecord)[2], vIn2x, vSampleData[2]);
    vRecord[3] = vbslq_f32(((uint32x4_t *)maskRecord)[3], vIn2x, vSampleData[3]);

    *vSampleOffset[0] = vget_low_f32(vRecord[0]);
    *vSampleOffset[1] = vget_high_f32(vRecord[0]);
    *vSampleOffset[2] = vget_low_f32(vRecord[1]);
    *vSampleOffset[3] = vget_high_f32(vRecord[1]);
    *vSampleOffset[4] = vget_low_f32(vRecord[2]);
    *vSampleOffset[5] = vget_high_f32(vRecord[2]);
    *vSampleOffset[6] = vget_low_f32(vRecord[3]);
    *vSampleOffset[7] = vget_high_f32(vRecord[3]);

    for (uint32_t i = 0; i < sSampleRepeat; i++) {
      sSampleCounter[0] += vdupq_n_f32(1.f);
      sSampleCounter[1] += vdupq_n_f32(1.f);

      maskSampleCounterOverflow[0] = vcgeq_f32(sSampleCounter[0], sSampleSize[0]);
      maskSampleCounterOverflow[1] = vcgeq_f32(sSampleCounter[1], sSampleSize[1]);
      sSampleCounter[0] = vbslq_f32(maskSampleCounterOverflow[0], sSampleCounter[0] - sSampleSize[0], sSampleCounter[0]);
      sSampleCounter[1] = vbslq_f32(maskSampleCounterOverflow[1], sSampleCounter[1] - sSampleSize[1], sSampleCounter[1]);

      vSampleCounter[0] = vcvtq_u32_f32(sSampleCounter[0]);
      vSampleCounter[1] = vcvtq_u32_f32(sSampleCounter[1]);
      vSampleChunkIndex[0] = vSampleCounter[0] >> CHUNK_SIZE_EXP;
      vSampleChunkIndex[1] = vSampleCounter[1] >> CHUNK_SIZE_EXP;
      vSampleChunkOffset[0] = vSampleCounter[0] & CHNUK_SIZE_MASK;
      vSampleChunkOffset[1] = vSampleCounter[1] & CHNUK_SIZE_MASK;

      vSampleOffset[0] = &((float32x2_t *)chunks[0][((uint32_t *)vSampleChunkIndex)[0]])[((uint32_t *)vSampleChunkOffset)[0]];
      vSampleOffset[1] = &((float32x2_t *)chunks[1][((uint32_t *)vSampleChunkIndex)[1]])[((uint32_t *)vSampleChunkOffset)[1]];
      vSampleOffset[2] = &((float32x2_t *)chunks[2][((uint32_t *)vSampleChunkIndex)[2]])[((uint32_t *)vSampleChunkOffset)[2]];
      vSampleOffset[3] = &((float32x2_t *)chunks[3][((uint32_t *)vSampleChunkIndex)[3]])[((uint32_t *)vSampleChunkOffset)[3]];
      vSampleOffset[4] = &((float32x2_t *)chunks[4][((uint32_t *)vSampleChunkIndex)[4]])[((uint32_t *)vSampleChunkOffset)[4]];
      vSampleOffset[5] = &((float32x2_t *)chunks[5][((uint32_t *)vSampleChunkIndex)[5]])[((uint32_t *)vSampleChunkOffset)[5]];
      vSampleOffset[6] = &((float32x2_t *)chunks[6][((uint32_t *)vSampleChunkIndex)[6]])[((uint32_t *)vSampleChunkOffset)[6]];
      vSampleOffset[7] = &((float32x2_t *)chunks[7][((uint32_t *)vSampleChunkIndex)[7]])[((uint32_t *)vSampleChunkOffset)[7]];

      *vSampleOffset[0] = vget_low_f32(vRecord[0]);
      *vSampleOffset[1] = vget_high_f32(vRecord[0]);
      *vSampleOffset[2] = vget_low_f32(vRecord[1]);
      *vSampleOffset[3] = vget_high_f32(vRecord[1]);
      *vSampleOffset[4] = vget_low_f32(vRecord[2]);
      *vSampleOffset[5] = vget_high_f32(vRecord[2]);
      *vSampleOffset[6] = vget_low_f32(vRecord[3]);
      *vSampleOffset[7] = vget_high_f32(vRecord[3]);
    }

    sSampleCounter[0] += sSampleCounterIncrement;
    sSampleCounter[1] += sSampleCounterIncrement;
  }
}

__unit_callback void unit_set_param_value(uint8_t id, int32_t value) {
  value = (int16_t)value;
  uint32_t track;
  if (id <= param_play_8) {
    track = id - param_play_1;
    maskMute[track][0] = value ? 0 : -1;
    maskMute[track][1] = maskMute[track][0];
  } else if (id <= param_record_8) {
    track = id - param_record_1;
    maskRecord[track][0] = value ? -1 : 0;
    maskRecord[track][1] = maskRecord[track][0];
    maskRestart[track] = maskRecord[track][0] & maskMute[track][0];
  } else {
    track = id - param_length_1;
    chunkOK[track] = allocateChunks(track, 1 << value);
  }
  sParams[id] = value;
}

__unit_callback int32_t unit_get_param_value(uint8_t id) {
  return sParams[id];
}

__unit_callback const char * unit_get_param_str_value(uint8_t id, int32_t value) {
  value = (int16_t)value;
  static char s[UNIT_PARAM_NAME_LEN + 1];
  uint32_t err = chunkOK[id - param_length_1] ? 0 : 2;
  uint32_t frac = 0;
  value -= 4;
  if (value < 0) {
    frac = 2;
    value = -value;
  }
  sprintf(s, "%.*s%.*s%d", err, "< ", frac, "1/", 1 << value);
  return s;
}

__unit_callback const uint8_t * unit_get_param_bmp_value(uint8_t id, int32_t value) {
  return &glyphs_bits[(((id >> 3) << 1) + value) << 5];
}

__unit_callback void unit_set_tempo(uint32_t tempo) {
  float fTempo = uq16_16_to_f32(tempo) * MIN_TEMPO_RECIP;
  if (fTempo < 1.f)
    fTempo = 1.f;
  sSampleRepeat = ((uint32_t)fTempo) - 1;
  sSampleCounterIncrement = vdupq_n_f32(fTempo - sSampleRepeat);
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
