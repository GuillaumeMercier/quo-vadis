/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2024 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-group-pthread.cc
 */

#include "qvi-group-pthread.h"
#include "qvi-utils.h"

qvi_group_pthread_s::qvi_group_pthread_s(
    int group_size
) {
    const int rc = qvi_new(&thgroup, group_size);
    if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error();
}

qvi_group_pthread_s::~qvi_group_pthread_s(void)
{
    qvi_delete(&thgroup);
}

int
qvi_group_pthread_s::self(
    qvi_group_t **
) {
    // TODO(skg)
    return QV_ERR_NOT_SUPPORTED;
}

int
qvi_group_pthread_s::split(
    int,
    int,
    qvi_group_s **
) {
    // TODO(skg)
    return QV_ERR_NOT_SUPPORTED;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
