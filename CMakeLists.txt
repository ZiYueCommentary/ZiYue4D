# CMakeList.txt: Cmake4D 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#
cmake_minimum_required (VERSION 3.8)

# 如果支持，请为 MSVC 编译器启用热重载。
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("Cmake4D")

# 将源代码添加到此项目的可执行文件。
add_executable (ZiYue4D "test.cpp" "Token.h" "Lex.h" "Lex.cpp" "exceptions.h" "AST.h" "AST.cpp" "SemanticAnalyzer.h" "SemanticAnalyzer.cpp" "CodeGen.h" "CodeGen.cpp")

find_package(LLVM REQUIRED CONFIG)

add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})

llvm_map_components_to_libnames(llvm_libs
  native
  orcjit
  executionengine
  target
  x86codegen
  asmparser
  asmprinter
)

target_link_libraries(ZiYue4D ${llvm_libs})

set_property(TARGET ZiYue4D PROPERTY CXX_STANDARD 20)

# TODO: 如有需要，请添加测试并安装目标。
