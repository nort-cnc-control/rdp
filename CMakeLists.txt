project(rdpos)
cmake_minimum_required(VERSION 3.13)

include(FindPkgConfig)
pkg_search_module(CRC32 REQUIRED crc32)

add_subdirectory(src)

if (NOT DEFINED NO_TESTS)
    add_subdirectory(tests)
endif ()
