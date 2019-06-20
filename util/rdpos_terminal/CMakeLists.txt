include(FindPkgConfig)

add_executable(rdpos_terminal rdpos_terminal.c)

target_include_directories(rdpos_terminal PUBLIC .)
target_link_libraries(rdpos_terminal rt pthread rdpos)
