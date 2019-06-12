add_subdirectory(rdp)

add_library(rdpos STATIC rdpos.c )
target_include_directories(rdpos PUBLIC .)

target_link_libraries(rdpos rdp)
