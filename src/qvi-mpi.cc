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
 * @file qvi-mpi.cc
 */

#include "private/qvi-common.h"
#include "private/qvi-mpi.h"
#include "private/qvi-task.h"
#include "private/qvi-log.h"

#include <new>
#include <unordered_map>

using group_tab_id_t = uint64_t;
using group_tab_t = std::unordered_map<group_tab_id_t, qvi_mpi_group_t>;

struct qvi_mpi_group_s {
    /** ID used for table lookups */
    group_tab_id_t tabid = 0;
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
    /** Duplicate of MPI_COMM_WORLD */
    MPI_Comm world_comm = MPI_COMM_NULL;
    /** Node communicator */
    MPI_Comm node_comm = MPI_COMM_NULL;
    /** Maintains the next available group ID value */
    group_tab_id_t group_next_id = QVI_MPI_GROUP_INTRINSIC_END;
    /** Group table (ID to internal structure mapping) */
    group_tab_t *group_tab = nullptr;
};

/**
 * Returns the next available group ID.
 */
static int
next_group_tab_id(
    qvi_mpi_t *mpi,
    group_tab_id_t *gid
) {
    if (mpi->group_next_id == UINT64_MAX) {
        qvi_log_error("qvi_mpi_group ID space exhausted");
        return QV_ERR_OOR;
    }
    *gid = mpi->group_next_id++;
    return QV_SUCCESS;
}

/**
 * Creates a QV group from an MPI communicator.
 */
static int
group_create_from_comm(
    MPI_Comm comm,
    qvi_mpi_group_t *new_group
) {
    char const *ers = nullptr;

    int rc = MPI_Comm_group(
        comm,
        &new_group->mpi_group
    );
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_group() failed";
        goto out;
    }
    rc = MPI_Comm_rank(
        comm,
        &new_group->group_id
    );
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_rank() failed";
        goto out;
    }
    rc = MPI_Comm_size(
        comm,
        &new_group->group_size
    );
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_size() failed";
        goto out;
    }
    new_group->mpi_comm = comm;
out:
    if (ers) {
        qvi_log_error(ers);
    }
    return rc;
}

/**
 *
 */
static int
group_add(
    qvi_mpi_t *mpi,
    qvi_mpi_group_t *group,
    group_tab_id_t given_id = QVI_MPI_GROUP_NULL
) {
    group_tab_id_t gtid;
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
    if (!mpi) return QV_ERR_INVLD_ARG;

    int rc = QV_SUCCESS;
    char const *ers = nullptr;

    qvi_mpi_t *impi = qvi_new qvi_mpi_t;
    if (!impi) {
        ers = "memory allocation failed";
        rc = QV_ERR_OOR;
        goto out;
    }
    // Task
    rc = qvi_task_construct(&impi->task);
    if (rc != QV_SUCCESS) {
        ers = "qvi_task_construct() failed";
        goto out;
    }
    // Groups
    impi->group_tab = qvi_new group_tab_t;
    if (!impi->group_tab) {
        ers = "memory allocation failed";
        rc = QV_ERR_OOR;
        goto out;
    }
    *mpi = impi;
out:
    if (ers) {
        qvi_log_error(ers);
        qvi_mpi_destruct(impi);
    }
    return rc;
}

void
qvi_mpi_destruct(
    qvi_mpi_t *mpi
) {
    if (!mpi) return;
    if (mpi->group_tab) {
        for (auto &i : *mpi->group_tab) {
            auto &mpi_group = i.second.mpi_group;
            if (mpi_group != MPI_GROUP_NULL) {
                MPI_Group_free(&mpi_group);
            }
            auto &mpi_comm = i.second.mpi_comm;
            if (mpi_comm != MPI_COMM_NULL) {
                MPI_Comm_free(&mpi_comm);
            }
        }
        delete mpi->group_tab;
    }
    if (mpi->world_comm != MPI_COMM_NULL) {
        MPI_Comm_free(&mpi->world_comm);
    }
    qvi_task_destruct(mpi->task);
    delete mpi;
}

/**
 * Creates the node communicator. Returns the MPI status.
 */
static int
create_intrinsic_comms(
    qvi_mpi_t *mpi,
    MPI_Comm comm
) {
    if (!mpi) return QV_ERR_INVLD_ARG;

    int rc;
    // MPI_COMM_SELF duplicate
    rc = MPI_Comm_dup(
        MPI_COMM_SELF,
        &mpi->self_comm
    );
    if (rc != MPI_SUCCESS) {
        qvi_log_error("MPI_Comm_dup(MPI_COMM_SELF) failed");
        return rc;
    }
    // MPI_COMM_WORLD duplicate
    rc = MPI_Comm_dup(
        MPI_COMM_WORLD,
        &mpi->world_comm
    );
    if (rc != MPI_SUCCESS) {
        qvi_log_error("MPI_Comm_dup(MPI_COMM_WORLD) failed");
        return rc;
    }
    // Node communicator
    rc = MPI_Comm_split_type(
        comm,
        MPI_COMM_TYPE_SHARED,
        0,
        MPI_INFO_NULL,
        &mpi->node_comm
    );
    if (rc != MPI_SUCCESS) {
        qvi_log_error("MPI_Comm_split_type(MPI_COMM_TYPE_SHARED) failed");
        return rc;
    }
    return rc;
}

static int
create_intrinsic_groups(
    qvi_mpi_t *mpi
) {
    char const *ers = nullptr;

    qvi_mpi_group_t self_group, node_group;

    int rc = group_create_from_comm(mpi->self_comm, &self_group);
    if (rc != MPI_SUCCESS) {
        ers = "group_create_from_comm(self_comm) failed";
        goto out;
    }
    rc = group_create_from_comm(mpi->node_comm, &node_group);
    if (rc != MPI_SUCCESS) {
        ers = "group_create_from_comm(node_comm) failed";
        goto out;
    }
    group_add(mpi, &self_group, QVI_MPI_GROUP_SELF);
    group_add(mpi, &node_group, QVI_MPI_GROUP_NODE);
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
    if (!mpi || !task) return QV_ERR_INVLD_ARG;

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
        ers = "MPI_Comm_size(MPI_COMM_WORLD) failed";
        goto out;
    }
    int world_id;
    rc = MPI_Comm_rank(comm, &world_id);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_rank(MPI_COMM_WORLD) failed";
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
    // Task initialisation
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
qvi_mpi_node_id(
    qvi_mpi_t *mpi
) {
    return qv_task_id(mpi->task);
}

int
qvi_mpi_world_id(
    qvi_mpi_t *mpi
) {
    qv_task_gid_t gid = qv_task_gid(mpi->task);
    return (int)gid;
}

int
qvi_mpi_node_size(
    qvi_mpi_t *mpi
) {
    return mpi->node_size;
}

int
qvi_mpi_world_size(
    qvi_mpi_t *mpi
) {
    return mpi->world_size;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
