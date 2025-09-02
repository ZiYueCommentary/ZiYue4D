project(stdlib)

find_program(CLANG_COMPILER clang++)
if(NOT CLANG_COMPILER)
    message(WARNING "clang++ not found, using system default")
    set(CLANG_COMPILER ${CMAKE_CXX_COMPILER})
endif()

file(GLOB STANDARD_LIBRARY_SOURCES "*.cpp")

add_custom_target(stdlib ALL
    COMMAND ${CLANG_COMPILER} 
            -emit-llvm -c -std=c++20
            ${STANDARD_LIBRARY_SOURCES}
            -o ${CMAKE_CURRENT_BINARY_DIR}/../stdlib.bc
    DEPENDS ${STANDARD_LIBRARY_SOURCES}
    COMMENT "Generating standard library LLVM IR"
    VERBATIM
)
