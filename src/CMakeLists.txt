add_subdirectory(rdp)

add_library(rdpos STATIC rdpos.c serial-datagram/serial_datagram.c)
target_include_directories(rdpos PUBLIC .)
target_include_directories(rdp SYSTEM PUBLIC ${CRC32_INCLUDE_DIRS})
target_link_directories(rdp PUBLIC ${CRC32_LIBRARY_DIRS})
target_link_libraries(rdpos rdp ${CRC32_LIBRARIES})
