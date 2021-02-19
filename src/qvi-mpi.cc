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
 * @file qvi-mpi.cc
 */

// TODO(skg) Have infrastructure-specific init (e.g., MPI), but have
// infrastructure-agnostic free(), etc (where possible).

#include "qvi-common.h"

#include "qvi-context.h"
#include "qvi-task.h"

#include "mpi.h"

using group_tab_t = std::unordered_map<qvi_mpi_group_id_t, qvi_mpi_group_t>;

struct qvi_mpi_group_s {
    /** ID used for table lookups */
    qvi_mpi_group_id_t tabid = 0;
    /** ID (rank) in group */
    int group_id = 0;
    /** Size of group */
    int group_size = 0;
    /* Underlying MPI group */
    MPI_Group mpi_group = MPI_GROUP_NULL;
    /* Underlying MPI communicator from MPI group */
    MPI_Comm mpi_comm = MPI_COMM_NULL;
};

struct qvi_mpi_s {
    /** Task associated with this MPI process */
    qv_task_t *task = nullptr;
    /** Node size */
    int node_size = 0;
    /** World size */
    int world_size = 0;
    /** Duplicate of MPI_COMM_SELF */
    MPI_Comm self_comm = MPI_COMM_NULL;
    /** Duplicate of initializing communicator */
    MPI_Comm world_comm = MPI_COMM_NULL;
    /** Node communicator */
    MPI_Comm node_comm = MPI_COMM_NULL;
    /** Maintains the next available group ID value */
    qvi_mpi_group_id_t group_next_id = QVI_MPI_GROUP_INTRINSIC_END;
    /** Group table (ID to internal structure mapping) */
    group_tab_t *group_tab = nullptr;
};

// TODO(skg) Consider implementing a connection manager that will deal with all
// the low-level details regarding connection setup, discovery, etc.
static int
connect_to_server(
    qv_context_t *ctx
) {
    int rc = qvi_rmi_client_connect(
        ctx->rmi,
        "tcp://127.0.0.1:55995"
    );
    return rc;
}

int
qv_mpi_create(
    qv_context_t **ctx,
    MPI_Comm comm
) {
    if (!ctx) return QV_ERR_INVLD_ARG;

    int rc = QV_SUCCESS;
    char const *ers = nullptr;

    qv_context_t *ictx = qvi_new qv_context_t;
    if (!ictx) {
        ers = "memory allocation failed";
        rc = QV_ERR_OOR;
        goto out;
    }

    rc = qvi_task_construct(&ictx->task);
    if (rc != QV_SUCCESS) {
        ers = "qvi_task_construct() failed";
        goto out;
    }

    rc = qvi_hwloc_construct(&ictx->hwloc);
    if (rc != QV_SUCCESS) {
        ers = "qvi_hwloc_construct() failed";
        goto out;
    }

    rc = qvi_mpi_construct(&ictx->mpi);
    if (rc != QV_SUCCESS) {
        ers = "qvi_mpi_construct() failed";
        goto out;
    }

    rc = qvi_rmi_client_construct(&ictx->rmi);
    if (rc != QV_SUCCESS) {
        ers = "qvi_rmi_client_construct() failed";
        goto out;
    }

    rc = qvi_mpi_init(ictx->mpi, ictx->task, comm);
    if (rc != QV_SUCCESS) {
        ers = "qvi_mpi_init() failed";
        goto out;
    }

    rc = connect_to_server(ictx);
    if (rc != QV_SUCCESS) {
        ers = "connect_to_server() failed";
        goto out;
    }
out:
    if (ers) {
        qvi_log_error("{} with rc={} ({})", ers, rc, qv_strerr(rc));
        (void)qv_free(ictx);
        ictx = nullptr;
    }
    *ctx = ictx;
    return rc;
}

// TODO(skg) Move.
int
qv_free(
    qv_context_t *ctx
) {
    if (!ctx) return QV_ERR_INVLD_ARG;

    qvi_task_destruct(&ctx->task);
    qvi_mpi_destruct(&ctx->mpi);
    qvi_rmi_client_destruct(&ctx->rmi);
    delete ctx;

    return QV_SUCCESS;
}

/**
 * Copies contents of internal structure from src to dst.
 */
static void
cp_mpi_group(
    const qvi_mpi_group_t *src,
    qvi_mpi_group_t *dst
) {
    memmove(dst, src, sizeof(*src));
}

/**
 * Returns the next available group ID.
 */
static int
next_group_tab_id(
    qvi_mpi_t *mpi,
    qvi_mpi_group_id_t *gid
) {
    if (mpi->group_next_id == UINT64_MAX) {
        qvi_log_error("qvi_mpi_group ID space exhausted");
        return QV_ERR_OOR;
    }
    *gid = mpi->group_next_id++;
    return QV_SUCCESS;
}

/**
 * Creates 'node' communicators from an arbitrary MPI communicator.
 */
static int
mpi_comm_to_new_node_comm(
    MPI_Comm comm,
    MPI_Comm *node_comm
) {
    int rc = MPI_Comm_split_type(
        comm,
        MPI_COMM_TYPE_SHARED,
        0,
        MPI_INFO_NULL,
        node_comm
    );
    if (rc != MPI_SUCCESS) {
        qvi_log_error("MPI_Comm_split_type(MPI_COMM_TYPE_SHARED) failed");
    }
    return rc;
}

/**
 * Creates a QV group from an MPI communicator.
 */
static int
group_create_from_mpi_comm(
    MPI_Comm comm,
    qvi_mpi_group_t *new_group
) {
    char const *ers = nullptr;

    new_group->mpi_comm = comm;

    int rc = MPI_Comm_group(
        new_group->mpi_comm,
        &new_group->mpi_group
    );
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_group() failed";
        goto out;
    }
    rc = MPI_Group_rank(
        new_group->mpi_group,
        &new_group->group_id
    );
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Group_rank() failed";
        goto out;
    }
    rc = MPI_Group_size(
        new_group->mpi_group,
        &new_group->group_size
    );
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Group_size() failed";
        goto out;
    }
out:
    if (ers) {
        qvi_log_error(ers);
        return QV_ERR_MPI;
    }
    return QV_SUCCESS;
}

/**
 * Creates a QV group from an MPI group.
 */
static int
group_create_from_mpi_group(
    qvi_mpi_t *mpi,
    MPI_Group group,
    qvi_mpi_group_t **maybe_group
) {
    char const *ers = nullptr;

    int qvrc = QV_SUCCESS;

    MPI_Comm group_comm;
    int rc = MPI_Comm_create_group(
        mpi->node_comm,
        group,
        0,
        &group_comm
    );
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_create_group() failed";
        qvrc = QV_ERR_MPI;
        goto out;
    }
    // Not in the group, so return NULL and bail.
    if (group_comm == MPI_COMM_NULL) {
        *maybe_group = nullptr;
        return QV_SUCCESS;
    }
    // In the group.
    qvrc = qvi_mpi_group_construct(maybe_group);
    if (qvrc != QV_SUCCESS) {
        ers = "qvi_mpi_group_construct() failed";
        goto out;
    }
    qvrc = group_create_from_mpi_comm(
        group_comm,
        *maybe_group
    );
    if (qvrc != QV_SUCCESS) {
        ers = "group_create_from_mpi_comm() failed";
        goto out;
    }
out:
    if (ers) {
        qvi_log_error(ers);
    }
    return qvrc;
}

/**
 *
 */
static int
group_add(
    qvi_mpi_t *mpi,
    qvi_mpi_group_t *group,
    qvi_mpi_group_id_t given_id = QVI_MPI_GROUP_NULL
) {
    qvi_mpi_group_id_t gtid;
    // Marker used to differentiate between intrinsic and automatic IDs.
    if (given_id != QVI_MPI_GROUP_NULL) {
        gtid = given_id;
    }
    else {
        int rc = next_group_tab_id(mpi, &gtid);
        if (rc != QV_SUCCESS) return rc;
    }
    group->tabid = gtid;
    mpi->group_tab->insert({gtid, *group});
    return QV_SUCCESS;
}

int
qvi_mpi_construct(
    qvi_mpi_t **mpi
) {
    int rc = QV_SUCCESS;
    char const *ers = nullptr;

    qvi_mpi_t *impi = qvi_new qvi_mpi_t;
    if (!impi) {
        ers = "memory allocation failed";
        rc = QV_ERR_OOR;
        goto out;
    }
    // Task
    // We don't own task memory. Set in qvi_mpi_init().
    // Groups
    impi->group_tab = qvi_new group_tab_t;
    if (!impi->group_tab) {
        ers = "memory allocation failed";
        rc = QV_ERR_OOR;
        goto out;
    }
out:
    if (ers) {
        qvi_log_error(ers);
        qvi_mpi_destruct(&impi);
    }
    *mpi = impi;
    return rc;
}

void
qvi_mpi_destruct(
    qvi_mpi_t **mpi
) {
    qvi_mpi_t *impi = *mpi;
    if (!impi) return;
    if (impi->group_tab) {
        for (auto &i : *impi->group_tab) {
            auto &mpi_group = i.second.mpi_group;
            if (mpi_group != MPI_GROUP_NULL) {
                MPI_Group_free(&mpi_group);
            }
            auto &mpi_comm = i.second.mpi_comm;
            if (mpi_comm != MPI_COMM_NULL) {
                MPI_Comm_free(&mpi_comm);
            }
        }
        delete impi->group_tab;
    }
    if (impi->world_comm != MPI_COMM_NULL) {
        MPI_Comm_free(&impi->world_comm);
    }
    delete impi;
    *mpi = nullptr;
}

/**
 * Creates the node communicator. Returns the MPI status.
 */
static int
create_intrinsic_comms(
    qvi_mpi_t *mpi,
    MPI_Comm comm
) {
    char const *ers = nullptr;
    // MPI_COMM_SELF duplicate
    int rc = MPI_Comm_dup(
        MPI_COMM_SELF,
        &mpi->self_comm
    );
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_dup(MPI_COMM_SELF) failed";
        goto out;
    }
    // 'World' (aka initializing communicator) duplicate
    rc = MPI_Comm_dup(
        comm,
        &mpi->world_comm
    );
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_dup() failed";
        goto out;
    }
    // Node communicator
    rc = mpi_comm_to_new_node_comm(comm, &mpi->node_comm);
    if (rc != MPI_SUCCESS) {
        ers = "mpi_comm_to_new_node_comm() failed";
        goto out;
    }
out:
    if (ers) {
        qvi_log_error(ers);
    }
    return rc;
}

static int
create_intrinsic_groups(
    qvi_mpi_t *mpi
) {
    char const *ers = nullptr;

    qvi_mpi_group_t self_group, node_group;
    int rc = group_create_from_mpi_comm(
        mpi->self_comm,
        &self_group
    );
    if (rc != QV_SUCCESS) {
        ers = "group_create_from_mpi_comm(self_comm) failed";
        goto out;
    }
    rc = group_create_from_mpi_comm(
        mpi->node_comm,
        &node_group
    );
    if (rc != QV_SUCCESS) {
        ers = "group_create_from_mpi_comm(node_comm) failed";
        goto out;
    }
    rc = group_add(mpi, &self_group, QVI_MPI_GROUP_SELF);
    if (rc != QV_SUCCESS) {
        ers = "group_add(self) failed";
        goto out;
    }
    rc = group_add(mpi, &node_group, QVI_MPI_GROUP_NODE);
    if (rc != QV_SUCCESS) {
        ers = "group_add(node) failed";
        goto out;
    }
out:
    if (ers) {
        qvi_log_error(ers);
    }
    return rc;
}

int
qvi_mpi_init(
    qvi_mpi_t *mpi,
    qv_task_t *task,
    MPI_Comm comm
) {
    char const *ers = nullptr;
    int inited;
    // If MPI isn't initialized, then we can't continue.
    int rc = MPI_Initialized(&inited);
    if (rc != MPI_SUCCESS) return QV_ERR_MPI;
    if (!inited) {
        ers = "MPI is not initialized. Cannot continue.";
        qvi_log_error(ers);
        return QV_ERR_MPI;
    }
    // MPI is initialized.
    rc = MPI_Comm_size(comm, &mpi->world_size);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_size() failed";
        goto out;
    }
    int world_id;
    rc = MPI_Comm_rank(comm, &world_id);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_rank() failed";
        goto out;
    }
    rc = create_intrinsic_comms(mpi, comm);
    if (rc != MPI_SUCCESS) {
        ers = "create_intrinsic_comms() failed";
        goto out;
    }
    rc = MPI_Comm_size(mpi->node_comm, &mpi->node_size);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_size(node_comm) failed";
        goto out;
    }
    int node_id;
    rc = MPI_Comm_rank(mpi->node_comm, &node_id);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_rank(node_comm) failed";
        goto out;
    }
    rc = create_intrinsic_groups(mpi);
    if (rc != MPI_SUCCESS) {
        ers = "create_intrinsic_groups() failed";
        goto out;
    }
    // Task initialization
    rc = qvi_task_init(task, getpid(), world_id, node_id);
    if (rc != QV_SUCCESS) {
        ers = "qvi_task_init() failed";
        qvi_log_error("{} with rc={} ({})", ers, rc, qv_strerr(rc));
        return rc;
    }
    mpi->task = task;
out:
    if (ers) {
        qvi_log_error("{} with rc={}", ers, rc);
        return QV_ERR_MPI;
    }
    return QV_SUCCESS;
}

int
qvi_mpi_finalize(
    qvi_mpi_t *mpi
) {
    QVI_UNUSED(mpi);
    return QV_SUCCESS;
}

int
qvi_mpi_group_construct(
    qvi_mpi_group_t **group
) {
    int rc = QV_SUCCESS;

    qvi_mpi_group_t *igroup = qvi_new qvi_mpi_group_t;
    if (!igroup) {
        qvi_log_error("memory allocation failed");
        rc = QV_ERR_OOR;
    }
    *group = igroup;
    return rc;
}

void
qvi_mpi_group_destruct(
    qvi_mpi_group_t **group
) {
    qvi_mpi_group_t *igroup = *group;
    if (!igroup) return;
    delete igroup;
    *group = nullptr;
}

int
qvi_mpi_group_size(
    qvi_mpi_t *,
    const qvi_mpi_group_t *group,
    int *size
) {
    *size = group->group_size;
    return QV_SUCCESS;
}

int
qvi_mpi_group_id(
    qvi_mpi_t *,
    const qvi_mpi_group_t *group,
    int *id
) {
    *id = group->group_id;
    return QV_SUCCESS;
}

int
qvi_mpi_group_lookup_by_id(
    qvi_mpi_t *mpi,
    qvi_mpi_group_id_t id,
    qvi_mpi_group_t *group
) {
    auto got = mpi->group_tab->find(id);
    if (got == mpi->group_tab->end()) {
        return QV_ERR_NOT_FOUND;
    }
    cp_mpi_group(&got->second, group);
    return QV_SUCCESS;
}

int
qvi_mpi_group_create_from_ids(
    qvi_mpi_t *mpi,
    const qvi_mpi_group_t *group,
    int num_group_ids,
    const int *group_ids,
    qvi_mpi_group_t **maybe_group
) {
    char const *ers = nullptr;
    int qvrc = QV_SUCCESS;

    MPI_Group new_mpi_group;
    int rc = MPI_Group_incl(
        group->mpi_group,
        num_group_ids,
        group_ids,
        &new_mpi_group
    );
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Group_incl() failed";
        qvrc = QV_ERR_MPI;
        goto out;
    }
    qvrc = group_create_from_mpi_group(
        mpi,
        new_mpi_group,
        maybe_group
    );
    if (qvrc != QV_SUCCESS) {
        ers = "group_create_from_mpi_group() failed";
        goto out;
    }
    // Not in the group and no errors.
    if (*maybe_group == nullptr) {
        return QV_SUCCESS;
    }
    // In the group.
    qvrc = group_add(mpi, *maybe_group);
    if (qvrc != QV_SUCCESS) {
        ers = "group_add() failed";
        goto out;
    }
out:
    if (ers) {
        qvi_log_error(ers);
    }
    return qvrc;
}

int
qvi_mpi_group_create_from_mpi_comm(
    qvi_mpi_t *mpi,
    MPI_Comm comm,
    qvi_mpi_group_t **new_group
) {
    char const *ers = nullptr;
    MPI_Comm node_comm = MPI_COMM_NULL;

    int rc = qvi_mpi_group_construct(new_group);
    if (rc != QV_SUCCESS) {
        ers = "qvi_mpi_group_construct() failed";
        goto out;
    }
    rc = mpi_comm_to_new_node_comm(comm, &node_comm);
    if (rc != MPI_SUCCESS) {
        ers = "mpi_comm_to_new_node_comm() failed";
        rc = QV_ERR_MPI;
        goto out;
    }
    rc = group_create_from_mpi_comm(
        node_comm,
        *new_group
    );
    if (rc != QV_SUCCESS) {
        ers = "group_create_from_mpi_comm() failed";
        goto out;
    }
    rc = group_add(mpi, *new_group);
    if (rc != QV_SUCCESS) {
        ers = "group_add() failed";
        goto out;
    }
out:
    if (ers) {
        qvi_mpi_group_destruct(new_group);
        if (node_comm != MPI_COMM_NULL) {
            MPI_Comm_free(&node_comm);
        }
    }
    return rc;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
