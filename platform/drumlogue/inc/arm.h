/*
 * File: arm.h
 *
 * Additional ARM opcodes.
 *
 * 2020-2023 (c) Oleg Burdaev
 * mailto: dukesrg@gmail.com
 *
 */

#pragma once

typedef int32_t q31_t;
typedef uint32_t uq32_t;
typedef uint32_t uq16_16_t;
typedef uint32x2x3_t uq32x2x3_t;
typedef uint32x4x3_t uq32x4x3_t;

//#define f32_to_q31(f)   ((q31_t)((float)(f) * (float)0x7FFFFFFF))
//#define q31_to_f32(q)   ((float)(q) * 4.65661287307739e-10f)
//#define uq16_16_to_f32(q)   ((float)(q) * 2.32830643654e-10f)

fast_inline q31_t f32_to_q31(float op1) {
  register q31_t result; 
  __asm__ volatile ("vcvt.s32.f32 %0, %0, 31" : "=t" (result) : "0" (op1));
  return result;
}

fast_inline float q31_to_f32(q31_t op1) {
  register float result;
  __asm__ volatile ("vcvt.f32.s32 %0, %0, 31" : "=t" (result) : "0" (op1));
  return result;
}

fast_inline uq32_t f32_to_uq32(float op1) {
  register uq32_t result;
  __asm__ volatile ("vcvt.u32.f32 %0, %0, 32" : "=t" (result) : "0" (op1));
  return result;
}

fast_inline float uq32_to_f32(uq32_t op1) {
  register float result;
  __asm__ volatile ("vcvt.f32.u32 %0, %0, 32" : "=t" (result) : "0" (op1));
  return result;
}

fast_inline uq16_16_t f32_to_uq16_16(float op1) {
  register uint32_t result;
  __asm__ volatile ("vcvt.u32.f32 %0, %0, 16" : "=t" (result) : "0" (op1));
  return result;
}

fast_inline float uq16_16_to_f32(uq16_16_t op1) {
  register float result;
  __asm__ volatile ("vcvt.f32.u32 %0, %0, 16" : "=t" (result) : "0" (op1));
  return result;
}

#define q31mul(a,b) ((q31_t)(((q63_t)(q31_t)(a) * (q31_t)(b))>>31))
//#define q31absmul(a,b) (-q31mul(a,-b))
//#define q31abs(a)   (__QSUB((q31_t)(a) ^ ((q31_t)(a)>>31), (q31_t)(a)>>31))

static inline __attribute__((always_inline, optimize("Ofast")))
int32_t q31add(int32_t op1, int32_t op2)
{
  register int32_t result;
  __asm__ volatile ("qadd %0, %1, %2" : "=r" (result) : "r" (op1), "r" (op2) );
  return result;
}

static inline __attribute__((always_inline, optimize("Ofast")))
int32_t q31sub(int32_t op1, int32_t op2)
{
  register int32_t result;
  __asm__ volatile ("qsub %0, %1, %2" : "=r" (result) : "r" (op1), "r" (op2) );
  return result;
}

static inline __attribute__((always_inline, optimize("Ofast")))
int32_t smmul(int32_t op1, int32_t op2)
{
  register int32_t result;
  __asm__ volatile ("smmul %0, %1, %2" : "=r" (result) : "r" (op1), "r" (op2) );
  return result;
}

static inline __attribute__((always_inline, optimize("Ofast")))
int32_t smlawb(int32_t op1, int32_t op2, int32_t op3)
{
  register int32_t result;
  __asm__ volatile ("smlawb %0, %1, %2, %3" : "=r" (result) : "r" (op1), "r" (op2), "r" (op3) );
  return result;
}

static inline __attribute__((always_inline, optimize("Ofast")))
int32_t smlawt(int32_t op1, int32_t op2, int32_t op3)
{
  register int32_t result;
  __asm__ volatile ("smlawt %0, %1, %2, %3" : "=r" (result) : "r" (op1), "r" (op2), "r" (op3) );
  return result;
}

static inline __attribute__((always_inline, optimize("Ofast")))
int32_t smulwb(int32_t op1, int32_t op2)
{
  register int32_t result;
  __asm__ volatile ("smulwb %0, %1, %2" : "=r" (result) : "r" (op1), "r" (op2) );
  return result;
}

static inline __attribute__((always_inline, optimize("Ofast")))
int32_t smulwt(int32_t op1, int32_t op2)
{
  register int32_t result;
  __asm__ volatile ("smulwt %0, %1, %2" : "=r" (result) : "r" (op1), "r" (op2) );
  return result;
}

#define smlal(a,b,c,d) __asm__ volatile ("smlal %0, %1, %2, %3" : "+r" (a), "+r" (b) : "r" (c), "r" (d))
#define tbb(a,b) __asm__ volatile ("tbb [%0, %1]" : : "r" (a), "r" (b))
#define tbb_pc(a) __asm__ volatile ("tbb [pc, %0]" : : "r" (a))
#define tbh(a,b) __asm__ volatile ("tbh [%0, %1, lsl #1]" : : "r" (a), "r" (b))
#define tbh_pc(a) __asm__ volatile ("tbh [pc, %0, lsl #1]" : : "r" (a))

static inline __attribute__((always_inline, optimize("Ofast")))
int32_t usat_asr(int32_t op1, int32_t op2, int32_t op3)
{
  register int32_t result;
  __asm__ volatile ("usat %0, %1, %2, asr %3" : "=r" (result) : "i" (op1), "r" (op2), "i" (op3));
  return result;
}

static inline __attribute__((always_inline, optimize("Ofast")))
int32_t usat_lsl(int32_t op1, int32_t op2, int32_t op3)
{
  register int32_t result;
  __asm__ volatile ("usat %0, %1, %2, lsl %3" : "=r" (result) : "i" (op1), "r" (op2), "i" (op3));
  return result;
}

static inline __attribute__((always_inline, optimize("Ofast")))
int32_t ldr_lsl(int32_t op1, int32_t op2, int32_t op3)
{
  register int32_t result;
  __asm__ volatile ("ldr %0, [%1, %2, lsl %3]" : "=r" (result) : "r" (op1), "r" (op2), "i" (op3));
  return result;
}

static inline __attribute__((always_inline, optimize("Ofast")))
int32_t ldrh_lsl(int32_t op1, int32_t op2, int32_t op3)
{
  register int32_t result;
  __asm__ volatile ("ldrh %0, [%1, %2, lsl %3]" : "=r" (result) : "r" (op1), "r" (op2), "i" (op3));
  return result;
}

static inline __attribute__((always_inline, optimize("Ofast")))
int32_t ldrsh_lsl(int32_t op1, int32_t op2, int32_t op3)
{
  register int32_t result;
  __asm__ volatile ("ldrsh %0, [%1, %2, lsl %3]" : "=r" (result) : "r" (op1), "r" (op2), "i" (op3));
  return result;
}

static inline __attribute__((always_inline, optimize("Ofast")))
int32_t sbfx(int32_t op1, int32_t op2, int32_t op3)
{
  register int32_t result;
  __asm__ volatile ("sbfx %0, %1, %2, %3" : "=r" (result) : "r" (op1), "i" (op2), "i" (op3));
  return result;
}

static inline __attribute__((always_inline, optimize("Ofast")))
int32_t ubfx(int32_t op1, int32_t op2, int32_t op3)
{
  register int32_t result;
  __asm__ volatile ("ubfx %0, %1, %2, %3" : "=r" (result) : "r" (op1), "i" (op2), "i" (op3));
  return result;
}

#include "arm_neon.h"
/*
// Store 0.f to float32x2x3_t structure element
fast_inline __attribute__((always_inline, optimize("Ofast")))
void vst1_z_f32_x3(float32x2x3_t *op1) {
    register float32x4x3_t result;
  __asm__ volatile(
      "veor %0, %0, %0 \n"
      "veor %1, %1, %1 \n"
      "veor %2, %2, %2 \n"
      "vst1.32 {%0-%2},[%[dst]]"
      : "=w"(result.val[2]), "=w"(result.val[1]), "=w"(result.val[0])
      : [dst] "r" (op1)
      : "memory"
  );
}

fast_inline __attribute__((always_inline, optimize("Ofast")))
float32x4x3_t vld1q_f32_x3(float32x4x3_t *op1) {
  register float32x4x3_t result;
  __asm__ volatile(
      "vld1.32 {%0-%2}, [%[src]]\n"
      : "=w"(result.val[2]), "=w"(result.val[1]), "=w"(result.val[0])
      : [src] "r" (op1)
  );
  return result;
}
*/
fast_inline float32x4x3_t operator+(float32x4x3_t a, const float32x4x3_t& b) {
  a.val[0] += b.val[0];
  a.val[1] += b.val[1];
  a.val[2] += b.val[2];
  return a;
}

fast_inline float32x4x3_t operator-(float32x4x3_t a, const float32x4x3_t& b) {
  a.val[0] -= b.val[0];
  a.val[1] -= b.val[1];
  a.val[2] -= b.val[2];
  return a;
}

fast_inline uq32x4x3_t operator+(uq32x4x3_t a, const uq32x4x3_t& b) {
  a.val[0] += b.val[0];
  a.val[1] += b.val[1];
  a.val[2] += b.val[2];
  return a;
}

fast_inline uq32x4x3_t operator-(uq32x4x3_t a, const uq32x4x3_t& b) {
  a.val[0] -= b.val[0];
  a.val[1] -= b.val[1];
  a.val[2] -= b.val[2];
  return a;
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
