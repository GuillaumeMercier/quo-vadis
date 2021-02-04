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
 * @file qv-mpi.h
 */

#ifndef QUO_VADIS_MPI_H
#define QUO_VADIS_MPI_H

#include "quo-vadis/qv-types.h"

#include "mpi.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 *
 */
int
qv_mpi_create(
    qv_context_t **ctx,
    MPI_Comm comm
);

/**
 *
 */
int
qv_mpi_free(
    qv_context_t *ctx
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
