cmake_minimum_required(VERSION 3.8)
project(Nexus)

# =========================================
# global options

option(NEXUS_FORCE_MACRO_PREFIX "if true, only NX_ macro versions are available" OFF)


# =========================================
# define library

file(GLOB_RECURSE SOURCES "src/*.cc" "src/*.hh")

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" FILES ${SOURCES})

add_library(nexus STATIC ${SOURCES})

target_include_directories(nexus PUBLIC src/)

target_link_libraries(nexus PUBLIC
    clean-core
    ctracer
    typed-geometry
)
# TODO: make more dependencies optional?

# =========================================
# optional dependencies

if (TARGET reflector)
    target_compile_definitions(nexus PUBLIC NX_HAS_REFLECTOR)
    target_link_libraries(nexus PUBLIC reflector)
    message(STATUS "[nexus] enable support for reflector")
endif()

# =========================================
# set up compile flags

if (MSVC)
    target_compile_options(nexus PUBLIC /MP)
else()
    target_compile_options(nexus PRIVATE -Wall -fPIC)
    target_link_libraries(nexus PUBLIC -fuse-ld=gold)
endif()

if (NEXUS_FORCE_MACRO_PREFIX)
    target_compile_definitions(nexus PUBLIC NX_FORCE_MACRO_PREFIX)
endif()
