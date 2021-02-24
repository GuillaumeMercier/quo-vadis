/*
 * Copyright (c) 2020-2021 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c) 2020-2021 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-macros.h
 */

#ifndef QVI_MACROS_H
#define QVI_MACROS_H

#define QVI_STRINGIFY(x) #x
#define QVI_TOSTRING(x)  QVI_STRINGIFY(x)

/**
 * Convenience macro used to silence warnings about unused variables.
 *
 * @param[in] x Unused variable.
 */
#define QVI_UNUSED(x)                                                          \
do {                                                                           \
    (void)(x);                                                                 \
} while (0)

/**
 * Convenience wrapper around new(std::nothrow).
 */
#define qvi_new new(std::nothrow)

/**
 * Buffer size for URLs used for client/server connections.
 */
#define QVI_URL_MAX 128

/**
 * Port environment variable string.
 */
#define QVI_ENV_PORT "QV_PORT"

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
