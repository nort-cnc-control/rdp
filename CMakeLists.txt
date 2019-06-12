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
install(DIRECTORY ${CMAKE_SOURCE_DIR}/src/
	DESTINATION include/rdpos
	FILES_MATCHING PATTERN "*.h")

install(FILES ${CMAKE_BINARY_DIR}/rdpos.pc
	DESTINATION lib/pkgconfig)
