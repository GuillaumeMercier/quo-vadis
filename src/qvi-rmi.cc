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
 * @file qvi-rmi.cc
 *
 * Resource Management and Inquiry
 */

#include "private/qvi-common.h"
#include "private/qvi-rmi.h"
#include "private/qvi-log.h"

struct qvi_rmi_server_s {
    qvi_rpc_server_t *rpcserv = nullptr;
};

struct qvi_rmi_client_s {
    qvi_rpc_client_t *rcpcli = nullptr;
};

int
qvi_rmi_server_construct(
    qvi_rmi_server_t **server
) {
    int rc = QV_SUCCESS;
    char const *ers = nullptr;

    qvi_rmi_server_t *iserver = qvi_new qvi_rmi_server_t;
    if (!iserver) {
        qvi_log_error("memory allocation failed");
        return QV_ERR_OOR;
    }

    rc = qvi_rpc_server_construct(&iserver->rpcserv);
    if (rc != QV_SUCCESS) {
        ers = "qvi_rpc_server_construct() failed";
        goto out;
    }
out:
    if (ers) {
        qvi_log_error("{} with rc={} ({})", ers, rc, qv_strerr(rc));
        qvi_rmi_server_destruct(&iserver);
    }
    *server = iserver;
    return rc;
}

void
qvi_rmi_server_destruct(
    qvi_rmi_server_t **server
) {
    qvi_rmi_server_t *iserver = *server;
    if (!iserver) return;

    qvi_rpc_server_destruct(&iserver->rpcserv);
    delete iserver;
    *server = nullptr;
}

int
qvi_rmi_server_start(
    qvi_rmi_server_t *server,
    const char *url
) {
    if (!server) return QV_ERR_INVLD_ARG;

    int rc = QV_SUCCESS;
    char const *ers = nullptr;

    rc = qvi_rpc_server_start(server->rpcserv, url);
    if (rc != QV_SUCCESS) {
        ers = "qvi_rpc_server_start() failed";
        goto out;
    }
out:
    if (ers) {
        qvi_log_error("{} with rc={} ({})", ers, rc, qv_strerr(rc));
    }
    return rc;
}

int
qvi_rmi_client_construct(
    qvi_rmi_client_t **client
) {
    if (!client) return QV_ERR_INVLD_ARG;

    int rc = QV_SUCCESS;
    char const *ers = nullptr;

    qvi_rmi_client_t *icli = qvi_new qvi_rmi_client_t;
    if (!icli) {
        qvi_log_error("memory allocation failed");
        return QV_ERR_OOR;
    }

    rc = qvi_rpc_client_construct(&icli->rcpcli);
    if (rc != QV_SUCCESS) {
        ers = "qvi_rpc_client_construct() failed";
        goto out;
    }
out:
    if (ers) {
        qvi_log_error("{} with rc={} ({})", ers, rc, qv_strerr(rc));
        qvi_rmi_client_destruct(&icli);
    }
    *client = icli;
    return rc;
}

void
qvi_rmi_client_destruct(
    qvi_rmi_client_t **client
) {
    qvi_rmi_client_t *iclient = *client;
    if (!iclient) return;
    qvi_rpc_client_destruct(&iclient->rcpcli);
    delete iclient;
    *client = nullptr;
}

int
qvi_rmi_client_connect(
    qvi_rmi_client_t *client,
    const char *url
) {
    return qvi_rpc_client_connect(client->rcpcli, url);
}

int
qvi_rmi_task_get_cpubind(
    qvi_rmi_client_t *client,
    pid_t who,
    hwloc_bitmap_t *out_bitmap
) {
    if (!client) return QV_ERR_INVLD_ARG;

    int rc = QV_SUCCESS;
    char const *ers = nullptr;

    hwloc_bitmap_t bitmap = hwloc_bitmap_alloc();

    qvi_rpc_argv_t args = 0;
    qvi_rpc_argv_pack(&args, who, out_bitmap);

    rc = qvi_rpc_client_req(
        client->rcpcli,
        QV_TASK_GET_CPUBIND,
        args,
        who,
        out_bitmap
    );
    if (rc != QV_SUCCESS) {
        ers = "qvi_rpc_client_req() failed";
        goto out;
    }

    qvi_rpc_fun_data_t fun_data;
    rc = qvi_rpc_client_rep(
        client->rcpcli,
        &fun_data
    );
    if (rc != QV_SUCCESS) {
        ers = "qvi_rpc_client_rep() failed";
        goto out;
    }
    hwloc_bitmap_sscanf(bitmap, fun_data.bitm_args[0]);
    *out_bitmap = bitmap;
out:
    if (ers) {
        qvi_log_error("{} with rc={} ({})", ers, rc, qv_strerr(rc));
    }
    return rc;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 *
 */
