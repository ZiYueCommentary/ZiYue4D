#include "std.hpp"

#include <stdio.h>

_STDLIB_BEGIN

void _STDLIB(print)(ZStr str) {
    puts(str->c_str());
}

_STDLIB_END