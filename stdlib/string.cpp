#include "std.hpp"

_STDLIB_BEGIN

_ALWAYS_INLINE
ZStr _STDLIB(create_string__)(const char* raw) {
    return new std::string(raw);
}

_ALWAYS_INLINE
void _STDLIB(release_string__)(ZStr str) {
    // danger! do not imitate
    delete const_cast<std::string*>(str);
}

_ALWAYS_INLINE
ZStr _STDLIB(int_to_string__)(int raw) {
    return new std::string(std::to_string(raw));
}

_ALWAYS_INLINE
ZStr _STDLIB(float_to_string__)(float raw) {
    return new std::string(std::to_string(raw));
}

_ALWAYS_INLINE
ZStr _STDLIB(concat)(ZStr str, ZStr b) {
    return new std::string(*str + *b);
}

ZStr _STDLIB(replace)(ZStr str, ZStr pattern, ZStr newpat) {
    std::string* result = new std::string();
    result->reserve(str->size());
    
    size_t pos = 0, prev_pos = 0;

    while ((pos = str->find(*pattern, pos)) != std::string::npos) {
        result->append(str->substr(prev_pos, pos - prev_pos));
        result->append(*newpat);
        pos += pattern->size();
        prev_pos = pos;
    }

    result->append(str->substr(prev_pos));
    return result;
}

ZStr _STDLIB(string)(ZStr str, int n) {
    std::string* result = new std::string();
    while (n-- > 0) result->append(*str);
    return result;
}

_STDLIB_END