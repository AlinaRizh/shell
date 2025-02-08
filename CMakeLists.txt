cmake_minimum_required(VERSION 3.26)
project(my_shell C)

set(CMAKE_C_STANDARD 11)

add_executable(my_shell
        app/main.c)
