/*
 *  File: mk2_utils_x.h
 *
 *  logue SDK 2.x microKORG2 exclusive utilities extension 
 *
 *  2025 (c) Oleg Burdaev
 *  mailto: dukesrg@gmail.com
 */

#pragma once

#include <arm_neon.h>

#include "attributes.h"
#include "runtime.h"

#pragma pack(push, 1)
typedef struct mk2_mod_data {
  uint32_t index[kNumMk2ModSrc];
  float depth[kNumMk2ModSrc];
  float data[kNumMk2ModSrc * kMk2MaxVoices];
} mk2_mod_data_t;

typedef struct mk2_mod_dest_name {
  uint8_t index;
  char name[UNIT_PARAM_NAME_LEN + 1]; //Param name length should be enough
} mk2_mod_dest_name_t;

/*ToDo: TBC message purpose and data format
typedef struct mk2_user_mod_source {
} mk2_user_mod_source_t;
*/
#pragma pack(pop)
