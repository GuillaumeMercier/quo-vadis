/*
 * Copyright (c)      2020 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c)      2020 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qv-hwloc.cc
 */

#include "private/qvi-common.h"
#include "private/qvi-logger.h"

#include "quo-vadis/qv-hwloc.h"

#include "hwloc.h"

// Type definition
struct qv_hwloc_s {
    /** The node topology. */
    hwloc_topology_t topo;
};

int
qv_hwloc_construct(
    qv_hwloc_t **hwl
) {
    if (!hwl) return QV_ERR_INVLD_ARG;

    qv_hwloc_t *ihwl = (qv_hwloc_t *)calloc(1, sizeof(*ihwl));
    if (!ihwl) {
        qvi_log_error("calloc() failed");
        return QV_ERR_OOR;
    }
    *hwl = ihwl;
    return QV_SUCCESS;
}

void
qv_hwloc_destruct(
    qv_hwloc_t *hwl
) {
    if (!hwl) return;

    hwloc_topology_destroy(hwl->topo);
    free(hwl);
}

int
qv_hwloc_init(
    qv_hwloc_t *hwl
) {
    if (!hwl) return QV_ERR_INVLD_ARG;

    char const *ers = nullptr;

    int rc = hwloc_topology_init(&hwl->topo);
    if (rc != 0) {
        ers = "hwloc_topology_init() failed";
        goto out;
    }
out:
    if (ers) {
        qvi_log_error("{} with rc={}", ers, rc);
        return QV_ERR_HWLOC;
    }
    return QV_SUCCESS;
}

int
qv_hwloc_topo_load(
    qv_hwloc_t *hwl
) {
    if (!hwl) return QV_ERR_INVLD_ARG;

    char const *ers = nullptr;

    /* Set flags that influence hwloc's behavior. */
    static const unsigned int flags = HWLOC_TOPOLOGY_FLAG_IS_THISSYSTEM;
    int rc = hwloc_topology_set_flags(hwl->topo, flags);
    if (rc != 0) {
        ers = "hwloc_topology_set_flags() failed";
        goto out;
    }

    rc = hwloc_topology_set_all_types_filter(
        hwl->topo,
        HWLOC_TYPE_FILTER_KEEP_ALL
    );
    if (rc != 0) {
        ers = "hwloc_topology_set_all_types_filter() failed";
        goto out;
    }

    rc = hwloc_topology_set_io_types_filter(
        hwl->topo,
        HWLOC_TYPE_FILTER_KEEP_IMPORTANT
    );
    if (rc != 0) {
        ers = "hwloc_topology_set_io_types_filter() failed";
        goto out;
    }

    rc = hwloc_topology_load(hwl->topo);
    if (rc != 0) {
        ers = "hwloc_topology_load() failed";
        goto out;
    }
out:
    if (ers) {
        qvi_log_error("{} with rc={}", ers, rc);
        return QV_ERR_HWLOC;
    }
    return QV_SUCCESS;
}

qv_hwloc_bitmap_t
qv_hwloc_bitmap_alloc(void)
{
    return (qv_hwloc_bitmap_t)hwloc_bitmap_alloc();
}

int
qv_hwloc_bitmap_free(
    qv_hwloc_bitmap_t bitmap
) {
    if (!bitmap) return QV_ERR_INVLD_ARG;

    hwloc_bitmap_free((hwloc_bitmap_t)bitmap);
    return QV_SUCCESS;
}

/**
 * \note Caller is responsible for freeing returned resources via
 * qv_hwloc_bitmap_free().
 */
int
qv_hwloc_task_get_cpubind(
    qv_hwloc_t *hwl,
    pid_t who,
    qv_hwloc_bitmap_t *out_bitmap
) {
    if (!hwl || !out_bitmap) return QV_ERR_INVLD_ARG;

    int qrc = QV_SUCCESS, rc = 0;

    hwloc_bitmap_t cur_bind = hwloc_bitmap_alloc();
    if (!cur_bind) {
        qvi_log_error("hwloc_bitmap_alloc() failed");
        qrc = QV_ERR_OOR;
        goto out;
    }
    // TODO(skg) Add another routine to also support getting TIDs.
    rc = hwloc_get_proc_cpubind(
            hwl->topo,
            who,
            cur_bind,
            HWLOC_CPUBIND_PROCESS
    );
    if (rc) {
        char const *ers = "hwloc_get_proc_cpubind() failed";
        qvi_log_error("{} with rc={}", ers, rc);
        qrc = QV_ERR_HWLOC;
        goto out;
    }
    *out_bitmap = (qv_hwloc_bitmap_t)cur_bind;
out:
    /* Cleanup on failure */
    if (qrc != QV_SUCCESS) {
        if (cur_bind) hwloc_bitmap_free(cur_bind);
        *out_bitmap = nullptr;
    }
    return qrc;
}

int
qv_hwloc_bitmap_snprintf(
    char *buf,
    size_t buflen,
    qv_hwloc_bitmap_t bitmap,
    int *result
) {
    *result = hwloc_bitmap_snprintf(
        buf,
        buflen,
        (hwloc_const_bitmap_t)bitmap
    );
    return QV_SUCCESS;
}

int
qv_hwloc_bitmap_asprintf(
    qv_hwloc_bitmap_t bitmap,
    char **result
) {
    if (!bitmap || !result) return QV_ERR_INVLD_ARG;

    int rc = QV_SUCCESS;
    // Caller is responsible for freeing returned resources.
    char *iresult = nullptr;
    (void)hwloc_bitmap_asprintf(&iresult, (hwloc_bitmap_t)bitmap);
    if (!iresult) {
        qvi_log_error("hwloc_bitmap_asprintf() failed");
        rc = QV_ERR_OOR;
    }
    *result = iresult;
    return rc;
}

int
qv_hwloc_bitmap_sscanf(
    qv_hwloc_bitmap_t bitmap,
    const char *string
) {
    int rc = hwloc_bitmap_sscanf((hwloc_bitmap_t)bitmap, string);
    if (rc != 0) {
        qvi_log_error("hwloc_bitmap_sscanf() failed");
        return QV_ERR_HWLOC;
    }
    return QV_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
