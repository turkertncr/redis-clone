cmake_minimum_required(VERSION 3.16)
project(redis_clone C)

set(CMAKE_C_STANDARD 99)

add_executable(redis_clone main.c
        include/data_types/hash_table.h
        src/data_types/hash_table.c
        include/resp_parser.h
        src/resp_parser.c
        include/redis_object.h
        src/redis_object.c
        include/server.h
        src/server.c
        include/command_handler.h
        src/command_handler.c
        include/data_types/quick_list.h
        include/data_types/zip_list.h
        src/data_types/zip_list.c
        src/data_types/quick_list.c
        include/data_types/redis_string.h
        src/data_types/redis_string.c
        include/data_types/skip_list.h
        src/data_types/skip_list.c
        include/data_types/sorted_set.h
        src/data_types/sorted_set.c
        include/data_types/set.h
        src/data_types/set.c
        include/repl.h
        src/repl.c
        include/client.h
        src/client.c
)

target_include_directories(redis_clone PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if(WIN32)
    target_link_libraries(redis_clone PRIVATE ws2_32)
endif()
