/*
 * Host (AArch64 / x86) stand-in for ../../inc/arm.h.
 * Same names and ARMv7 semantics, written in portable C so the unit can run
 * under sanitizers on a desktop. Test-only; never part of the device build.
 */
#pragma once

#include <stdint.h>
#include <math.h>
#include <arm_neon.h>

typedef int32_t q31_t;
typedef int64_t q63_t;
typedef uint32_t uq32_t;
typedef uint32_t uq16_16_t;
typedef uint32x2x3_t uq32x2x3_t;
typedef uint32x4x3_t uq32x4x3_t;

// vcvt to fixed point: scale, round toward zero, saturate, NaN -> 0
static inline int32_t host_cvt_s32(double v) {
  if (v != v) return 0;
  if (v >= 2147483647.0) return INT32_MAX;
  if (v <= -2147483648.0) return INT32_MIN;
  return (int32_t)v;
}
static inline uint32_t host_cvt_u32(double v) {
  if (v != v || v <= 0.0) return 0;
  if (v >= 4294967295.0) return UINT32_MAX;
  return (uint32_t)v;
}

fast_inline q31_t f32_to_q31(float f) { return host_cvt_s32((double)f * 2147483648.0); }
fast_inline float q31_to_f32(q31_t q) { return (float)((double)q / 2147483648.0); }
fast_inline uq32_t f32_to_uq32(float f) { return host_cvt_u32((double)f * 4294967296.0); }
fast_inline float uq32_to_f32(uq32_t q) { return (float)((double)q / 4294967296.0); }
fast_inline uq16_16_t f32_to_uq16_16(float f) { return host_cvt_u32((double)f * 65536.0); }
fast_inline float uq16_16_to_f32(uq16_16_t q) { return (float)((double)q / 65536.0); }

#define q31mul(a,b) ((q31_t)(((q63_t)(q31_t)(a) * (q31_t)(b))>>31))

static inline int32_t host_sat32(int64_t v) {
  return v > INT32_MAX ? INT32_MAX : v < INT32_MIN ? INT32_MIN : (int32_t)v;
}
static inline int32_t q31add(int32_t a, int32_t b) { return host_sat32((int64_t)a + b); }
static inline int32_t q31sub(int32_t a, int32_t b) { return host_sat32((int64_t)a - b); }
static inline int32_t smmul(int32_t a, int32_t b) { return (int32_t)(((int64_t)a * b) >> 32); }
static inline int32_t smulwb(int32_t a, int32_t b) { return (int32_t)(((int64_t)a * (int16_t)b) >> 16); }
static inline int32_t smulwt(int32_t a, int32_t b) { return (int32_t)(((int64_t)a * (int16_t)(b >> 16)) >> 16); }
static inline int32_t smlawb(int32_t a, int32_t b, int32_t c) { return smulwb(a, b) + c; }
static inline int32_t smlawt(int32_t a, int32_t b, int32_t c) { return smulwt(a, b) + c; }

// usat #n: clamp signed value to [0, 2^n - 1]
static inline int32_t host_usat(int64_t v, int n) {
  int64_t hi = ((int64_t)1 << n) - 1;
  return (int32_t)(v < 0 ? 0 : v > hi ? hi : v);
}
static inline int32_t usat_asr(int32_t n, int32_t x, int32_t s) { return host_usat((int64_t)x >> s, n); }
static inline int32_t usat_lsl(int32_t n, int32_t x, int32_t s) { return host_usat((int64_t)x << s, n); }

// The device build passes table pointers as int32_t (32-bit ARM). The host
// build rewrites those casts to intptr_t, so loads take a full pointer here.
static inline int32_t ldr_lsl(intptr_t base, int32_t i, int32_t s) { return *(const int32_t *)(base + ((intptr_t)i << s)); }
static inline int32_t ldrh_lsl(intptr_t base, int32_t i, int32_t s) { return *(const uint16_t *)(base + ((intptr_t)i << s)); }
static inline int32_t ldrsh_lsl(intptr_t base, int32_t i, int32_t s) { return *(const int16_t *)(base + ((intptr_t)i << s)); }

static inline int32_t sbfx(int32_t x, int32_t lsb, int32_t w) { return (int32_t)((uint32_t)x << (32 - lsb - w)) >> (32 - w); }
static inline int32_t ubfx(int32_t x, int32_t lsb, int32_t w) { return (int32_t)(((uint32_t)x >> lsb) & (w >= 32 ? 0xFFFFFFFFu : ((1u << w) - 1))); }

fast_inline float32x4x3_t operator+(float32x4x3_t a, const float32x4x3_t& b) {
  a.val[0] += b.val[0]; a.val[1] += b.val[1]; a.val[2] += b.val[2]; return a;
}
fast_inline float32x4x3_t operator-(float32x4x3_t a, const float32x4x3_t& b) {
  a.val[0] -= b.val[0]; a.val[1] -= b.val[1]; a.val[2] -= b.val[2]; return a;
}
fast_inline uq32x4x3_t operator+(uq32x4x3_t a, const uq32x4x3_t& b) {
  a.val[0] += b.val[0]; a.val[1] += b.val[1]; a.val[2] += b.val[2]; return a;
}
fast_inline uq32x4x3_t operator-(uq32x4x3_t a, const uq32x4x3_t& b) {
  a.val[0] -= b.val[0]; a.val[1] -= b.val[1]; a.val[2] -= b.val[2]; return a;
}
fast_inline int32x4x3_t vcvtq_s32_f32(float32x4x3_t a) {
  return {vcvtq_s32_f32(a.val[0]), vcvtq_s32_f32(a.val[1]), vcvtq_s32_f32(a.val[2])};
}
fast_inline float32x4x3_t vcvtq_f32_s32(int32x4x3_t a) {
  return {vcvtq_f32_s32(a.val[0]), vcvtq_f32_s32(a.val[1]), vcvtq_f32_s32(a.val[2])};
}
fast_inline float32x4x3_t vcvtq_n_f32_uq32(uq32x4x3_t a) {
  return {vcvtq_n_f32_u32(a.val[0], 32), vcvtq_n_f32_u32(a.val[1], 32), vcvtq_n_f32_u32(a.val[2], 32)};
}
fast_inline uq32x4x3_t vcvtq_n_f32_uq32(float32x4x3_t a) {
  return {vcvtq_n_u32_f32(a.val[0], 32), vcvtq_n_u32_f32(a.val[1], 32), vcvtq_n_u32_f32(a.val[2], 32)};
}
fast_inline uint32x4_t vcntq_u32(uint32x4_t value) {
  uint8x16_t tmp = vcntq_u8(vreinterpretq_u8_u32(value));
  tmp = vaddq_u8(tmp, vrev16q_u8(tmp));
  tmp = vaddq_u8(tmp, vrev32q_u8(tmp));
  return vshrq_n_u32(vreinterpretq_u32_u8(tmp), 24);
}
fast_inline int32x4_t vcntq_s32(int32x4_t value) {
  int8x16_t tmp = vcntq_s8(vreinterpretq_s8_s32(value));
  tmp = vaddq_s8(tmp, vrev16q_s8(tmp));
  tmp = vaddq_s8(tmp, vrev32q_s8(tmp));
  return vshrq_n_s32(vreinterpretq_s32_s8(tmp), 24);
}
#define vbicq_f32(a,b) vreinterpretq_f32_u32(vbicq_u32(vreinterpretq_u32_f32(a),b))
