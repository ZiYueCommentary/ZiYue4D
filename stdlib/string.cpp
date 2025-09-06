#include "std.hpp"

_STDLIB_BEGIN
ZStr _STDLIB(create_string__)(const char* raw) {
    return new std::string(raw);
}

ZStr _STDLIB(concat)(ZStr a, ZStr b) {
    return new std::string(*a + *b);
}

int _STDLIB(release_string__)(ZStr a) {
    // danger! do not imitate
    delete const_cast<std::string*>(a);
    return 0;
}

_STDLIB_END