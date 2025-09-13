#include "std.hpp"

_STDLIB_BEGIN

ZStr _RETURN_STRING _STDLIB(create_string__)(const char* raw) {
    return new std::string(raw);
}

ZStr _RETURN_STRING _STDLIB(int_to_string__)(int raw) {
    return new std::string(std::to_string(raw));
}

ZStr _RETURN_STRING _STDLIB(float_to_string__)(float raw) {
    return new std::string(std::to_string(raw));
}

ZStr _RETURN_STRING _STDLIB(concat)(ZStr a, ZStr b) {
    return new std::string(*a + *b);
}

void _STDLIB(release_string__)(ZStr a) {
    // danger! do not imitate
    delete const_cast<std::string*>(a);
}

_STDLIB_END