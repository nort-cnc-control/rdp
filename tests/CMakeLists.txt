add_executable(test test.c)
target_link_libraries(test rdpos)

add_executable(test_srv test_echo_server.c)
target_link_libraries(test_srv rdpos)

add_executable(test_clt test_echo_client.c)
target_link_libraries(test_clt rdpos)

