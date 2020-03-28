cmake_minimum_required(VERSION 3.8)
project(Nexus)

include(cmake/UnityBuild.cmake)
include(cmake/SourceGroup.cmake)

# =========================================
# global options

option(NEXUS_FORCE_MACRO_PREFIX "if true, only NX_ macro versions are available" OFF)

option(NEXUS_ENABLE_UNITY_BUILD "If enabled, compiles this library as a single compilation unit" ON)

# =========================================
# define library

file(GLOB_RECURSE SOURCES "src/*.cc")
file(GLOB_RECURSE HEADERS "src/*.hh" "src/*.inl")

arcana_source_group(SOURCES HEADERS)

if (NEXUS_ENABLE_UNITY_BUILD)
    arcana_enable_unity_build(nexus SOURCES 100 cc)
endif()

add_library(nexus STATIC ${SOURCES} ${HEADERS})

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

if (MSVC)
    target_compile_options(nexus PUBLIC /MP)
else()
    target_compile_options(nexus PRIVATE -Wall -fPIC)
    target_link_libraries(nexus PUBLIC -fuse-ld=gold)
endif()

if (NEXUS_FORCE_MACRO_PREFIX)
    target_compile_definitions(nexus PUBLIC NX_FORCE_MACRO_PREFIX)
endif()
