cmake_minimum_required (VERSION 3.8)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("ZiYue4D")

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

add_subdirectory(stdlib)
