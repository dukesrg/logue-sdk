/*
 * Host stand-in for ../../common/attributes.h. Same shorthands; the unit
 * header goes in an ordinary data section (Mach-O needs "segment,section").
 */
#ifndef ATTRIBUTES_H_
#define ATTRIBUTES_H_

#define noopt
#define fast

#define force_inline inline __attribute__((always_inline))
#define fast_inline inline __attribute__((always_inline))

#define __unit_callback __attribute__((used))
#define __unit_header __attribute__((used))

#endif // ATTRIBUTES_H_
