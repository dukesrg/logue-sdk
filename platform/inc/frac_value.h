/*
 *  File: frac_value.h
 *
 *  logue SDK 2.x fractional parameter value display workwaround
 *
 *  2024-2025 (c) Oleg Burdaev
 *  mailto: dukesrg@gmail.com
 */

#pragma once

#include "logue_wrap.h"

#if defined(UNIT_API_VERSION) && defined(UNIT_TARGET_PLATFORM)

#ifdef UNIT_TARGET_PLATFORM_DRUMLOGUE
  #define MAX_CHARS 8
#elif defined(UNIT_TARGET_PLATFORM_NTS1_MKII)
  #define MAX_CHARS 4
#elif defined(UNIT_TARGET_PLATFORM_NTS3_KAOSS)
  #define MAX_CHARS 6
#elif defined(UNIT_TARGET_PLATFORM_MICROKORG2)
  #define MAX_CHARS 8
#endif

fast_inline const char * unit_get_param_frac_value(uint8_t index, int32_t value) {
  static const float decimal_frac[16] = {1.f, 1e-1f, 1e-2f, 1e-3f, 1e-4f, 1e-5f, 1e-6f, 1e-7f, 1e-8f, 1e-9f, 1e-10f, 1e-11f, 1e-12f, 1e-13f, 1e-14f, 1e-15f};
  static char result[MAX_CHARS + 1];
  uint32_t idx = 0;

#ifdef UNIT_GENERICFX_H_
  const unit_param_t *param = unit_header.common.params;
#else
  const unit_param_t *param = unit_header.params;
#endif

  value = (int16_t)value;
  if (value < 0) {
    result[idx++] = '-';
    value = -value;
  }

  static float fval = (param[index].frac_mode == k_unit_param_frac_mode_fixed) ? (value << (16 - param[index].frac)) * .15258789e-4f : value * decimal_frac[param[index].frac];

//TODO: to string conversion
  value = fval;
  result[idx++] = '0' + (value & 0x0F);
//

  if (idx == 0)
    result[idx++] = '0';
  result[idx] = 0;
  return result;
}

#undef MAX_CHARS

#endif
