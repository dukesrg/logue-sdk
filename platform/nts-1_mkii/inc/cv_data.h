/*
 * File: cv_data.h
 *
 * Audio-CV data conversion routines for logueSDK
 *
 * 2024 (c) Oleg Burdaev
 * mailto: dukesrg@gmail.com
 *
 */

#pragma once

#define NAN_MASK (0x7F800000)
#define cv_to_float 1.192093e-7f //2^-23
//#define cv_to_float 1.1920929e-7f //2^-23

typedef union {
    float f;
    int32_t i;
} ieee754_t;

/*
static inline __attribute__((always_inline, optimize("Ofast")))
int32_t ssat_(int32_t op1, int32_t op2)
{
  register int32_t result;
  __asm__ volatile ("ssat %0, %1, %2" : "=r" (result) : "i" (op1), "r" (op2));
  return result;
}
*/

uint32_t isCV (float x) {
    return ((ieee754_t){.f = x}.i & NAN_MASK) == NAN_MASK;
}

float fromCV(float x) {
    int t = (ieee754_t){.f = x}.i;
    return isCV(x) ? ((float)((t & 0x80000000) ? t : (t & ~NAN_MASK)) * cv_to_float  ) : 0.f;
//    return isCV(x) ? ((float)(ssat_(24, (ieee754_t){.f = x}.i)) * cv_to_float  ) : 0.f;
}

float toCV(float x) {
    return isCV(x) ? ((float)((ieee754_t){.f = x}.i & ~NAN_MASK) * cv_to_float) : 0.f;
}
