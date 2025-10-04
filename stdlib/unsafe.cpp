#include "std.hpp"

_STDLIB_BEGIN

int _STDLIB(unsafe_ptr_to_int)(void* ptr) {
    return (int)ptr;
}

void* _STDLIB(unsafe_int_to_ptr)(int ptr) {
    return (void*)ptr;
}

const char* _STDLIB(unsafe_cstr)(ZStr str) {
    return str->c_str();
}

_STDLIB_END
