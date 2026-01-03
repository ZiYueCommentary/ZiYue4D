cmake_minimum_required (VERSION 3.13)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project ("ZiYue4D")

add_executable (ZiYue4D "CLI11.hpp" "main.cpp" "Token.h" "Lex.h" "Lex.cpp" "exceptions.h" "AST.h" "AST.cpp" "SemanticAnalyzer.h" "SemanticAnalyzer.cpp" "CodeGen.h" "CodeGen.cpp" )

find_package(LLVM REQUIRED CONFIG)

add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})

llvm_map_components_to_libnames(llvm_libs
  native
  target
  x86codegen
  asmparser
  asmprinter
)

target_link_libraries(ZiYue4D ${llvm_libs})

add_subdirectory(stdlib)
