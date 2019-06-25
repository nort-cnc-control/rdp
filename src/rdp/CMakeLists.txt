add_library(rdp STATIC cycle.c packages.c )

target_include_directories(rdp PUBLIC .)
