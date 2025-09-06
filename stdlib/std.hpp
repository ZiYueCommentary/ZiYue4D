#pragma once

#include <string>

#define _STDLIB(x) _ziyue4d_##x
#define _STDLIB_BEGIN extern "C" {
#define _STDLIB_END }

using ZStr = const std::string*;