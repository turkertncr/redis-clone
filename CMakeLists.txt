cmake_minimum_required(VERSION 3.16)
project(redis_clone C)

set(CMAKE_C_STANDARD 99)

option(JEMALLOC "Use jemalloc as the allocator" ON)

file(GLOB_RECURSE SOURCES
        "src/*.c"
        "include/*.h"
        "main.c"
)
add_executable(redis_clone ${SOURCES}
        include/util.h
        src/util.c)
target_include_directories(redis_clone PRIVATE include)

if(WIN32)
    target_link_libraries(redis_clone PRIVATE ws2_32)
endif()

if(JEMALLOC)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(JEMALLOC_PKG REQUIRED jemalloc)

    if(JEMALLOC_PKG_FOUND)
        target_include_directories(redis_clone SYSTEM PRIVATE ${JEMALLOC_PKG_INCLUDE_DIRS})
        target_link_libraries(redis_clone PRIVATE ${JEMALLOC_PKG_LIBRARIES})
        target_compile_definitions(redis_clone PRIVATE USE_JEMALLOC=1)
        message(STATUS "Jemalloc wired! ${JEMALLOC_PKG_INCLUDE_DIRS}")
    else()
        message(FATAL_ERROR "Jemalloc wiring failed!")
    endif()
endif()
