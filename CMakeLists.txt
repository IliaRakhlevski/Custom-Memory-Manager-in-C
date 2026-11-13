cmake_minimum_required(VERSION 3.16)
project(CustomMemoryManager C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(MM_WERROR   "Treat compiler warnings as errors" OFF)
option(MM_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer (GCC/Clang)" OFF)

# warnings and sanitizers shared by all targets
function(mm_configure target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
        if(MM_WERROR)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -pedantic)
        if(MM_WERROR)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
        if(MM_SANITIZE)
            target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
            target_link_options(${target} PRIVATE -fsanitize=address,undefined)
        endif()
    endif()
endfunction()

add_library(memory_manager STATIC memory_manager.c)
target_include_directories(memory_manager PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
mm_configure(memory_manager)

# demo program: prints the memory layout after each step
add_executable(memory_manager_demo main.c)
target_link_libraries(memory_manager_demo PRIVATE memory_manager)
mm_configure(memory_manager_demo)

# unit tests
add_executable(test_memory_manager tests/test_memory_manager.c)
target_link_libraries(test_memory_manager PRIVATE memory_manager)
mm_configure(test_memory_manager)

enable_testing()
add_test(NAME memory_manager_tests COMMAND test_memory_manager)
