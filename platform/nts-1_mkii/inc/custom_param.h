/*
 * File: custom_param.h
 *
 * User customizable parameters wrapper for logue SDK 1.0/2.0
 *
 * 2021-2025 (c) Oleg Burdaev
 * mailto: dukesrg@gmail.com
 *
 */

#pragma once

#include "logue_wrap.h"

#ifdef USER_API_VERSION
#include "userosc.h"

#ifndef CUSTOM_PARAM_OFFSET
  #define CUSTOM_PARAM_OFFEST 256
#endif

#define CUSTOM_PARAM_INIT(a,b,c,d,e,f,g,h) static const __attribute__((used, section(".hooks"))) uint16_t custom_params[k_num_user_osc_param_id] = {a,b,c,d,e,f,g,h}

#elif defined(UNIT_API_VERSION)
#include "custom_data.h"

#ifdef UNIT_OSC_H_
  #define CUSTOM_PARAM_OFFSET UNBRACE(UNBRACE(UNIT_OSC_MAX_PARAM_COUNT))
#elif defined(UNIT_MODFX_H_)
  #define CUSTOM_PARAM_OFFSET UNBRACE(UNBRACE(UNIT_MODFX_MAX_PARAM_COUNT))
#elif defined(UNIT_DELFX_H_)
  #define CUSTOM_PARAM_OFFSET UNBRACE(UNBRACE(UNIT_DELFX_MAX_PARAM_COUNT))
#elif defined(UNIT_REVFX_H_)
  #define CUSTOM_PARAM_OFFSET UNBRACE(UNBRACE(UNIT_REVFX_MAX_PARAM_COUNT))
#elif defined(UNIT_GENERICFX_H_)
  #define CUSTOM_PARAM_OFFSET UNBRACE(UNBRACE(UNIT_GENERICFX_MAX_PARAM_COUNT))
#elif defined(UNIT_MAX_PARAM_COUNT)
  #define CUSTOM_PARAM_OFFSET UNBRACE(UNBRACE(UNIT_MAX_PARAM_COUNT))
#else
  #pragma GCC error "Unable to define custom parameters offset: unit max parameter count is not set"
#endif

#ifdef UNIT_TARGET_PLATFORM_DRUMLOGUE
  #define PARAM_SIZE 23
#elif defined(UNIT_TARGET_PLATFORM_NTS1_MKII) || defined(UNIT_TARGET_PLATFORM_NTS3_KAOSS)
  #define PARAM_SIZE 32
#elif defined(UNIT_TARGET_PLATFORM_MICROKORG2)
  #define PARAM_SIZE 19
#else  
  #pragma GCC error "Unsupported platform"
#endif

#if CUSTOM_PARAM_COUNT < CUSTOM_PARAM_OFFSET
  #pragma GCC error "Custom params count is less than unit params count"
#endif

#if (CUSTOM_PARAM_OFFSET + CUSTOM_PARAM_COUNT) > 256
  #pragma GCC error "Custom params count is too big"
#endif

#define CUSTOM_PARAMS(...) custom_data("custom_params", PARAM_SIZE, CUSTOM_PARAM_COUNT) const unit_param_t custom_params_[CUSTOM_PARAM_COUNT] = {__VA_ARGS__}
#define CUSTOM_PARAM_INIT(...) custom_data("custom_params_mapping", 2, CUSTOM_PARAM_OFFSET) uint16_t custom_params[CUSTOM_PARAM_OFFSET] = {__VA_ARGS__}

#else
  #pragma GCC error "Unsupported platform"
#endif

#define CUSTOM_PARAM_GET(a) ((uint16_t *)custom_params)[a]
#define CUSTOM_PARAM_SET(a,b) CUSTOM_PARAM_GET(a) = b
#define CUSTOM_PARAM_ID(a) (a + CUSTOM_PARAM_OFFSET - 1)
