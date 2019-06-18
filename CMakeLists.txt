project(rdpos)
cmake_minimum_required(VERSION 3.13)

include(FindPkgConfig)
pkg_search_module(CRC32 REQUIRED crc32)

set(DEST_DIR "${CMAKE_INSTALL_PREFIX}")
foreach(LIB ${COMMA_SEAPARATED_LIST})
  set(PRIVATE_LIBS "${PRIVATE_LIBS} -l${LIB}")
endforeach()
CONFIGURE_FILE("rdpos.pc.in" "rdpos.pc" @ONLY)
CONFIGURE_FILE("rdp.pc.in" "rdp.pc" @ONLY)


add_subdirectory(src)
if (BUILD_TESTING)
    add_subdirectory(tests)
endif ()

set(RT ${CMAKE_SOURCE_DIR}/src/)

install(TARGETS rdpos ARCHIVE DESTINATION lib)
install(FILES   ${RT}/rdpos.h
                ${RT}/rdp/rdp.h
                ${RT}/rdp/packages.h
                ${RT}/rdp/defs.h
                ${RT}/rdp/cycle.h
                ${RT}/rdp/config.h
                ${RT}/rdp/packages_public.h 
	DESTINATION include/rdpos)
install(FILES   ${RT}/serial-datagram/serial_datagram.h
  DESTINATION include/rdpos/serial-datagram)
install(FILES ${CMAKE_BINARY_DIR}/rdpos.pc
	DESTINATION lib/pkgconfig)

install(TARGETS rdp ARCHIVE DESTINATION lib)
install(FILES   ${RT}/rdp/rdp.h
                ${RT}/rdp/packages.h
                ${RT}/rdp/defs.h
                ${RT}/rdp/cycle.h
                ${RT}/rdp/config.h
                ${RT}/rdp/packages_public.h 
	DESTINATION include/rdp)
install(FILES ${CMAKE_BINARY_DIR}/rdp.pc
	DESTINATION lib/pkgconfig)
