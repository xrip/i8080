cmake_minimum_required(VERSION 4.1)
project(i8080 C)

set(CMAKE_C_STANDARD 23)

add_executable(i8080 main.c)

target_compile_definitions(i8080 PRIVATE
    I8080_TESTS_DIR="${CMAKE_SOURCE_DIR}/tests")
