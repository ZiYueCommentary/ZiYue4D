#pragma once

#include <string>

#define _STDLIB(x) ziyue4d_##x
#define _STDLIB_BEGIN extern "C" {
#define _STDLIB_END }
#define _CONSTRUCTOR __attribute__((constructor))
#define _ALWAYS_INLINE [[clang::always_inline]]

using ZStr = const std::string*;
