#include "std.hpp"

#include <stdio.h>

_STDLIB_BEGIN

int _STDLIB(print)(ZStr str) {
    puts(str->c_str());
    return 0;
}

_STDLIB_END