add_executable(rdp_test rdp_test.c)
target_link_libraries(rdp_test rdp)

add_executable(rdpos_test rdpos_test.c)
target_link_libraries(rdpos_test rdpos)

add_executable(rdp_test_srv rdp_test_echo_server.c)
target_link_libraries(rdp_test_srv rdp)

add_executable(rdp_test_clt rdp_test_echo_client.c)
target_link_libraries(rdp_test_clt rdp)

