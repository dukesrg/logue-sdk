/*
 * File: custom_data.h
 *
 * User customizable data for logue SDK 2.0
 *
 * 2024 (c) Oleg Burdaev
 * mailto: dukesrg@gmail.com
 *
 */

#pragma once

#ifdef UNIT_API_VERSION

#define DATAVAL(a) #a
#define DATASTR(a) DATAVAL(a)

#define custom_data(a,b,c) __attribute__((used, section("\"" a ", 0, " DATASTR(b) ", " DATASTR(c) "\""), aligned(4)))

#else
  #pragma GCC error "Unsupported platform"
#endif
