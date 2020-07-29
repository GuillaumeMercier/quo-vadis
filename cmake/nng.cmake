#
# Copyright (c)      2020 Triad National Security, LLC
#                         All rights reserved.
#
# Copyright (c)      2020 Lawrence Livermore National Security, LLC
#                         All rights reserved.
#
# This file is part of the quo-vadis project. See the LICENSE file at the
# top-level directory of this distribution.
#

set(QVI_NNG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/nng-1.3.0)

set(NNG_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(${QVI_NNG_DIR})

# vim: ts=4 sts=4 sw=4 expandtab
