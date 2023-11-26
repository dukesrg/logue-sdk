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
#define LENGTH_STEPS_MAX 16
#define LENGTH_STEPS_DEFAULT 7
#define MIN_TEMPO 56.f
#define MIN_TEMPO_RECIP .017857143f 
#define SIZE_SCALE 3214.2857f //60 * 48000 / (16 * MIN_TEMPO)
#define CHUNK_SIZE_EXP 17
#define CHUNK_SIZE (1 << CHUNK_SIZE_EXP)
#define CHNUK_SIZE_MASK (CHUNK_SIZE - 1)
#define CHUNK_COUNT (uint32_t)(SIZE_SCALE * (1 << LENGTH_STEPS_MAX) / CHUNK_SIZE + 1)

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
static float32x2_t * chunks[TRACK_COUNT][CHUNK_COUNT];
static bool isChunksAllocated[TRACK_COUNT];
static uint32x4_t maskMute[TRACK_COUNT >> 1];
static uint32_t maskRecord[TRACK_COUNT];
static uint32_t maskRestart[TRACK_COUNT];
static uint32_t maskDup[TRACK_COUNT];
static float32x4_t sSampleSize[TRACK_COUNT >> 2];
static float32x4_t sSampleDupSize[TRACK_COUNT >> 2];
static float32x4_t sSampleDupSizeRecip[TRACK_COUNT >> 2];
static float32x4_t sSampleCounter[TRACK_COUNT >> 2];
static float32x4_t sSampleCounterIncrement;
static uint32_t sWriteBackSamples;
static float32x2_t sWriteBackSamplesRecip;

fast_inline void allocateChunks(uint32_t track, uint32_t newsize) {
  static uint32_t allocated[TRACK_COUNT];
  uint32_t count = newsize == 0 ? 0 : (newsize / CHUNK_SIZE + 1);
  if (newsize < ((float *)sSampleSize)[track]) {
    ((float *)sSampleSize)[track] = newsize;  
    if (newsize <= ((float *)sSampleDupSize)[track]) {
      maskDup[track] = 0;
      ((float *)sSampleDupSize)[track] = newsize;
    }
    for (uint32_t i = count; i < allocated[track]; free(chunks[track][i++]));
  } else if (newsize > ((float *)sSampleSize)[track]) {
    for (uint32_t i = allocated[track]; i < count; i++) {
      chunks[track][i] = (float32x2_t *)malloc(CHUNK_SIZE * sizeof(float32x2_t));
      if (chunks[track][i] == NULL) {
        for (;i > allocated[track]; free(chunks[track][--i]));
        isChunksAllocated[track] = false;
        return;
      }
    }
    if (maskDup[track] == 0) {
      maskDup[track] = -1;
      ((float *)sSampleDupSize)[track] = ((float *)sSampleSize)[track];
      ((float *)sSampleDupSizeRecip)[track] = 1.f / ((float *)sSampleDupSize)[track]; 
    }
    ((float *)sSampleSize)[track] = newsize;
  }
  allocated[track] = count;
  isChunksAllocated[track] = true;
}

fast_inline void calcOffsets(float32x2_t ** sampleOffset, float32x4_t * sampleCounter) {
    uint32x4_t maskSampleCounterOverflow[TRACK_COUNT >> 2];
    uint32x4_t vSampleCounter[TRACK_COUNT >> 2];
    uint32_t vSampleChunkIndex[TRACK_COUNT];
    uint32_t vSampleChunkOffset[TRACK_COUNT];
    float32x4_t vSampleDupCounter[TRACK_COUNT >> 2];

    maskSampleCounterOverflow[0] = vcgeq_f32(sampleCounter[0], sSampleSize[0]);
    maskSampleCounterOverflow[1] = vcgeq_f32(sampleCounter[1], sSampleSize[1]);
    sampleCounter[0] = vbslq_f32(maskSampleCounterOverflow[0], sampleCounter[0] - sSampleSize[0], sSampleCounter[0]);
    sampleCounter[1] = vbslq_f32(maskSampleCounterOverflow[1], sampleCounter[1] - sSampleSize[1], sSampleCounter[1]);
    ((uint32x4_t*)maskDup)[0] = vbicq_u32(((uint32x4_t*)maskDup)[0], maskSampleCounterOverflow[0]);
    ((uint32x4_t*)maskDup)[1] = vbicq_u32(((uint32x4_t*)maskDup)[1], maskSampleCounterOverflow[1]);
    vSampleDupCounter[0] = sampleCounter[0] * sSampleDupSizeRecip[0];
    vSampleDupCounter[1] = sampleCounter[1] * sSampleDupSizeRecip[1];
    vSampleDupCounter[0] -= vcvtq_f32_u32(vcvtq_u32_f32(vSampleDupCounter[0]));
    vSampleDupCounter[1] -= vcvtq_f32_u32(vcvtq_u32_f32(vSampleDupCounter[1]));
    vSampleDupCounter[0] *= sSampleDupSize[0];
    vSampleDupCounter[1] *= sSampleDupSize[1];
    vSampleCounter[0] = vcvtq_u32_f32(vbslq_f32(((uint32x4_t*)maskDup)[0], vSampleDupCounter[0], sampleCounter[0]));
    vSampleCounter[1] = vcvtq_u32_f32(vbslq_f32(((uint32x4_t*)maskDup)[1], vSampleDupCounter[1], sampleCounter[1]));
    ((uint32x4_t*)vSampleChunkIndex)[0] = vSampleCounter[0] >> CHUNK_SIZE_EXP;
    ((uint32x4_t*)vSampleChunkIndex)[1] = vSampleCounter[1] >> CHUNK_SIZE_EXP;
    ((uint32x4_t*)vSampleChunkOffset)[0] = vSampleCounter[0] & CHNUK_SIZE_MASK;
    ((uint32x4_t*)vSampleChunkOffset)[1] = vSampleCounter[1] & CHNUK_SIZE_MASK;

    sampleOffset[0] = &(chunks[0][vSampleChunkIndex[0]])[vSampleChunkOffset[0]];
    sampleOffset[1] = &(chunks[1][vSampleChunkIndex[1]])[vSampleChunkOffset[1]];
    sampleOffset[2] = &(chunks[2][vSampleChunkIndex[2]])[vSampleChunkOffset[2]];
    sampleOffset[3] = &(chunks[3][vSampleChunkIndex[3]])[vSampleChunkOffset[3]];
    sampleOffset[4] = &(chunks[4][vSampleChunkIndex[4]])[vSampleChunkOffset[4]];
    sampleOffset[5] = &(chunks[5][vSampleChunkIndex[5]])[vSampleChunkOffset[5]];
    sampleOffset[6] = &(chunks[6][vSampleChunkIndex[6]])[vSampleChunkOffset[6]];
    sampleOffset[7] = &(chunks[7][vSampleChunkIndex[7]])[vSampleChunkOffset[7]];
}

fast_inline void calcWriteOffsets(float32x2_t ** sampleOffset, float32x4_t * sampleCounter) {
    uint32x4_t maskSampleCounterOverflow[TRACK_COUNT >> 2];
    uint32x4_t vSampleCounter[TRACK_COUNT >> 2];
    uint32_t vSampleChunkIndex[TRACK_COUNT];
    uint32_t vSampleChunkOffset[TRACK_COUNT];

    maskSampleCounterOverflow[0] = vcgeq_f32(sampleCounter[0], sSampleSize[0]);
    maskSampleCounterOverflow[1] = vcgeq_f32(sampleCounter[1], sSampleSize[1]);
    sampleCounter[0] = vbslq_f32(maskSampleCounterOverflow[0], sampleCounter[0] - sSampleSize[0], sSampleCounter[0]);
    sampleCounter[1] = vbslq_f32(maskSampleCounterOverflow[1], sampleCounter[1] - sSampleSize[1], sSampleCounter[1]);

    vSampleCounter[0] = vcvtq_u32_f32(sampleCounter[0]);
    vSampleCounter[1] = vcvtq_u32_f32(sampleCounter[1]);
    ((uint32x4_t*)vSampleChunkIndex)[0] = vSampleCounter[0] >> CHUNK_SIZE_EXP;
    ((uint32x4_t*)vSampleChunkIndex)[1] = vSampleCounter[1] >> CHUNK_SIZE_EXP;
    ((uint32x4_t*)vSampleChunkOffset)[0] = vSampleCounter[0] & CHNUK_SIZE_MASK;
    ((uint32x4_t*)vSampleChunkOffset)[1] = vSampleCounter[1] & CHNUK_SIZE_MASK;

    sampleOffset[0] = &(chunks[0][vSampleChunkIndex[0]])[vSampleChunkOffset[0]];
    sampleOffset[1] = &(chunks[1][vSampleChunkIndex[1]])[vSampleChunkOffset[1]];
    sampleOffset[2] = &(chunks[2][vSampleChunkIndex[2]])[vSampleChunkOffset[2]];
    sampleOffset[3] = &(chunks[3][vSampleChunkIndex[3]])[vSampleChunkOffset[3]];
    sampleOffset[4] = &(chunks[4][vSampleChunkIndex[4]])[vSampleChunkOffset[4]];
    sampleOffset[5] = &(chunks[5][vSampleChunkIndex[5]])[vSampleChunkOffset[5]];
    sampleOffset[6] = &(chunks[6][vSampleChunkIndex[6]])[vSampleChunkOffset[6]];
    sampleOffset[7] = &(chunks[7][vSampleChunkIndex[7]])[vSampleChunkOffset[7]];
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

  for (uint32_t i = 0; i < TRACK_COUNT; allocateChunks(i++, SIZE_SCALE * (1 << LENGTH_STEPS_DEFAULT)));

  return k_unit_err_none;
}

__unit_callback void unit_teardown() {
  for (uint32_t i = 0; i < TRACK_COUNT; allocateChunks(i++, 0));
}

__unit_callback void unit_reset() {
  sSampleCounter[0] = vdupq_n_f32(0.f);
  sSampleCounter[1] = vdupq_n_f32(0.f);
}

__unit_callback void unit_resume() {
}

__unit_callback void unit_suspend() {
}

__unit_callback void unit_render(const float * in, float * out, uint32_t frames) {
  const float * __restrict in_p = in;
  float * __restrict out_p = out;
  const float * out_e = out_p + (frames << 1);
  static float32x2_t vInOld;
  
  sSampleCounter[0] = vbicq_f32(sSampleCounter[0], ((uint32x4_t *)maskRestart)[0]);
  sSampleCounter[1] = vbicq_f32(sSampleCounter[1], ((uint32x4_t *)maskRestart)[1]);
  ((uint32x4_t *)maskRestart)[0] = vdupq_n_u32(0);
  ((uint32x4_t *)maskRestart)[1] = vdupq_n_u32(0);

  for (; out_p != out_e; in_p += 2, out_p += 2) {
    float32x2_t * vSampleOffset[TRACK_COUNT];
    float32x2_t * vSampleWriteOffset[TRACK_COUNT];
    float32x4_t vSampleData[TRACK_COUNT >> 1];
    float32x4_t vSampleCounterWrite[TRACK_COUNT >> 2];
    float32x4_t vOut;
    float32x2_t vIn = *(float32x2_t *)in_p;
    float32x2_t vInIncrement;

    calcOffsets(vSampleOffset, sSampleCounter);
    vSampleCounterWrite[0] = sSampleCounter[0];
    vSampleCounterWrite[1] = sSampleCounter[1];

    float32x4_t vSampleData0[TRACK_COUNT >> 1];
    float32x4_t vSampleData1[TRACK_COUNT >> 1];
    float32x4_t vSampleFrac[TRACK_COUNT >> 1];

    vSampleData0[0] = vcombine_f32(*vSampleOffset[0], *vSampleOffset[1]);
    vSampleData0[1] = vcombine_f32(*vSampleOffset[2], *vSampleOffset[3]);
    vSampleData0[2] = vcombine_f32(*vSampleOffset[4], *vSampleOffset[5]);
    vSampleData0[3] = vcombine_f32(*vSampleOffset[6], *vSampleOffset[7]);

    vSampleFrac[0] = sSampleCounter[0] - vcvtq_f32_u32(vcvtq_u32_f32(sSampleCounter[0]));
    vSampleFrac[1] = sSampleCounter[1] - vcvtq_f32_u32(vcvtq_u32_f32(sSampleCounter[1]));
    float32x4x2_t vSampleFrac0 = vzipq_f32(vSampleFrac[0], vSampleFrac[0]);
    float32x4x2_t vSampleFrac1 = vzipq_f32(vSampleFrac[1], vSampleFrac[1]);

    float32x4_t vSampleCounter[TRACK_COUNT >> 2];
    vSampleCounter[0] = sSampleCounter[0] + vdupq_n_f32(1.f);
    vSampleCounter[1] = sSampleCounter[1] + vdupq_n_f32(1.f);
    calcOffsets(vSampleOffset, vSampleCounter);

    vSampleData1[0] = vcombine_f32(*vSampleOffset[0], *vSampleOffset[1]);
    vSampleData1[1] = vcombine_f32(*vSampleOffset[2], *vSampleOffset[3]);
    vSampleData1[2] = vcombine_f32(*vSampleOffset[4], *vSampleOffset[5]);
    vSampleData1[3] = vcombine_f32(*vSampleOffset[6], *vSampleOffset[7]);

    vSampleData[0] = vSampleData0[0] + (vSampleData1[0] - vSampleData0[0]) * vSampleFrac0.val[0];
    vSampleData[1] = vSampleData0[1] + (vSampleData1[1] - vSampleData0[1]) * vSampleFrac0.val[1];
    vSampleData[2] = vSampleData0[2] + (vSampleData1[2] - vSampleData0[2]) * vSampleFrac1.val[0];
    vSampleData[3] = vSampleData0[3] + (vSampleData1[3] - vSampleData0[3]) * vSampleFrac1.val[1];

    vSampleData[0] = vbicq_f32(vSampleData[0], maskMute[0]);
    vSampleData[1] = vbicq_f32(vSampleData[1], maskMute[1]);
    vSampleData[2] = vbicq_f32(vSampleData[2], maskMute[2]);
    vSampleData[3] = vbicq_f32(vSampleData[3], maskMute[3]);

    vOut = vSampleData[0] + vSampleData[1] + vSampleData[2] + vSampleData[3];

    vst1_f32(out_p, vIn + vget_low_f32(vOut) + vget_high_f32(vOut));

    vInIncrement = (vInOld - vIn) * sWriteBackSamplesRecip;
    vInOld = vIn;

    for (uint32_t i = 0; i < sWriteBackSamples; i++) {
      calcWriteOffsets(vSampleWriteOffset, vSampleCounterWrite);
      calcOffsets(vSampleOffset, vSampleCounterWrite);

      if (maskRecord[0]) *vSampleWriteOffset[0] = vIn;
      else if (maskDup[0]) *vSampleWriteOffset[0] = *vSampleOffset[0];
      if (maskRecord[1]) *vSampleWriteOffset[1] = vIn;
      else if (maskDup[1]) *vSampleWriteOffset[1] = *vSampleOffset[1];
      if (maskRecord[2]) *vSampleWriteOffset[2] = vIn;
      else if (maskDup[2]) *vSampleWriteOffset[2] = *vSampleOffset[2];
      if (maskRecord[3]) *vSampleWriteOffset[3] = vIn;
      else if (maskDup[3]) *vSampleWriteOffset[3] = *vSampleOffset[3];
      if (maskRecord[4]) *vSampleWriteOffset[4] = vIn;
      else if (maskDup[4]) *vSampleWriteOffset[4] = *vSampleOffset[4];
      if (maskRecord[5]) *vSampleWriteOffset[5] = vIn;
      else if (maskDup[5]) *vSampleWriteOffset[5] = *vSampleOffset[5];
      if (maskRecord[6]) *vSampleWriteOffset[6] = vIn;
      else if (maskDup[6]) *vSampleWriteOffset[6] = *vSampleOffset[6];
      if (maskRecord[7]) *vSampleWriteOffset[7] = vIn;
      else if (maskDup[7]) *vSampleWriteOffset[7] = *vSampleOffset[7];

      vSampleCounterWrite[0] -= vdupq_n_f32(1.f);
      vSampleCounterWrite[1] -= vdupq_n_f32(1.f);
      vIn += vInIncrement;
    }

    sSampleCounter[0] += sSampleCounterIncrement;
    sSampleCounter[1] += sSampleCounterIncrement;
  }
}

__unit_callback void unit_set_param_value(uint8_t id, int32_t value) {
  value = (int16_t)value;
  sParams[id] = value;
  if (id <= param_play_8) {
    id -= param_play_1;
    ((uint32x2_t *)maskMute)[id] = vdup_n_u32(value ? 0 : -1);
  } else if (id <= param_record_8) {
    id -= param_record_1;
    maskRecord[id] = value ? -1 : 0;
    maskRestart[id] = maskRecord[id] & ((uint32x2_t *)maskMute)[id][0];
  } else {
    id -= param_length_1;
    allocateChunks(id, SIZE_SCALE * (1 << value));
  }
}

__unit_callback int32_t unit_get_param_value(uint8_t id) {
  return sParams[id];
}

__unit_callback const char * unit_get_param_str_value(uint8_t id, int32_t value) {
  value = (int16_t)value;
  static char s[UNIT_PARAM_NAME_LEN + 1];
  uint32_t err = isChunksAllocated[id - param_length_1] ? 0 : 2;
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
  sSampleCounterIncrement = vdupq_n_f32(fTempo);
  sWriteBackSamples = fTempo;
  if ((fTempo - sWriteBackSamples) > 0.f)
    sWriteBackSamples++;
  if (sWriteBackSamples > 0)
    sWriteBackSamplesRecip = vdup_n_f32(1.f / sWriteBackSamples);
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
