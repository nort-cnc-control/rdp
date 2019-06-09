#project(rdpos)
#cmake_minimum_required(VERSION 3.13)

add_subdirectory(src)

if (NOT DEFINED NO_TESTS)
    add_subdirectory(tests)
endif ()
