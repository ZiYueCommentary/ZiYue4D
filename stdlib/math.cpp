#include "std.hpp"

#include <math.h>
#include <stdlib.h>

_STDLIB_BEGIN

_ALWAYS_INLINE
float _STDLIB(Sin)(float x) {
    return sinf(x);
}

_ALWAYS_INLINE
float _STDLIB(Cos)(float x) {
    return cosf(x);
}

_ALWAYS_INLINE
float _STDLIB(Tan)(float x) {
    return tanf(x);
}

_ALWAYS_INLINE
float _STDLIB(Sqr)(float x) {
    return sqrtf(x);
}

_ALWAYS_INLINE
float _STDLIB(Round)(float x) {
    return roundf(x);
}

_STDLIB_END