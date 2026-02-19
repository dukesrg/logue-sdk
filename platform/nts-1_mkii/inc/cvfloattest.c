#include <stdio.h>
#include <stdint.h>

#include "cv_data.h"

int main(int argc, char * argv[]) {
    ieee754_t a;
//    a.i = NAN_MASK | 0xff800000;
    a.i = NAN_MASK | 0x007fffff;
    float b = fromCV(a.f);
    printf("%.10f %08x %08x\n", b, (ieee754_t){.f = b}.i, -2^23);
    printf(" %08x\n", (ieee754_t){.f = b}.i & ~NAN_MASK);
}