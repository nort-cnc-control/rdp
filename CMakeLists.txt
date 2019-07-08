project(rdp)
cmake_minimum_required(VERSION 3.13)

set(DEST_DIR "${CMAKE_INSTALL_PREFIX}")
CONFIGURE_FILE("rdp.pc.in" "rdp.pc" @ONLY)

add_subdirectory(src)
if (BUILD_TESTING)
    add_subdirectory(tests)
endif ()

set(RT ${CMAKE_SOURCE_DIR}/src/)

install(TARGETS rdp ARCHIVE DESTINATION lib)
install(FILES   ${RT}/rdp.h
                ${RT}/packages.h
                ${RT}/defs.h
                ${RT}/cycle.h
                ${RT}/config.h
                ${RT}/packages_public.h 
	DESTINATION include/rdp)
install(FILES ${CMAKE_BINARY_DIR}/rdp.pc
	DESTINATION lib/pkgconfig)

