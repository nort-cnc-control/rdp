add_library(rdpos STATIC cycle.c packages.c )
target_include_directories(rdpos PUBLIC .)
target_link_libraries(rdpos crc32)
