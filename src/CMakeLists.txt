add_library(rdpos STATIC cycle.c packages.c )

target_include_directories(rdpos SYSTEM PUBLIC ${CRC32_INCLUDE_DIRS})
target_include_directories(rdpos PUBLIC .)

target_link_directories(rdpos PUBLIC ${CRC32_LIBRARY_DIRS})
target_link_libraries(rdpos ${CRC32_LIBRARIES})
