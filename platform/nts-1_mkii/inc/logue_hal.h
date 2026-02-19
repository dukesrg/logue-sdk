/*
 * File: logue_hal.h
 *
 * logue SDK 1.0/2.0 hardware abstraction
 *
 * 2024 (c) Oleg Burdaev
 * mailto: dukesrg@gmail.com
 *
 */

#pragma once

#define VALUE_TO_STRING(x) #x
#define VALUE(x) VALUE_TO_STRING(x)
#define VAR_NAME_VALUE(var) #var "=" VALUE(var)

#define DEPAREN(X) ESC(ISH X)
#define ISH(...) ISH __VA_ARGS__
#define ESC(...) ESC_(__VA_ARGS__)
#define ESC_(...) VAN ## __VA_ARGS__
#define VANISH
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
  #pragma message "logue SDK 1.0 detected"
  #define TARGET_PLATFORM VAL(DEPAREN(USER_TARGET_PLATFORM))
  #define TARGET_MODULE VAL(USER_TARGET_MODULE)
#elif defined(UNIT_API_VERSION) && defined(UNIT_TARGET_PLATFORM)
  #pragma message "logue SDK 2.0 detected"
  #define TARGET_PLATFORM VAL(DEPAREN(UNIT_TARGET_PLATFORM))
  #define TARGET_MODULE VAL(UNIT_TARGET_MODULE)
#else 
  #pragma GCC error "Unsupported platform"
#endif

#define k_user_target_prologue_val 1
#define k_user_target_miniloguexd_val 2
#define k_user_target_nutektdigital_val 3
#define k_unit_target_prologue_val 1
#define k_unit_target_miniloguexd_val 2
#define k_unit_target_nts1_val 3
#define k_unit_target_nutektdigital_val 3
#define k_unit_target_drumlogue_val 4
#define k_unit_target_nts1_mkii_val 5
#define k_unit_target_nts3_kaoss_val 6

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
#else
  #pragma GCC error "Unsupported platform"
#endif

#define k_user_module_modfx_val 11
#define k_user_module_delfx_val 12
#define k_user_module_revfx_val 13
#define k_user_module_osc_val 14
#define k_unit_module_modfx_val 21
#define k_unit_module_delfx_val 22
#define k_unit_module_revfx_val 23
#define k_unit_module_osc_val 24
#define k_unit_module_synth_val 25
#define k_unit_module_masterfx_val 26
#define k_unit_module_genericfx_val 27

#ifdef UNIT_TARGET_MODULE
  #if TARGET_MODULE == k_user_module_modfx_val
    #pragma GCC warning "ModFX module detected"
    #include "usermodfx.h"
  #elif TARGET_MODULE == k_user_module_delfx_val
    #pragma GCC warning "DelFX module detected"
    #include "userdel.h"
  #elif TARGET_MODULE == k_user_module_revfx_val
    #pragma GCC warning "RevFX module detected"
    #include "userrevfx.h"
  #elif TARGET_MODULE == k_user_module_osc_val
    #pragma message "OSC module detected"
    #include "userosc.h"
  #elif TARGET_MODULE == k_unit_module_modfx_val
    #pragma GCC warning "ModFX module detected"
    #ifdef UNIT_TARGET_PLATFORM_DRUMLOGUE
      #include "unit.h"
    #else
      #include "unit_modfx.h"
    #endif
  #elif TARGET_MODULE == k_unit_module_delfx_val
    #pragma GCC warning "DelFX module detected"
    #ifdef UNIT_TARGET_PLATFORM_DRUMLOGUE
      #include "unit.h"
    #else
      #include "unit_delfx.h"
    #endif
  #elif TARGET_MODULE == k_unit_module_revfx_val
    #pragma GCC warning "RevFX module detected"
    #ifdef UNIT_TARGET_PLATFORM_DRUMLOGUE
      #include "unit.h"
    #else
      #include "unit_revfx.h"
    #endif
  #elif TARGET_MODULE == k_unit_module_osc_val
    #pragma message "OSC module detected"
    #include "unit_osc.h"
  #elif TARGET_MODULE == k_unit_module_synth_val
    #pragma message "Synth module detected"
    #include "unit.h"
  #elif TARGET_MODULE == k_unit_module_masterfx_val
    #pragma message "MasterFX module detected"
    #include "unit.h"
  #elif TARGET_MODULE == k_unit_module_genericfx_val
    #pragma message "GenericFX module detected"
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
#undef k_user_target_prologue_val
#undef k_user_target_miniloguexd_val
#undef k_user_target_nutektdigital_val
#undef k_unit_target_prologue_val
#undef k_unit_target_miniloguexd_val
#undef k_unit_target_nts1_val
#undef k_unit_target_nutektdigital_val
#undef k_unit_target_drumlogue_val
#undef k_unit_target_nts1_mkii_val
#undef k_unit_target_nts3_kaoss_val

#undef TARGET_MODULE
#undef k_user_module_modfx_val
#undef k_user_module_delfx_val
#undef k_user_module_revfx_val
#undef k_user_module_osc_val
#undef k_unit_module_modfx_val
#undef k_unit_module_delfx_val
#undef k_unit_module_revfx_val
#undef k_unit_module_osc_val
#undef k_unit_module_synth_val
#undef k_unit_module_masterfx_val
#undef k_unit_module_genericfx_val
