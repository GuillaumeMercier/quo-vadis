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
 * @file qvi-task.h
 */

#ifndef QVI_TASK_H
#define QVI_TASK_H

#include "qvi-common.h"

#ifdef __cplusplus
extern "C" {
#endif

struct qvi_task_s;
typedef struct qvi_task_s qvi_task_t;

/**
 *
 */
int
qvi_task_new(
    qvi_task_t **task
);

/**
 *
 */
void
qvi_task_free(
    qvi_task_t **task
);

/**
 *
 */
int
qvi_task_init(
    qvi_task_t *task,
    pid_t pid,
    int64_t gid,
    int lid
);

/**
 *
 */
pid_t
qvi_task_pid(
    qvi_task_t *task
);

/**
 *
 */
int64_t
qvi_task_gid(
    qvi_task_t *task
);

/**
 *
 */
int
qvi_task_lid(
    qvi_task_t *task
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
