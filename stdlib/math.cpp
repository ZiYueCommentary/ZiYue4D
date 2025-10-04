#include "std.hpp"

#include <math.h>
#include <stdlib.h>

_STDLIB_BEGIN

_ALWAYS_INLINE
float _STDLIB(sin)(float x) {
    return sinf(x);
}

_ALWAYS_INLINE
float _STDLIB(cos)(float x) {
    return cosf(x);
}

_ALWAYS_INLINE
float _STDLIB(tan)(float x) {
    return tanf(x);
}

_ALWAYS_INLINE
float _STDLIB(sqr)(float x) {
    return sqrtf(x);
}

_ALWAYS_INLINE
float _STDLIB(round)(float x) {
    return roundf(x);
}

_STDLIB_END