add_executable(rdpos_test test.c)
target_link_libraries(rdpos_test rdpos)

add_executable(rdpos_test_srv test_echo_server.c)
target_link_libraries(rdpos_test_srv rdpos)

add_executable(rdpos_test_clt test_echo_client.c)
target_link_libraries(rdpos_test_clt rdpos)

