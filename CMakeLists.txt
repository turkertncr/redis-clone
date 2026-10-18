cmake_minimum_required(VERSION 3.16)
project(redis_clone C)

set(CMAKE_C_STANDARD 99)

option(JEMALLOC "Use jemalloc as the allocator" ON)

set(COMMON_SOURCES
        src/alloc.c
        src/queue.c
        src/resp_parser.c
        src/redis_object.c
        src/config.c
        src/util.c
        src/command_handler.c
        src/aof_handler.c
        src/repl.c
        src/data_types/quick_list.c
        src/data_types/skip_list.c
        src/data_types/redis_string.c
        src/data_types/zip_list.c
        src/data_types/set.c
        src/data_types/hash_table.c
        src/data_types/sorted_set.c
        include/redis_db.h
        src/redis_db.c
)

add_library(redis_common STATIC ${COMMON_SOURCES})
target_include_directories(redis_common PUBLIC include)

add_executable(redis_server main.c src/server.c)
target_link_libraries(redis_server PRIVATE redis_common)

add_executable(redis_cli client_main.c src/client.c)
target_link_libraries(redis_cli PRIVATE redis_common)

add_custom_command(TARGET redis_server POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy_if_different
${CMAKE_SOURCE_DIR}/redis.conf $<TARGET_FILE_DIR:redis_server>)

if(WIN32)
    target_link_libraries(redis_server PRIVATE ws2_32)
    target_link_libraries(redis_cli PRIVATE ws2_32)
endif()

if(JEMALLOC)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(JEMALLOC_PKG REQUIRED jemalloc)

    if(JEMALLOC_PKG_FOUND)
        target_include_directories(redis_common SYSTEM PUBLIC ${JEMALLOC_PKG_INCLUDE_DIRS})
        target_link_libraries(redis_common PUBLIC ${JEMALLOC_PKG_LIBRARIES})
        target_compile_definitions(redis_common PUBLIC USE_JEMALLOC=1)
        message(STATUS "Jemalloc wired! ${JEMALLOC_PKG_INCLUDE_DIRS}")
    else()
        message(FATAL_ERROR "Jemalloc wiring failed!")
    endif()
endif()
