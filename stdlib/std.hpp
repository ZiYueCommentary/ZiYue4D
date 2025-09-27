#pragma once

#include <string>

#define _STDLIB(x) __attribute__((always_inline)) _ziyue4d_##x
#define _STDLIB_BEGIN extern "C" {
#define _STDLIB_END }
#define _CONSTRUCTOR __attribute__((constructor))

#ifdef _WIN64
#define INT int64_t
#else
#define INT int32_t
#endif

using ZStr = const std::string*;
