/*
 *  File: logue_wrap.h
 *
 *  logue SDK 1.X/2.x wrapper
 *
 *  2024-2025 (c) Oleg Burdaev
 *  mailto: dukesrg@gmail.com
 */

#pragma once

#define UNBRACE(X) ESC(ESCE X)
#define ESCE(...) ESCE __VA_ARGS__
#define ESC(...) ESC_(__VA_ARGS__)
#define ESC_(...) EVAN ## __VA_ARGS__
#define EVANESCE

#define VAL_(a) a##_val
#define VAL(a) VAL_(a)

#ifdef ARM_MATH_CM4
  #pragma message "ARM Cortex M4 target detected"
  #include "userprg.h"
#elif defined(__cortex_a7__)
  #pragma message "ARM Cortex A7 target detected"
  #include "runtime.h"
#elif defined(ARM_MATH_CM7)
  #pragma message "ARM Cortex M7 target detected"
  #include "runtime.h"
#endif

#if defined(USER_API_VERSION) && defined(USER_TARGET_PLATFORM)
  #pragma message "logue SDK 1.x detected"
  #define TARGET_PLATFORM USER_TARGET_PLATFORM
  #define TARGET_MODULE USER_TARGET_MODULE
#elif defined(UNIT_API_VERSION) && defined(UNIT_TARGET_PLATFORM)
  #pragma message "logue SDK 2.x detected"
  #define TARGET_PLATFORM VAL(UNBRACE(UNIT_TARGET_PLATFORM))
  #define TARGET_MODULE VAL(UNIT_TARGET_MODULE)
#else 
  #pragma GCC error "Unsupported platform"
#endif

#define k_unit_target_prologue_val (1<<8)
#define k_unit_target_miniloguexd_val (2<<8)
#define k_unit_target_nts1_val (3<<8)
#define k_unit_target_nutektdigital_val (3<<8)
#define k_unit_target_drumlogue_val (4<<8)
#define k_unit_target_nts1_mkii_val (5<<8)
#define k_unit_target_nts3_kaoss_val (6<<8)
#define k_unit_target_microkorg2_val (7<<8)

#if TARGET_PLATFORM == k_unit_target_prologue_val
  #pragma message "prologue target detected"
  #define UNIT_TARGET_PLATFORM_PROLOGUE
#elif TARGET_PLATFORM == k_unit_target_miniloguexd_val
  #pragma message "minilogue XD target detected"
  #define UNIT_TARGET_PLATFORM_MINILOGUEXD
#elif TARGET_PLATFORM == k_unit_target_nts1_val
  #pragma message "NTS-1 target detected"
  #define UNIT_TARGET_PLATFORM_NTS1
#elif TARGET_PLATFORM == k_unit_target_drumlogue_val
  #pragma message "drumlogue target detected"
  #define UNIT_TARGET_PLATFORM_DRUMLOGUE
#elif TARGET_PLATFORM == k_unit_target_nts1_mkii_val
  #pragma message "NTS-1 mkII target detected"
  #define UNIT_TARGET_PLATFORM_NTS1_MKII
#elif TARGET_PLATFORM == k_unit_target_nts3_kaoss_val
  #pragma message "NTS-3 target detected"
  #define UNIT_TARGET_PLATFORM_NTS3_KAOSS
#elif TARGET_PLATFORM == k_unit_target_microkorg2_val
  #pragma message "microKORG2 target detected"
  #define UNIT_TARGET_PLATFORM_MICROKORG2
  #define UNIT_TIMBRE_COUNT 2
  #include "macros.h"
  #include "mk2_utils_x.h"
#else
  #pragma GCC error "Unsupported platform"
#endif

#define k_unit_module_modfx_val 1
#define k_unit_module_delfx_val 2
#define k_unit_module_revfx_val 3
#define k_unit_module_osc_val 4
#define k_unit_module_synth_val 5
#define k_unit_module_masterfx_val 6
#define k_unit_module_genericfx_val 7

#define UNIT_INPUT_CHANNELS 2
#define UNIT_OUTPUT_CHANNELS 2
#define unit_output_type_t float
#define float_to_output(a) (a)
#define q31_to_output(a) (q31_to_f32(a))

#ifdef TARGET_MODULE
  #if TARGET_MODULE == k_unit_module_modfx_val
    #pragma message "ModFX module detected"
    #define UNIT_TARGET_MODULE_MODFX
    #ifdef UNIT_TARGET_PLATFORM_DRUMLOGUE
      #include "unit.h"
    #elif defined(UNIT_TARGET_PLATFORM_NTS1_MKII) || defined(UNIT_TARGET_PLATFORM_MICROKORG2)
      #include "unit_modfx.h"
    #else
      #include "usermodfx.h"
    #endif
  #elif TARGET_MODULE == k_unit_module_delfx_val
    #pragma message "DelFX module detected"
    #define UNIT_TARGET_MODULE_DELFX
    #ifdef UNIT_TARGET_PLATFORM_DRUMLOGUE
      #include "unit.h"
    #elif defined(UNIT_TARGET_PLATFORM_NTS1_MKII) || defined(UNIT_TARGET_PLATFORM_MICROKORG2)
      #include "unit_delfx.h"
    #else
      #include "userdel.h"
    #endif
  #elif TARGET_MODULE == k_unit_module_revfx_val
    #pragma message "RevFX module detected"
    #define UNIT_TARGET_MODULE_REVFX
    #ifdef UNIT_TARGET_PLATFORM_DRUMLOGUE
      #include "unit.h"
    #elif defined(UNIT_TARGET_PLATFORM_NTS1_MKII) || defined(UNIT_TARGET_PLATFORM_MICROKORG2)
      #include "unit_revfx.h"
    #else
      #include "userrevfx.h"
    #endif
  #elif TARGET_MODULE == k_unit_module_osc_val
    #pragma message "OSC module detected"
    #define UNIT_TARGET_MODULE_OSC
    #if defined(UNIT_TARGET_PLATFORM_MICROKORG2)
      #include "unit_osc.h"
      #undef UNIT_INPUT_CHANNELS
      #define UNIT_INPUT_CHANNELS 0
      #undef UNIT_OUTPUT_CHANNELS
      #define UNIT_OUTPUT_CHANNELS 8
    #elif defined(UNIT_TARGET_PLATFORM_NTS1_MKII)
      #include "unit_osc.h"
      #undef UNIT_OUTPUT_CHANNELS
      #define UNIT_OUTPUT_CHANNELS 1
    #else
      #include "userosc.h"
      #undef unit_output_type_t
      #define unit_output_type_t q31_t
      #undef float_to_output
      #define float_to_output(a) (f32_to_q31(a))
      #undef q31_to_output
      #define q31_to_output(a) (a)
    #endif
  #elif TARGET_MODULE == k_unit_module_synth_val
    #pragma message "Synth module detected"
    #define UNIT_TARGET_MODULE_SYNTH
    #undef UNIT_INPUT_CHANNELS
    #define UNIT_INPUT_CHANNELS 0
    #include "unit.h"
  #elif TARGET_MODULE == k_unit_module_masterfx_val
    #pragma message "MasterFX module detected"
    #define UNIT_TARGET_MODULE_MASTERFX
    #undef UNIT_INPUT_CHANNELS
    #define UNIT_INPUT_CHANNELS 4
    #include "unit.h"
  #elif TARGET_MODULE == k_unit_module_genericfx_val
    #pragma message "GenericFX module detected"
    #define UNIT_TARGET_MODULE_GENERICFX
    #include "unit_genericfx.h"
  #else
    #pragma GCC error "Unsupported unit module target"
  #endif
#else
  #pragma GCC error "Unsupported unit module target"
#endif

#ifdef UNIT_GENERICFX_H_
  #define UNIT_HEADER_TYPE genericfx_unit_header_t
  #define UNIT_HEADER_TARGET_FIELD unit_header.common.target
#else
  #define UNIT_HEADER_TYPE unit_header_t
  #define UNIT_HEADER_TARGET_FIELD unit_header.target
#endif

#define UNIT_HEADER_TARGET_VALUE (UNIT_TARGET_PLATFORM | UNIT_TARGET_MODULE)

#undef TARGET_PLATFORM
#undef k_unit_target_prologue_val
#undef k_unit_target_miniloguexd_val
#undef k_unit_target_nts1_val
#undef k_unit_target_nutektdigital_val
#undef k_unit_target_drumlogue_val
#undef k_unit_target_nts1_mkii_val
#undef k_unit_target_nts3_kaoss_val
#undef k_unit_target_microkorg2_val

#undef TARGET_MODULE
#undef k_unit_module_modfx_val
#undef k_unit_module_delfx_val
#undef k_unit_module_revfx_val
#undef k_unit_module_osc_val
#undef k_unit_module_synth_val
#undef k_unit_module_masterfx_val
#undef k_unit_module_genericfx_val
