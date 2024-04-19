/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2024 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c) 2020-2021 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c) 2022      Inria.
 *                         All rights reserved.
 *
 * Copyright (c) 2022      Bordeaux INP.
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file quo-vadis.h
 */

#ifndef QUO_VADIS_H
#define QUO_VADIS_H

#ifdef __cplusplus
extern "C" {
#endif

/** Convenience definition. */
#define QUO_VADIS 1

/**
 * This number is updated to (X<<16)+(Y<<8)+Z
 * when a release X.Y.Z modifies the API.
 *
 * In python: print(f'{(X<<16)+(Y<<8)+Z:#010x}')
 */
#define QUO_VADIS_API_VERSION 0x00000001

/** Opaque quo-vadis context. */
struct qv_context_s;
typedef struct qv_context_s qv_context_t;

/** Opaque quo-vadis scope. */
struct qv_scope_s;
typedef struct qv_scope_s qv_scope_t;

/* If this changes, please update the order and contents of qvi_rc_strerrs. */
/**
 * Return codes.
 */
enum {
    QV_SUCCESS = 0,
    QV_SUCCESS_ALREADY_DONE,
    QV_SUCCESS_SHUTDOWN,
    QV_ERR,
    QV_ERR_ENV,
    QV_ERR_INTERNAL,
    QV_ERR_FILE_IO,
    QV_ERR_SYS,
    QV_ERR_OOR,
    QV_ERR_INVLD_ARG,
    QV_ERR_CALL_BEFORE_INIT,
    QV_ERR_HWLOC,
    QV_ERR_MPI,
    QV_ERR_MSG,
    QV_ERR_RPC,
    QV_ERR_NOT_SUPPORTED,
    QV_ERR_POP,
    QV_ERR_PMI,
    QV_ERR_NOT_FOUND,
    QV_ERR_SPLIT,
    /** Resources unavailable. */
    QV_RES_UNAVAILABLE,
    /** Sentinel value. */
    QV_RC_LAST
};

/**
 * Intrinsic scope types.
 */
typedef enum qv_scope_intrinsic_e {
    QV_SCOPE_SYSTEM = 0,
    QV_SCOPE_USER,
    QV_SCOPE_JOB,
    QV_SCOPE_PROCESS
} qv_scope_intrinsic_t;

/**
 * Hardware object types.
 */
typedef enum qv_hw_obj_type_e {
    QV_HW_OBJ_MACHINE = 0,
    QV_HW_OBJ_PACKAGE,
    QV_HW_OBJ_CORE,
    QV_HW_OBJ_PU,
    QV_HW_OBJ_L1CACHE,
    QV_HW_OBJ_L2CACHE,
    QV_HW_OBJ_L3CACHE,
    QV_HW_OBJ_L4CACHE,
    QV_HW_OBJ_L5CACHE,
    QV_HW_OBJ_NUMANODE,
    /** Device types */
    QV_HW_OBJ_GPU,
    /** Sentinel value. */
    QV_HW_OBJ_LAST
} qv_hw_obj_type_t;

/**
 * Binding string representaiton formats.
 */
typedef enum qv_bind_string_format_e {
    QV_BIND_STRING_AS_BITMAP = 0,
    QV_BIND_STRING_AS_LIST
} qv_bind_string_format_t;

/**
 * Automatic grouping options for qv_scope_split(). The following values can be
 * used instead of group_id to influence how automatic task grouping is
 * accomplished.
 */
enum {
    /**
     * Constant used to indicate undefined or unknown integer value. This means
     * that the caller will not be considered in the split, and therefore
     * receive an empty scope.
     */
    QV_SCOPE_SPLIT_UNDEFINED = -1,
    /**
     * Split the provided group by attempting to preserve tasks' current
     * affinities (at time of the split call) as much as possible.
     */
    QV_SCOPE_SPLIT_AFFINITY_PRESERVING = -2
};

/**
 *
 */
typedef enum qv_scope_create_hint_e {
    // TODO(skg) Enumerate all actual values.
    // TODO(skg) Add to Fortran interface.
    QV_SCOPE_CREATE_HINT_NONE      = 0x00000000,
    QV_SCOPE_CREATE_HINT_EXCLUSIVE = 0x00000001,
    QV_SCOPE_CREATE_HINT_CLOSE     = 0x00000002
} qv_scope_create_hint_t;

/**
 * Device identifier types.
 */
typedef enum qv_device_id_type_e {
    QV_DEVICE_ID_UUID = 0,
    QV_DEVICE_ID_PCI_BUS_ID,
    QV_DEVICE_ID_ORDINAL
} qv_device_id_type_t;

/**
 * Version query routine.
 *
 * @param[out] major Major version.
 *
 * @param[out] minor Minor version.
 *
 * @param[out] patch Patch version.
 *
 * @retval QV_SUCCESS if the operation completed successfully.
 */
int
qv_version(
    int *major,
    int *minor,
    int *patch
);

/**
 *
 */
int
qv_scope_get(
    qv_context_t *ctx,
    qv_scope_intrinsic_t iscope,
    qv_scope_t **scope
);

/**
 *
 */
int
qv_scope_free(
    qv_context_t *ctx,
    qv_scope_t *scope
);

/**
 *
 */
int
qv_scope_split(
    qv_context_t *ctx,
    qv_scope_t *scope,
    int npieces,
    int group_id,
    qv_scope_t **subscope
);

/**
 *
 */
int
qv_scope_split_at(
    qv_context_t *ctx,
    qv_scope_t *scope,
    qv_hw_obj_type_t type,
    int group_id,
    qv_scope_t **subscope
);

/**
 *
 */
// TODO(skg) Add to Fortran interface.
int
qv_scope_create(
    qv_context_t *ctx,
    qv_scope_t *scope,
    qv_hw_obj_type_t type,
    int nobjs,
    qv_scope_create_hint_t hint,
    qv_scope_t **subscope
);

/**
 *
 */
int
qv_scope_nobjs(
    qv_context_t *ctx,
    qv_scope_t *scope,
    qv_hw_obj_type_t obj,
    int *nobjs
);

/**
 *
 */
int
qv_scope_taskid(
    qv_context_t *ctx,
    qv_scope_t *scope,
    int *taskid
);

/**
 *
 */
int
qv_scope_ntasks(
    qv_context_t *ctx,
    qv_scope_t *scope,
    int *ntasks
);

/**
 *
 */
int
qv_scope_barrier(
    qv_context_t *ctx,
    qv_scope_t *scope
);

/**
 *
 */
int
qv_scope_get_device_id(
    qv_context_t *ctx,
    qv_scope_t *scope,
    qv_hw_obj_type_t dev_obj,
    int dev_index,
    qv_device_id_type_t id_type,
    char **dev_id
);

/**
 *
 */
int
qv_bind_push(
    qv_context_t *ctx,
    qv_scope_t *scope
);

/**
 *
 */
int
qv_bind_pop(
    qv_context_t *ctx
);

/**
 *
 */
int
qv_bind_string(
    qv_context_t *ctx,
    qv_bind_string_format_t format,
    char **str
);

/**
 *
 */
int
qv_context_barrier(
    qv_context_t *ctx
);

/**
 *
 */
const char *
qv_strerr(
    int ec
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
