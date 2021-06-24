#
# Copyright (c) 2020-2021 Triad National Security, LLC
#                         All rights reserved.
#
# Copyright (c) 2020-2021 Lawrence Livermore National Security, LLC
#                         All rights reserved.
#
# This file is part of the quo-vadis project. See the LICENSE file at the
# top-level directory of this distribution.
#

# Includes support for external projects
include(ExternalProject)

set(QVI_HWLOC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps/hwloc-2.5.0.tar.gz)
set(QVI_HWLOC_BIN ${CMAKE_CURRENT_BINARY_DIR}/hwloc)
set(QVI_HWLOC_STATIC_LIB ${QVI_HWLOC_BIN}/lib/libhwloc.a)
set(QVI_HWLOC_INCLUDES ${QVI_HWLOC_BIN}/include)
# A list containing any relevant GPU flags required in hwloc configuration.
set(QVI_HWLOC_GPU_FLAGS "")

file(MAKE_DIRECTORY ${QVI_HWLOC_INCLUDES})

if(CMAKE_GENERATOR STREQUAL "Unix Makefiles")
    # Workaround for the following warning when attempting to perform make -j:
    # make[3]: warning: jobserver unavailable:
    # using -j1.  Add '+' to parent make rule.
    set(QVI_HWLOC_BUILD_COMMAND "\$(MAKE)")
else()
    # Ninja doesn't like $(MAKE), so just use make
    set(QVI_HWLOC_BUILD_COMMAND "make")
endif()

# TODO(skg) Add option to turn this off even if found.
if(CUDAToolkit_FOUND AND NOT QV_DISABLE_GPU_SUPPORT)
    list(APPEND QVI_HWLOC_GPU_FLAGS "--enable-cuda=yes")
    list(APPEND QVI_HWLOC_GPU_FLAGS "--enable-nvml=yes")
    # TODO(skg) This may be incomplete. Assumes we have pkg-config.
    list(APPEND QVI_HWLOC_GPU_FLAGS "--with-cuda-version=${CUDAToolkit_VERSION}")
else()
    list(APPEND QVI_HWLOC_GPU_FLAGS "--enable-cuda=no")
    list(APPEND QVI_HWLOC_GPU_FLAGS "--enable-nvml=no")
endif()

message(STATUS "hwloc GPU configure flags are:")
list(APPEND CMAKE_MESSAGE_INDENT "  ")
foreach(flag ${QVI_HWLOC_GPU_FLAGS})
    message(STATUS "${flag}")
endforeach()
list(POP_BACK CMAKE_MESSAGE_INDENT)

ExternalProject_Add(
    libhwloc
    URL file://${QVI_HWLOC_DIR}
    URL_MD5 "e9cb9230bcdf450b0948f255d505503f"
    PREFIX ${QVI_HWLOC_BIN}
    CONFIGURE_COMMAND
      <SOURCE_DIR>/configure
      CC=${CMAKE_C_COMPILER}
      CXX=${CMAKE_CXX_COMPILER}
      --prefix=${QVI_HWLOC_BIN}
      --with-hwloc-symbol-prefix=quo_vadis_internal_
      --enable-plugins=no
      --enable-static=yes
      --enable-shared=no
      --enable-libxml2=no
      --enable-cairo=no
      --enable-gl=no
      --enable-opencl=no
      --enable-pci=no
      --enable-libudev=no
      ${QVI_HWLOC_GPU_FLAGS}
    BUILD_COMMAND ${QVI_HWLOC_BUILD_COMMAND}
    INSTALL_COMMAND ${QVI_HWLOC_BUILD_COMMAND} install
    BUILD_BYPRODUCTS ${QVI_HWLOC_STATIC_LIB}
)

add_library(hwloc STATIC IMPORTED GLOBAL)
add_dependencies(hwloc libhwloc)

set_target_properties(
    hwloc
    PROPERTIES
      IMPORTED_LOCATION ${QVI_HWLOC_STATIC_LIB}
      INTERFACE_INCLUDE_DIRECTORIES ${QVI_HWLOC_INCLUDES}
)

# vim: ts=4 sts=4 sw=4 expandtab
