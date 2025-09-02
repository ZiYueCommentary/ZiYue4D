#include <math.h>
#include <stdlib.h>

#define _STDLIB(x) _ziyue4d_##x

extern "C" {
    float _STDLIB(sin)(float x) {
        return sinf(x);
    }

    float _STDLIB(cos)(float x) {
        return cosf(x);
    }

    float _STDLIB(tan)(float x) {
        return tanf(x);
    }

    float _STDLIB(sqr)(float x) {
        return sqrtf(x);
    }
}