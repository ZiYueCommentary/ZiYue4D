project(stdlib)

find_program(CLANG_COMPILER clang++)
if(NOT CLANG_COMPILER)
    message(WARNING "clang++ not found, using system default")
    set(CLANG_COMPILER ${CMAKE_CXX_COMPILER})
endif()

file(GLOB STANDARD_LIBRARY_SOURCES "*.cpp")

foreach(SRC_FILE ${STANDARD_LIBRARY_SOURCES})
    get_filename_component(SRC_NAME ${SRC_FILE} NAME_WE)
    set(OBJECT_FILE ${CMAKE_CURRENT_BINARY_DIR}/${SRC_NAME}.bc)
    
    add_custom_command(
        OUTPUT ${OBJECT_FILE}
        COMMAND ${CLANG_COMPILER}
                -emit-llvm -c -std=c++20
                ${SRC_FILE}
                -o ${OBJECT_FILE}
        DEPENDS ${SRC_FILE}
        COMMENT "Compiling ${SRC_NAME} to LLVM bitcode"
    )
    
    list(APPEND STD_LIB_OBJECTS ${OBJECT_FILE})
endforeach()

find_program(LLVM_LINK llvm-link REQUIRED)

add_custom_target(stdlib ALL
    COMMAND ${LLVM_LINK}
            -o ${CMAKE_CURRENT_BINARY_DIR}/../stdlib.bc
            ${STD_LIB_OBJECTS}
    DEPENDS ${STD_LIB_OBJECTS}
    COMMENT "Linking standard library LLVM bitcode"
    VERBATIM
)