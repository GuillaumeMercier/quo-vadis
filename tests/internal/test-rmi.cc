/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2024 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c) 2020-2021 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file test-rmi.cc
 */


#include "quo-vadis.h"
#include "qvi-utils.h"
#include "qvi-hwloc.h"
#include "qvi-rmi.h"

static int
server(
    const char *url
) {
    printf("# [%d] Starting Server (%s)\n", getpid(), url);

    char const *ers = NULL;
    const char *basedir = qvi_tmpdir();
    char *path = nullptr;
    double start = qvi_time(), end;
    qvi_rmi_config_s config;

    qvi_rmi_server_t *server = NULL;
    int rc = qvi_rmi_server_new(&server);
    if (rc != QV_SUCCESS) {
        ers = "qvi_rmi_server_new() failed";
        goto out;
    }

    qvi_hwloc_t *hwloc;
    rc = qvi_hwloc_new(&hwloc);
    if (rc != QV_SUCCESS) {
        ers = "qvi_hwloc_new() failed";
        goto out;
    }

    rc = qvi_hwloc_topology_init(hwloc, NULL);
    if (rc != QV_SUCCESS) {
        ers = "qvi_hwloc_topology_init() failed";
        goto out;
    }

    rc = qvi_hwloc_topology_load(hwloc);
    if (rc != QV_SUCCESS) {
        ers = "qvi_hwloc_topology_load() failed";
        goto out;
    }

    config.url = std::string(url);
    config.hwloc = hwloc;

    rc = qvi_hwloc_topology_export(
        hwloc, basedir, &path
    );
    if (rc != QV_SUCCESS) {
        ers = "qvi_hwloc_topology_export() failed";
        goto out;
    }
    config.hwtopo_path = std::string(path);
    free(path);

    rc = qvi_rmi_server_config(server, &config);
    if (rc != QV_SUCCESS) {
        ers = "qvi_rmi_server_config() failed";
        goto out;
    }

    rc = qvi_rmi_server_start(server, false);
    if (rc != QV_SUCCESS) {
        ers = "qvi_rmi_server_start() failed";
        goto out;
    }
    end = qvi_time();
    printf("# [%d] Server Start Time %lf seconds\n", getpid(), end - start);
out:
    sleep(4);
    qvi_rmi_server_delete(&server);
    qvi_hwloc_delete(&hwloc);
    if (ers) {
        fprintf(stderr, "\n%s (rc=%d, %s)\n", ers, rc, qv_strerr(rc));
        return 1;
    }
    return 0;
}

static int
client(
    const char *url
) {
    printf("# [%d] Starting Client (%s)\n", getpid(), url);

    char const *ers = NULL;
    pid_t who = qvi_gettid();
    hwloc_bitmap_t bitmap = NULL;

    qvi_rmi_client_t *client = NULL;
    int rc = qvi_rmi_client_new(&client);
    if (rc != QV_SUCCESS) {
        ers = "qvi_rmi_client_new() failed";
        goto out;
    }

    rc = qvi_rmi_client_connect(client, url);
    if (rc != QV_SUCCESS) {
        ers = "qvi_rpc_client_connect() failed";
        goto out;
    }

    rc = qvi_rmi_cpubind(client, who, &bitmap);
    if (rc != QV_SUCCESS) {
        ers = "qvi_rmi_cpubind() failed";
        goto out;
    }
    char *res;
    qvi_hwloc_bitmap_asprintf(bitmap, &res);
    printf("# [%d] cpubind = %s\n", who, res);
    hwloc_bitmap_free(bitmap);
    free(res);
out:
    qvi_rmi_client_delete(&client);
    if (ers) {
        fprintf(stderr, "\n%s (rc=%d, %s)\n", ers, rc, qv_strerr(rc));
        return 1;
    }
    return 0;
}

static void
usage(const char *appn)
{
    fprintf(stderr, "Usage: %s URL -s|-c\n", appn);
}

int
main(
    int argc,
    char **argv
) {
    int rc = 0;

    setbuf(stdout, NULL);

    if (argc != 3) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    if (strcmp(argv[2], "-s") == 0) {
        rc = server(argv[1]);
    }
    else if (strcmp(argv[2], "-c") == 0) {
        rc = client(argv[1]);
    }
    else {
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    return (rc == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
