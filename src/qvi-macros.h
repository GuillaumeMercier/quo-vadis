/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2025 Triad National Security, LLC
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

/**
 * Add branch prediction information: will likely happen.
 */
#define qvi_likely(x) __builtin_expect(!!(x), 1)

/**
 * Add branch prediction information: won't likely happen.
 */
#define qvi_unlikely(x) __builtin_expect(!!(x), 0)

/**
 * Convenience macro used to silence warnings about unused variables.
 *
 * @param[in] x Unused variable.
 */
#define qvi_unused(x)                                                          \
do {                                                                           \
    (void)(x);                                                                 \
} while (0)

#define qvi_runtime_error()                                                    \
std::runtime_error(__FILE__ ":" + std::to_string(__LINE__))

#define qvi_catch_and_return()                                                 \
catch (...)                                                                    \
{                                                                              \
    auto eptr = std::current_exception();                                      \
    try {                                                                      \
        if (eptr) std::rethrow_exception(eptr);                                \
        qvi_log_error("An unknown exception occurred.");                       \
    }                                                                          \
    catch(const std::exception &e)                                             \
    {                                                                          \
        qvi_log_error("An exception occurred at {}", e.what());                \
    }                                                                          \
    return QV_ERR;                                                             \
}                                                                              \
do {                                                                           \
} while (0)

/**
 * Prints abort location then calls abort().
 */
#define qvi_abort()                                                            \
do {                                                                           \
    qvi_log_info("abort() raised at {}:{}", __FILE__, __LINE__);               \
    abort();                                                                   \
} while (0)

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
