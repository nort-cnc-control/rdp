add_executable(rdpos_test rdpos_test.c)
target_link_libraries(rdpos_test rdpos)

add_executable(rdpos_test_srv rdpos_test_echo_server2.c)
target_link_libraries(rdpos_test_srv rdpos)

add_executable(rdpos_test_clt rdpos_test_echo_client.c)
target_link_libraries(rdpos_test_clt rdpos)

