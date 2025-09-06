#include "std.hpp"

#include <math.h>
#include <stdlib.h>

_STDLIB_BEGIN

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

_STDLIB_END