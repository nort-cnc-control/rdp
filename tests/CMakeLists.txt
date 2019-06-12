add_executable(rdp_test test.c)
target_link_libraries(rdp_test rdp)

add_executable(rdp_test_srv test_echo_server.c)
target_link_libraries(rdp_test_srv rdp)

add_executable(rdp_test_clt test_echo_client.c)
target_link_libraries(rdp_test_clt rdp)

