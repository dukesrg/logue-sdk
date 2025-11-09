/*
 *  File: logue_perf.h
 *
 *  logue SDK 2.x performance monitoring
 *
 *  2025 (c) Oleg Burdaev
 *  mailto: dukesrg@gmail.com
 */

#pragma once

#ifdef __cplusplus

#ifdef PERFMON_ENABLE
#ifdef __cortex_a7__
  #include <time.h>
#elif defined(ARM_MATH_CM7)
  #include <../ext/CMSIS/Device/ARM/ARMCM7/Include/ARMCM7_DP.h>
#else
  #pragma GCC error "Unsupported platform"
#endif

#ifdef UNIT_TARGET_MODULE_GENERICFX
#define UNIT_PARAMS unit_header.common.params
#else
#define UNIT_PARAMS unit_header.params
#endif

#include <limits.h>

enum perfmon_values {
  perfmon_val_low = 0U,
  perfmon_val_avg,
  perfmon_val_high,
  perfmon_val_count
};

static struct perfmon {
#ifdef __cortex_a7__
  struct timespec tss, tse;
#endif
  uint32_t val[perfmon_val_count], cnt, sum;

  perfmon () {
    reset(true);
  }

  inline __attribute__((optimize("Ofast"), always_inline))
  void reset(bool reset) {
    if (!reset)
      return;
    val[perfmon_val_low] = UINT32_MAX;
    val[perfmon_val_avg] = 0;
    val[perfmon_val_high] = 0;
    cnt = 0;
    sum = 0;
    value(true);
  }

  inline __attribute__((optimize("Ofast"), always_inline))
  void start() {
#ifdef __cortex_a7__
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &tss);
#else
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
#endif
  }

  inline __attribute__((optimize("Ofast"), always_inline))
  void end(uint32_t frames) {
    uint32_t dif;
#ifdef __cortex_a7__
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &tse);
    dif = tse.tv_nsec - tss.tv_nsec;
    if (dif >= 0x80000000)
      dif += 1000000000;
#else
    dif = DWT->CYCCNT;
    CoreDebug->DEMCR &= ~CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL &= ~DWT_CTRL_CYCCNTENA_Msk;
#endif
    sum += dif;
    cnt += frames;
    dif >>= __builtin_ctz(frames);
    if (dif < val[perfmon_val_low])
      val[perfmon_val_low] = dif;
    else if (dif > val[perfmon_val_high])
      val[perfmon_val_high] = dif;
  }

  inline __attribute__((optimize("Ofast"), always_inline))
  const char *value(bool update) {
#ifdef UNIT_TARGET_PLATFORM_MICROKORG2
    static char s[] = "{Low=---{Avg=---{High=---";
    static const uint32_t idx[] = {5, 13, 22}; 
#else
    static char s[] = "L.--- A.--- H.---";
    static const uint32_t idx[] = {2, 8, 14}; 
#endif
    static const char *hex = "0123456789ABCDEF";
    static const uint32_t num_chars = 3;
    if (update) {
      val[perfmon_val_avg] = cnt == 0 ? 0 : sum / cnt;
      for (uint32_t v = 0; v < perfmon_val_count; v++)
        for (uint32_t i = 0; i < num_chars; ++i)
          s[idx[v] + i] = hex[(val[v] >> ((num_chars - 1 - i) << 2)) & 0x0F];
    }
    return s;
  }
} perfmon;
#define PERFMON_START perfmon.start();
#define PERFMON_END(samples) perfmon.end(samples);
#define PERFMON_RESET(param_num, index, value) if (index == param_num) return perfmon.reset(value == UNIT_PARAMS[param_num].min);
#define PERFMON_VALUE(param_num, index, value) if (index == param_num) return perfmon.value(value == UNIT_PARAMS[param_num].max);
#else
#define PERFMON_START
#define PERFMON_END(samples)
#define PERFMON_RESET(param_num, index, value)
#define PERFMON_VALUE(param_num, index, value)
#endif

#else
#define PERFMON_PARAM {0, 1023, 0, 0, k_unit_param_type_strings, 0, k_unit_param_frac_mode_fixed, 0, {"PerfMon"}}
#ifdef UNIT_TARGET_MODULE_GENERICFX
#define PERFMON_DEFAULT_MAPPING {k_genericfx_param_assign_none, k_genericfx_curve_linear, k_genericfx_curve_unipolar, 0, 1023, 0}
#endif
#endif