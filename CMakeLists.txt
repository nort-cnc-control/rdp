project(rdpos)
cmake_minimum_required(VERSION 3.13)

include(FindPkgConfig)
pkg_search_module(CRC32 REQUIRED crc32)

set(DEST_DIR "${CMAKE_INSTALL_PREFIX}")
foreach(LIB ${COMMA_SEAPARATED_LIST})
  set(PRIVATE_LIBS "${PRIVATE_LIBS} -l${LIB}")
endforeach()
CONFIGURE_FILE("rdpos.pc.in" "rdpos.pc" @ONLY)

add_subdirectory(src)
if (BUILD_TESTING)
    add_subdirectory(tests)
endif ()

install(TARGETS rdpos ARCHIVE DESTINATION lib)

set(RT ${CMAKE_SOURCE_DIR}/src/)

install(FILES   ${RT}/rdpos.h
                ${RT}/rdp/rdp.h
                ${RT}/rdp/packages.h
                ${RT}/rdp/defs.h
                ${RT}/rdp/cycle.h
                ${RT}/rdp/config.h
                ${RT}/rdp/packages_public.h 
	DESTINATION include/rdpos)

install(FILES ${CMAKE_BINARY_DIR}/rdpos.pc
	DESTINATION lib/pkgconfig)
