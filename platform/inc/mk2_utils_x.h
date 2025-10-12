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
#if UNIT_TIMBRE_COUNT == 2
  float depth[UNIT_TIMBRE_COUNT][kNumModDest];
  float data[kNumModDest * kMk2MaxVoices];
#else
//ToDo: TBC buffer geometry dependency from timbre count and voiceLimit
  #pragma GCC error "Unsupported timbre count"
#endif
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
