/*
 * File: wavetable.h
 *
 * Sample bank handling routines for logue SDK 2.x.
 *
 * 2023 (c) Oleg Burdaev
 * mailto: dukesrg@gmail.com
 *
 */

#pragma once

#include <cstdint>
#include "attributes.h"

#if UNIT_TARGET_PALFORM == (k_unit_target_drumlogue)
#ifndef MAX_SAMPLES_LOAD
#define MAX_SAMPLES_LOAD 256
#endif
static const char SAMPLE_BANK_NAMES[][5] = {
    "CH",
    "OH",
    "RS",
    "CP",
    "MISC",
    "USER",
    "EXP"};
#endif

static const float *sin_wave_ptr = (float *)(const uint32_t[]){
    0x00000000, 0x3CC90AB0, 0x3D48FB30, 0x3D96A905, 0x3DC8BD36, 0x3DFAB273, 0x3E164083, 0x3E2F10A2,
    0x3E47C5C2, 0x3E605C13, 0x3E78CFCC, 0x3E888E93, 0x3E94A031, 0x3EA09AE5, 0x3EAC7CD4, 0x3EB8442A,
    0x3EC3EF15, 0x3ECF7BCA, 0x3EDAE880, 0x3EE63375, 0x3EF15AEA, 0x3EFC5D27, 0x3F039C3D, 0x3F08F59B,
    0x3F0E39DA, 0x3F13682A, 0x3F187FC0, 0x3F1D7FD1, 0x3F226799, 0x3F273656, 0x3F2BEB4A, 0x3F3085BB,
    0x3F3504F3, 0x3F396842, 0x3F3DAEF9, 0x3F41D870, 0x3F45E403, 0x3F49D112, 0x3F4D9F02, 0x3F514D3D,
    0x3F54DB31, 0x3F584853, 0x3F5B941A, 0x3F5EBE05, 0x3F61C598, 0x3F64AA59, 0x3F676BD8, 0x3F6A09A7,
    0x3F6C835E, 0x3F6ED89E, 0x3F710908, 0x3F731447, 0x3F74FA0B, 0x3F76BA07, 0x3F7853F8, 0x3F79C79D,
    0x3F7B14BE, 0x3F7C3B28, 0x3F7D3AAC, 0x3F7E1324, 0x3F7EC46D, 0x3F7F4E6D, 0x3F7FB10F, 0x3F7FEC43,
    0x3F800000, 0x3F7FEC43, 0x3F7FB10F, 0x3F7F4E6D, 0x3F7EC46D, 0x3F7E1324, 0x3F7D3AAC, 0x3F7C3B28,
    0x3F7B14BE, 0x3F79C79D, 0x3F7853F8, 0x3F76BA07, 0x3F74FA0B, 0x3F731447, 0x3F710908, 0x3F6ED89E,
    0x3F6C835E, 0x3F6A09A7, 0x3F676BD8, 0x3F64AA59, 0x3F61C598, 0x3F5EBE05, 0x3F5B941A, 0x3F584853,
    0x3F54DB31, 0x3F514D3D, 0x3F4D9F02, 0x3F49D112, 0x3F45E403, 0x3F41D870, 0x3F3DAEF9, 0x3F396842,
    0x3F3504F3, 0x3F3085BB, 0x3F2BEB4A, 0x3F273656, 0x3F226799, 0x3F1D7FD1, 0x3F187FC0, 0x3F13682A,
    0x3F0E39DA, 0x3F08F59B, 0x3F039C3D, 0x3EFC5D27, 0x3EF15AEA, 0x3EE63375, 0x3EDAE880, 0x3ECF7BCA,
    0x3EC3EF15, 0x3EB8442A, 0x3EAC7CD4, 0x3EA09AE5, 0x3E94A031, 0x3E888E93, 0x3E78CFCC, 0x3E605C13,
    0x3E47C5C2, 0x3E2F10A2, 0x3E164083, 0x3DFAB273, 0x3DC8BD36, 0x3D96A905, 0x3D48FB30, 0x3CC90AB0,
    0x250D3132, 0xBCC90AB0, 0xBD48FB30, 0xBD96A905, 0xBDC8BD36, 0xBDFAB273, 0xBE164083, 0xBE2F10A2,
    0xBE47C5C2, 0xBE605C13, 0xBE78CFCC, 0xBE888E93, 0xBE94A031, 0xBEA09AE5, 0xBEAC7CD4, 0xBEB8442A,
    0xBEC3EF15, 0xBECF7BCA, 0xBEDAE880, 0xBEE63375, 0xBEF15AEA, 0xBEFC5D27, 0xBF039C3D, 0xBF08F59B,
    0xBF0E39DA, 0xBF13682A, 0xBF187FC0, 0xBF1D7FD1, 0xBF226799, 0xBF273656, 0xBF2BEB4A, 0xBF3085BB,
    0xBF3504F3, 0xBF396842, 0xBF3DAEF9, 0xBF41D870, 0xBF45E403, 0xBF49D112, 0xBF4D9F02, 0xBF514D3D,
    0xBF54DB31, 0xBF584853, 0xBF5B941A, 0xBF5EBE05, 0xBF61C598, 0xBF64AA59, 0xBF676BD8, 0xBF6A09A7,
    0xBF6C835E, 0xBF6ED89E, 0xBF710908, 0xBF731447, 0xBF74FA0B, 0xBF76BA07, 0xBF7853F8, 0xBF79C79D,
    0xBF7B14BE, 0xBF7C3B28, 0xBF7D3AAC, 0xBF7E1324, 0xBF7EC46D, 0xBF7F4E6D, 0xBF7FB10F, 0xBF7FEC43,
    0xBF800000, 0xBF7FEC43, 0xBF7FB10F, 0xBF7F4E6D, 0xBF7EC46D, 0xBF7E1324, 0xBF7D3AAC, 0xBF7C3B28,
    0xBF7B14BE, 0xBF79C79D, 0xBF7853F8, 0xBF76BA07, 0xBF74FA0B, 0xBF731447, 0xBF710908, 0xBF6ED89E,
    0xBF6C835E, 0xBF6A09A7, 0xBF676BD8, 0xBF64AA59, 0xBF61C598, 0xBF5EBE05, 0xBF5B941A, 0xBF584853,
    0xBF54DB31, 0xBF514D3D, 0xBF4D9F02, 0xBF49D112, 0xBF45E403, 0xBF41D870, 0xBF3DAEF9, 0xBF396842,
    0xBF3504F3, 0xBF3085BB, 0xBF2BEB4A, 0xBF273656, 0xBF226799, 0xBF1D7FD1, 0xBF187FC0, 0xBF13682A,
    0xBF0E39DA, 0xBF08F59B, 0xBF039C3D, 0xBEFC5D27, 0xBEF15AEA, 0xBEE63375, 0xBEDAE880, 0xBECF7BCA,
    0xBEC3EF15, 0xBEB8442A, 0xBEAC7CD4, 0xBEA09AE5, 0xBE94A031, 0xBE888E93, 0xBE78CFCC, 0xBE605C13,
    0xBE47C5C2, 0xBE2F10A2, 0xBE164083, 0xBDFAB273, 0xBDC8BD36, 0xBD96A905, 0xBD48FB30, 0xBCC90AB0};

struct wave_t
{
  const float *sample_ptr;
  float size;
  uint32_t size_mask;
  uint32_t size_exp;
};

struct wavetable_t
{
  struct
  {
    const float *sample_ptr;
    char name[6];
    uint32_t count;
    uint32_t size_exp;
  } wavetables[MAX_SAMPLES_LOAD] = {sin_wave_ptr, "Sine", 1, 8};

  fast_inline void init(const unit_runtime_desc_t *desc, uint32_t prefix)
  {
    uint32_t bankcount = desc->get_num_sample_banks();
    uint32_t wavetable_idx = 0;
    uint32_t wavesize, namelength;
    char *lastspace;
    for (uint32_t bank_idx = 0; bank_idx < bankcount; bank_idx++)
    {
      uint32_t samplecount = desc->get_num_samples_for_bank(bank_idx);
      for (uint32_t sample_idx = 0; sample_idx < samplecount; sample_idx++)
      {
        const sample_wrapper_t *sample = desc->get_sample(bank_idx, sample_idx);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
        if (sample == nullptr || sample->sample_ptr == nullptr || sample->channels > 1 || *(uint32_t *)sample->name != unit_header.unit_id)
#pragma GCC diagnostic pop
          continue;
        if (((uint32_t *)sample->name)[1] == prefix)
          wavetables[wavetable_idx].sample_ptr = sample->sample_ptr;
        wavesize = 256;
        if (strlen(sample->name) >= 10 && sscanf((char *)&sample->name[9], " %d", &wavesize) != 1)
        {
          if ((lastspace = strrchr((char *)&sample->name[9], ' ')) == nullptr || sscanf(lastspace, " %d", &wavesize) != 1)
          {
            strncpy(wavetables[wavetable_idx].name, (char *)&sample->name[9], sizeof(wavetables[wavetable_idx].name) - 1);
          }
          else
          {
            namelength = strlen((char *)&sample->name[9]) - strlen(lastspace);
            if (namelength > (sizeof(wavetables[wavetable_idx].name) - 1))
              namelength = (sizeof(wavetables[wavetable_idx].name) - 1);
            strncpy(wavetables[wavetable_idx].name, (char *)&sample->name[9], namelength);
          }
        }
        else
        {
          sprintf(wavetables[wavetable_idx].name, "%.1s.%3d", SAMPLE_BANK_NAMES[bank_idx], sample_idx + 1);
        }
        wavetables[wavetable_idx].size_exp = fasterlog2(wavesize);
        wavetables[wavetable_idx].count = sample->frames * sample->channels >> wavetables[wavetable_idx].size_exp;
        wavetable_idx++;
      }
    }
  }

  fast_inline const char *getName(uint32_t index)
  {
    static char name[UNIT_SAMPLE_WRAPPER_MAX_NAME_LEN + 1] = {0};
    uint32_t wavetable_idx = 0;
    while (wavetable_idx < MAX_SAMPLES_LOAD && index >= wavetables[wavetable_idx].count && wavetables[wavetable_idx].count > 0)
      index -= wavetables[wavetable_idx++].count;
    if (wavetable_idx >= MAX_SAMPLES_LOAD || wavetables[wavetable_idx].count <= 0)
      return nullptr;
    if (wavetables[wavetable_idx].count > 1)
    {
      sprintf(name, "%s.%d", wavetables[wavetable_idx].name, index + 1);
    }
    else
    {
      sprintf(name, "%s", wavetables[wavetable_idx].name);
    }
    return name;
  }

  fast_inline wave_t getWave(uint32_t index)
  {
    uint32_t wavetable_idx = 0;
    static wave_t wave;
    while (wavetable_idx < MAX_SAMPLES_LOAD && index >= wavetables[wavetable_idx].count && wavetables[wavetable_idx].count > 0)
      index -= wavetables[wavetable_idx++].count;
    if (wavetable_idx < MAX_SAMPLES_LOAD && wavetables[wavetable_idx].count > 0)
    {
      wave.size_exp = wavetables[wavetable_idx].size_exp;
      wave.sample_ptr = &wavetables[wavetable_idx].sample_ptr[index << wave.size_exp];
    }
    else
    {
      wave.size_exp = 8;
      wave.sample_ptr = sin_wave_ptr;
    }
    wave.size = 1 << wave.size_exp;
    wave.size_mask = wave.size - 1;
    return wave;
  }
};
