#include "std.hpp"
#include <stdio.h>

_STDLIB_BEGIN

_ALWAYS_INLINE
void _STDLIB(Print)(ZStr str) {
    puts(str->c_str());
}

_STDLIB_END