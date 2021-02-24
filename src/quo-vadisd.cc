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
 * @file quo-vadisd.cc
 */

#include "qvi-common.h"
#include "qvi-rmi.h"
#include "qvi-utils.h"

struct context {
    qvi_rmi_server_t *rmiserv;
};

#if 0
static void
closefds(void)
{
    qvi_syslog_debug("Entered {}", __func__);
    // Determine the max number of file descriptors.
    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) < 0) {
        static const char *ers = "Cannot determine RLIMIT_NOFILE";
        const int err = errno;
        qvi_panic_syslog_error("{} (rc={}, {})", ers, err, qvi_strerr(err));
    }
    // Default: no limit on this resource, so pick one.
    int64_t maxfd = 1024;
    if (rl.rlim_max != RLIM_INFINITY) {
        // Not RLIM_INFINITY, so set to resource limit.
        maxfd = (int64_t)rl.rlim_max;
    }
    // Close all the file descriptors.
    for (int64_t fd = 0; fd < maxfd; ++fd) {
        (void)close(fd);
    }
}
#endif

static void
become_session_leader(void)
{
    qvi_syslog_debug("Entered {}", __func__);

    pid_t pid = 0;
    if ((pid = fork()) < 0) {
        static const char *ers = "fork() failed";
        const int err = errno;
        qvi_panic_syslog_error("{} (rc={}, {})", ers, err, qvi_strerr(err));
    }
    // Parent
    if (pid != 0) {
        // _exit(2) used to match daemon(3) behavior.
        _exit(EXIT_SUCCESS);
    }
    // Child
    pid_t pgid = setsid();
    if (pgid < 0) {
        static const char *ers = "setsid() failed";
        const int err = errno;
        qvi_panic_syslog_error("{} (rc={}, {})", ers, err, qvi_strerr(err));
    }
}

#if 0
        ers = "The following environment variable is not set: QV_PORT.\n"
              "Please set to an unused port number.\n";
static int
#endif

static void
start_rmi(
    context *ctx
) {
    qvi_syslog_debug("Entered {}", __func__);

    cstr ers = nullptr;

    // TODO(skg) Improve.
    char *url = nullptr;
    int rc = qvi_url(&url);
    if (rc != QV_SUCCESS) {
        ers = "qvi_url() failed";
        goto out;
    }

    rc = qvi_rmi_server_construct(&ctx->rmiserv);
    if (rc != QV_SUCCESS) {
        ers = "qvi_rmi_server_construct() failed";
        goto out;
    }

    rc = qvi_rmi_server_start(ctx->rmiserv, url);
    if (rc != QV_SUCCESS) {
        ers = "qvi_rmi_server_start() failed";
        goto out;
    }
    // TODO(skg) Add flags option
out:
    if (ers) {
        qvi_panic_syslog_error("{} (rc={}, {})", ers, rc, qv_strerr(rc));
    }
    if (url) free(url);
}

static void
main_loop(
    context *
) {
    qvi_syslog_debug("Entered {}", __func__);
}

// TODO(skg) Add daemonize option.
int
main(
    int,
    char **
) {
    qvi_syslog_debug("Entered {}", __func__);
    //
    context ctx;
    // Clear umask. Note: this system call always succeeds.
    umask(0);
    // Become a session leader to lose controlling TTY.
    become_session_leader();
    // Close all file descriptors.
    //closefds();
    // Gather hardware information.
    start_rmi(&ctx);
    // Enter the main processing loop.
    main_loop(&ctx);

    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
