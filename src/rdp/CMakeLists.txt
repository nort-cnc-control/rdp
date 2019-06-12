add_library(rdp STATIC cycle.c packages.c )

target_include_directories(rdp SYSTEM PUBLIC ${CRC32_INCLUDE_DIRS})
target_include_directories(rdp PUBLIC .)

target_link_directories(rdp PUBLIC ${CRC32_LIBRARY_DIRS})
target_link_libraries(rdp ${CRC32_LIBRARIES})
