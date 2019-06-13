add_subdirectory(rdp)

add_library(rdpos STATIC rdpos.c serial-datagram/serial_datagram.c)
target_include_directories(rdpos PUBLIC .)

target_link_libraries(rdpos rdp)
