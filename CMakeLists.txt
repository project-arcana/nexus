cmake_minimum_required(VERSION 3.8)
project(Nexus)

if (NOT TARGET clean-core)
    message(FATAL_ERROR "[nexus] clean-core must be available")
endif()

# =========================================
# global options

option(NEXUS_FORCE_MACRO_PREFIX "if true, only NX_ macro versions are available" OFF)

# =========================================
# define library

file(GLOB_RECURSE SOURCES "src/*.cc")
file(GLOB_RECURSE HEADERS "src/*.hh" "src/*.inl")

arcana_add_library(NX nexus SOURCES HEADERS)

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

if (TARGET polymesh)
    target_compile_definitions(nexus PUBLIC NX_HAS_POLYMESH)
    target_link_libraries(nexus PUBLIC polymesh)
    message(STATUS "[nexus] enable support for polymesh")
endif()

# =========================================
# set up compile flags

if (NEXUS_FORCE_MACRO_PREFIX)
    target_compile_definitions(nexus PUBLIC NX_FORCE_MACRO_PREFIX)
endif()
