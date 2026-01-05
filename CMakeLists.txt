cmake_minimum_required(VERSION 3.10)

project(embedded_http_server_c C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(http_server
    src/main.c
)
