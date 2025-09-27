#include "std.hpp"

#include <stdio.h>

_STDLIB_BEGIN

void _STDLIB(print)(ZStr str) {
    puts(str->c_str());
}

int _STDLIB(unsafe_ptr)(void* ptr) {
    return (int)ptr;
}

_STDLIB_END