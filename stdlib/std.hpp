#pragma once

#include <string>

#define _STDLIB(x) _ziyue4d_##x
#define _STDLIB_BEGIN extern "C" {
#define _STDLIB_END }

#ifdef _WIN64
#define INT int64_t
#else
#define INT int32_t
#endif

#define ZStr const std::string* __attribute__((annotate("ziyue4d_string")))