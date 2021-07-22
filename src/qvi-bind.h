/*
 * Copyright (c)      2021 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c)      2021 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-bind.h
 */

#ifndef QVI_BIND_H
#define QVI_BIND_H

#include "qvi-common.h"
#include "qvi-task.h"
#include "qvi-hwloc.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque quo-vadis bind stack. */
struct qvi_bind_stack_s;
typedef struct qvi_bind_stack_s qvi_bind_stack_t;

/**
 *
 */
int
qvi_bind_stack_new(
    qvi_bind_stack_t **bstack
);

/**
 *
 */
void
qvi_bind_stack_free(
    qvi_bind_stack_t **bstack
);

/**
 *
 */
int
qvi_bind_stack_init(
    qvi_bind_stack_t *bstack,
    qvi_task_t *task,
    qvi_hwloc_t *hwloc
);

/**
 *
 */
int
qvi_bind_push(
    qvi_bind_stack_t *bstack,
    hwloc_bitmap_t bitmap
);

/**
 *
 */
int
qvi_bind_pop(
    qvi_bind_stack_t *bstack
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
