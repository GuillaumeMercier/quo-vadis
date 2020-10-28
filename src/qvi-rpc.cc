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
 * @file qvi-rpc.cc
 */

// TODO(skg)
// * Bind the server to some subset of hardware resources because it
//   spawns threads.
// * Add message magic to front/back of message body and verify entire message
//   was sent successfully.

#include "private/qvi-common.h"
#include "private/qvi-rpc.h"
#include "private/qvi-logger.h"

#include "hwloc.h"
#include "nng/supplemental/util/platform.h"

#include <stdarg.h>

// This should be more than plenty for our use case.
#define URL_MAX_LEN 512

/** Message type definition. */
typedef struct qvi_rpc_wqi_s {
    enum {
        INIT,
        RECV,
        SEND
    } state;
    nng_aio *aio;
    nng_socket sock;
    nng_msg *msg;
    //
    qv_hwloc_t *hwloc;
} qvi_rpc_wqi_t;

struct qvi_rpc_server_s {
    char url[URL_MAX_LEN];
    nng_socket sock;
    uint16_t qdepth;
    qvi_rpc_wqi_t **wqis;
    qv_hwloc_t *hwloc;
};

struct qvi_rpc_client_s {
    char url[URL_MAX_LEN];
    nng_socket sock;
};

typedef struct qvi_msg_header_s {
    qvi_rpc_funid_t funid;
    qvi_rpc_argv_t argv;
} qvi_msg_header_t;

typedef int (*qvi_rpc_fun_ptr_t)(qvi_rpc_fun_args_s *);

////////////////////////////////////////////////////////////////////////////////
// Forward declarations
////////////////////////////////////////////////////////////////////////////////
static void
server_msg_state_machine(
    void *data
);

static inline void
server_msg_state_recv(
    qvi_rpc_wqi_t *wqi
);

////////////////////////////////////////////////////////////////////////////////
// Server-Side RPC Stub Definitions
////////////////////////////////////////////////////////////////////////////////
static int
rpc_stub_task_get_cpubind(
    qvi_rpc_fun_args_t *args
) {
    qv_hwloc_bitmap_t bitmap;

    int rc = qv_hwloc_task_get_cpubind(
        args->hwloc,
        args->int_args[0],
        &bitmap
    );
    if (rc != QV_SUCCESS) {
        static char const *ers = "qv_hwloc_task_get_cpubind() failed";
        QVI_LOG_ERROR("{} with rc={} ({})", ers, rc, qv_strerr(rc));
        qv_hwloc_bitmap_free(bitmap);
        return QV_ERR_RPC;
    }
    hwloc_bitmap_snprintf(args->bitm_args[0], sizeof(args->bitm_args[0]), (hwloc_bitmap_t)bitmap);
    qv_hwloc_bitmap_free(bitmap);
    return QV_SUCCESS;
}

/**
 * Maps a given qvi_rpc_funid_t to a given function pointer. Must be kept in
 * sync with qvi_rpc_funid_t.
 */
static const qvi_rpc_fun_ptr_t qvi_server_rpc_dispatch_table[] = {
    rpc_stub_task_get_cpubind
};

/**
 *
 */
static void
register_atexit(void)
{
    // We want exactly one atexit handler installed per process.
    static bool handler_installed = false;
    if (!handler_installed) {
        const int rc = atexit(nng_fini);
        if (rc) {
            QVI_LOG_WARN("atexit(nng_fini) failed");
        }
        handler_installed = true;
    }
}

/**
 *
 */
static inline int
msg_append_header(
    nng_msg *msg,
    qvi_msg_header_t *hdr
) {
    const int rc = nng_msg_append(msg, hdr, sizeof(*hdr));
    if (rc != 0) {
        char const *ers = "nng_msg_append() failed";
        QVI_LOG_ERROR("{} with rc={} ({})", ers, rc, nng_strerror(rc));
        return QV_ERR_MSG;
    }
    return QV_SUCCESS;
}

/**
 *
 */
static inline int
rpc_pack_msg_prep(
    nng_msg **msg,
    const qvi_rpc_funid_t funid,
    const qvi_rpc_argv_t argv
) {
    int rc = nng_msg_alloc(msg, 0);
    if (rc != 0) {
        QVI_LOG_ERROR("nng_msg_alloc() failed");
        return QV_ERR_MSG;
    }

    qvi_msg_header_t msghdr = {funid, argv};
    rc = msg_append_header(*msg, &msghdr);
    if (rc != QV_SUCCESS) {
        nng_msg_free(*msg);
        *msg = nullptr;
        return rc;
    }
    return QV_SUCCESS;
}

/**
 *
 */
static int
client_rpc_pack(
    nng_msg **msg,
    const qvi_rpc_funid_t funid,
    const qvi_rpc_argv_t argv,
    va_list args
) {
    char const *ers = nullptr;

    nng_msg *imsg = nullptr;
    int rc = rpc_pack_msg_prep(&imsg, funid, argv);
    if (rc != QV_SUCCESS) {
        QVI_LOG_ERROR("rpc_pack_msg_prep() failed");
        return rc;
    }
    //
    const size_t nargs = qvi_rpc_args_maxn();
    const size_t tbits = qvi_rpc_type_nbits();
    // Flag indicating whether or not we are done processing arguments.
    bool done = false;
    // We will need to manipulate the argument list contents, so copy it.
    qvi_rpc_argv_t argvc = argv;
    // Process each argument and store them into the message body in the order
    // in which they were specified.
    for (size_t argidx = 0; argidx < nargs && !done; ++argidx) {
        const qvi_rpc_arg_type_t type = (qvi_rpc_arg_type_t)(
            argvc & rpc_argv_type_mask
        );
        switch (type) {
            // The values are packed contiguously, so we have reached the end.
            case QVI_RPC_TYPE_NONE: {
                done = true;
                break;
            }
            case QVI_RPC_TYPE_INT: {
                const int value = va_arg(args, int);
                if (nng_msg_append(imsg, &value, sizeof(value)) != 0) {
                    ers = "QVI_RPC_TYPE_INT: nng_msg_append() failed";
                    rc = QV_ERR_MSG;
                    goto out;
                }
                break;
            }
            case QVI_RPC_TYPE_CSTR: {
                char *value = va_arg(args, char *);
                if (nng_msg_append(imsg, value, strlen(value) + 1) != 0) {
                    ers = "QVI_RPC_TYPE_CSTR: nng_msg_append() failed";
                    rc = QV_ERR_MSG;
                    goto out;
                }
                break;
            }
            case QVI_RPC_TYPE_BITM: {
                // TODO(skg) Make sure this is the correct type
                qv_hwloc_bitmap_t *value = va_arg(args, qv_hwloc_bitmap_t *);
                QVI_UNUSED(value);
                break;
            }
            default:
                ers = "Unrecognized RPC type";
                rc = QV_ERR_INTERNAL;
                goto out;
        }
        // Advance argument bits to process next argument.
        argvc = argvc >> tbits;
    }
out:
    if (ers) {
        QVI_LOG_ERROR("{}", ers);
        nng_msg_free(imsg);
        *msg = nullptr;
        return rc;
    }
    *msg = imsg;
    return QV_SUCCESS;
}

/**
 *
 */
static inline size_t
rpc_unpack_msg_header(
    nng_msg *msg,
    qvi_msg_header_t *hdr
) {
    const size_t hdrsize = sizeof(*hdr);
    memmove(hdr, nng_msg_body(msg), hdrsize);
    return hdrsize;
}

/**
 *
 */
static int
server_rpc_unpack(
    nng_msg *msg,
    const qvi_msg_header_t msghdr,
    qvi_rpc_fun_args_t *args
) {
    int rc = QV_SUCCESS;
    char const *ers = nullptr;
    // Get pointer to start of message body.
    uint8_t *bodyp = (uint8_t *)nng_msg_body(msg);
    //
    const size_t nargs = qvi_rpc_args_maxn();
    const size_t tbits = qvi_rpc_type_nbits();
    // Flag indicating whether or not we are done processing arguments.
    bool done = false;
    // Unpack the values in the message body and 'return' them to the caller by
    // reference. Notice during the unpacking process, namely during va_arg(),
    // we add an extra level of indirection to the decoded type. For example,
    // QVI_RPC_TYPE_INT will expect an int * as an argument, not an int.
    qvi_rpc_argv_t argv = msghdr.argv;
    for (size_t argidx = 0; argidx < nargs && !done; ++argidx) {
        const qvi_rpc_arg_type_t type = (qvi_rpc_arg_type_t)(
            argv & rpc_argv_type_mask
        );
        switch (type) {
            case QVI_RPC_TYPE_NONE: {
                done = true;
                break;
            }
            case QVI_RPC_TYPE_INT: {
                int *arg = &args->int_args[args->int_i++];
                memmove(arg, bodyp, sizeof(*arg));
                bodyp += sizeof(*arg);
                break;
            }
            case QVI_RPC_TYPE_CSTR: {
                char const *cstr = (char const *)bodyp;
                const int bufsize = snprintf(NULL, 0, "%s", cstr) + 1;
                char *value = (char *)calloc(bufsize, sizeof(*value));
                if (!value) {
                    ers = "calloc() failed";
                    rc = QV_ERR_OOR;
                    goto out;
                }
                memmove(value, cstr, bufsize);
                args->cstr_args[args->cstr_i++] = value;
                bodyp += bufsize;
                break;
            }
            case QVI_RPC_TYPE_BITM: {
                break;
            }
            default:
                ers = "Unrecognized RPC type";
                rc = QV_ERR_INTERNAL;
                goto out;
        }
        // Advance argument bits to process next argument.
        argv = argv >> tbits;
    }
out:
    if (ers) {
        QVI_LOG_ERROR("{}", ers);
        return rc;
    }
    return QV_SUCCESS;
}

/**
 *
 */
static int
client_connect(
    qvi_rpc_client_t *client,
    const char *url
) {
    char const *ers = nullptr;

    int rc = nng_req0_open(&client->sock);
    if (rc != 0) {
        ers = "nng_req0_open() failed";
        goto out;
    }

    rc = nng_dial(client->sock, url, nullptr, 0);
    if (rc != 0) {
        ers = "nng_dial() failed";
        goto out;
    }
out:
    if (ers) {
        QVI_LOG_ERROR("{} with rc={} ({})", ers, rc, nng_strerror(rc));
        nng_close(client->sock);
        return QV_ERR_MSG;
    }
    return QV_SUCCESS;
}

int
qvi_rpc_client_req(
    qvi_rpc_client_t *client,
    const qvi_rpc_funid_t funid,
    const qvi_rpc_argv_t argv,
    ...
) {
    va_list vl;
    va_start(vl, argv);

    nng_msg *msg = nullptr;
    int rc = client_rpc_pack(&msg, funid, argv, vl);

    va_end(vl);
    // Do this here to make dealing with va_start()/va_end() easier.
    if (rc != QV_SUCCESS) {
        char const *ers = "client_rpc_pack() failed";
        QVI_LOG_ERROR("{} with rc={} ({})", ers, rc, qv_strerr(rc));
        return rc;
    }

    rc = nng_sendmsg(client->sock, msg, 0);
    if (rc != 0) {
        QVI_LOG_INFO("nng_sendmsg() failed");
    }
    // Freeing up of the msg resource will be done for us.
    return QV_SUCCESS;
}

int
qvi_rpc_client_rep(
    qvi_rpc_client_t *client,
    qvi_rpc_fun_args_t *fun_args
) {
    nng_msg *msg;
    int rc = nng_recvmsg(client->sock, &msg, 0);
    if (rc != 0) {
        QVI_LOG_INFO("nng_recvmsg() failed");
    }

    memmove(fun_args, nng_msg_body(msg), sizeof(*fun_args));

    nng_msg_free(msg);

    return QV_SUCCESS;
}

/**
 *
 */
// TODO(skg) Optimize
static void
server_msg_state_machine(
    void *data
) {
    qvi_rpc_wqi_t *wqi = (qvi_rpc_wqi_t *)data;

    switch (wqi->state) {
    case qvi_rpc_wqi_t::RECV:
        server_msg_state_recv(wqi);
        break;
    case qvi_rpc_wqi_t::SEND:
        if (nng_aio_result(wqi->aio) != 0) {
            nng_msg_free(wqi->msg);
            // TODO(skg) deal with error
        }
        wqi->state = qvi_rpc_wqi_t::RECV;
        nng_recv_aio(wqi->sock, wqi->aio);
        break;
    // The least likely state.
    case qvi_rpc_wqi_t::INIT:
        wqi->state = qvi_rpc_wqi_t::RECV;
        nng_recv_aio(wqi->sock, wqi->aio);
        break;
    default:
        QVI_LOG_ERROR("Invalid state entered: {}", wqi->state);
        break;
    }
}

/**
 *
 */
static void
server_deallocate_outstanding_msg_queue(
    qvi_rpc_server_t *server
) {
    if (!server->wqis) return;

    for (uint16_t i = 0; i < server->qdepth; ++i) {
        if (server->wqis[i]->aio) {
            nng_aio_free(server->wqis[i]->aio);
        }
        if (server->wqis[i]) {
            nng_free(server->wqis[i], sizeof(qvi_rpc_wqi_t));
        }
    }
    free(server->wqis);
    server->wqis = nullptr;
}

/**
 *
 */
static int
server_allocate_outstanding_msg_queue(
    qvi_rpc_server_t *server
) {
    int rc = QV_SUCCESS;
    char const *ers = nullptr;

    server->wqis = (qvi_rpc_wqi_t **)calloc(
        server->qdepth,
        sizeof(*server->wqis)
    );
    if (!server->wqis) {
        QVI_LOG_ERROR("calloc() failed");
        return QV_ERR_OOR;
    }

    for (uint16_t i = 0; i < server->qdepth; ++i) {
        qvi_rpc_wqi_t *wqi = nullptr;
        if (!(wqi = (qvi_rpc_wqi_t *)nng_alloc(sizeof(*wqi)))) {
            ers = "nng_alloc() failed";
            rc = QV_ERR_OOR;
            goto out;
        }
        rc = nng_aio_alloc(&wqi->aio, server_msg_state_machine, wqi);
        if (rc != 0) {
            ers = "nng_aio_alloc() failed";
            rc = QV_ERR_OOR;
            goto out;
        }
        wqi->hwloc = server->hwloc;
        wqi->state = qvi_rpc_wqi_t::INIT;
        wqi->sock  = server->sock;
        server->wqis[i] = wqi;
    }
out:
    if (ers) {
        QVI_LOG_ERROR("{}", ers);
        // Relinquish any allocated resources.
        server_deallocate_outstanding_msg_queue(server);
        return rc;
    }
    return QV_SUCCESS;
}

/**
 *
 */
static int
server_hwloc_init(
    qvi_rpc_server_t *server
) {
    int rc = QV_SUCCESS;
    char const *ers = nullptr;

    rc = qv_hwloc_init(server->hwloc);
    if (rc != QV_SUCCESS) {
        ers = "qv_hwloc_init() failed";
        goto out;
    }

    rc = qv_hwloc_topo_load(server->hwloc);
    if (rc != QV_SUCCESS) {
        ers = "qv_hwloc_topo_load() failed";
        goto out;
    }
out:
    if (ers) {
        QVI_LOG_ERROR("{} with rc={} ({})", ers, rc, qv_strerr(rc));
    }
    return rc;
}

int
qvi_rpc_server_construct(
    qvi_rpc_server_t **server
) {
    if (!server) return QV_ERR_INVLD_ARG;

    int rc = QV_SUCCESS;
    char const *ers = nullptr;

    qvi_rpc_server_t *iserver = (qvi_rpc_server_t *)calloc(1, sizeof(*iserver));
    if (!iserver) {
        QVI_LOG_ERROR("calloc() failed");
        return QV_ERR_OOR;
    }

    rc = qv_hwloc_construct(&iserver->hwloc);
    if (rc != QV_SUCCESS) {
        ers = "qv_hwloc_construct() failed";
        goto out;
    }
out:
    if (ers) {
        QVI_LOG_ERROR("{} with rc={} ({})", ers, rc, qv_strerr(rc));
        qvi_rpc_server_destruct(iserver);
        *server = nullptr;
        return rc;
    }
    *server = iserver;
    return QV_SUCCESS;
}

void
qvi_rpc_server_destruct(
    qvi_rpc_server_t *server
) {
    if (!server) return;
    // Close the socket before we free any other resources.
    int rc = nng_close(server->sock);
    if (rc != 0) {
        char const *ers = "nng_close() failed during shutdown";
        QVI_LOG_WARN("{} with rc={} ({})", ers, rc, nng_strerror(rc));
    }
    //
    server_deallocate_outstanding_msg_queue(server);
    qv_hwloc_destruct(server->hwloc);
    free(server);
}

static int
server_setup(
    qvi_rpc_server_t *server,
    const char *url,
    uint16_t qdepth
) {
    server->qdepth = qdepth;

    const int nwritten = snprintf(server->url, sizeof(server->url), "%s", url);
    if (nwritten >= URL_MAX_LEN) {
        QVI_LOG_ERROR("snprintf() truncated");
        return QV_ERR_INTERNAL;
    }
    // Register cleanup function at exit.
    register_atexit();
    return QV_SUCCESS;
}

/**
 *
 */
static int
server_open_commchan(
    qvi_rpc_server_t *server
) {
    const int rc = nng_rep0_open_raw(&server->sock);
    if (rc != 0) {
        char const *ers = "nng_rep0_open_raw() failed";
        QVI_LOG_ERROR("{} with rc={} ({})", ers, rc, nng_strerror(rc));
        return QV_ERR_MSG;
    }
    return QV_SUCCESS;
}

/**
 *
 */
static int
server_listen(
    qvi_rpc_server_t *server
) {
    char const *ers = nullptr;

    int rc = nng_listen(server->sock, server->url, nullptr, 0);
    if (rc != 0) {
        ers = "nng_listen() failed";
        goto out;
    }
    for (uint16_t i = 0; i < server->qdepth; ++i) {
        // This starts the messaging state machine.
        server_msg_state_machine(server->wqis[i]);
    }
out:
    if (ers) {
        QVI_LOG_ERROR("{} with rc={} ({})", ers, rc, nng_strerror(rc));
        return QV_ERR_MSG;
    }
    return QV_SUCCESS;
}

int
qvi_rpc_server_start(
    qvi_rpc_server_t *server,
    const char *url,
    uint16_t qdepth
) {
    if (!server || !url) return QV_ERR_INVLD_ARG;

    int rc = QV_SUCCESS;
    char const *ers = nullptr;

    rc = server_hwloc_init(server);
    if (rc != QV_SUCCESS) {
        ers = "server_hwloc_init() failed";
        goto out;
    }

    rc = server_setup(server, url, qdepth);
    if (rc != QV_SUCCESS) {
        ers = "server_setup() failed";
        goto out;
    }

    rc = server_open_commchan(server);
    if (rc != QV_SUCCESS) {
        ers = "server_open_commchan() failed";
        goto out;
    }

    rc = server_allocate_outstanding_msg_queue(server);
    if (rc != QV_SUCCESS) {
        ers = "server_allocate_outstanding_msg_queue() failed";
        goto out;
    }

    rc = server_listen(server);
    if (rc != QV_SUCCESS) {
        ers = "server_listen() failed";
        goto out;
    }
out:
    if (ers) {
        QVI_LOG_ERROR("{} with rc={} ({})", ers, rc, qv_strerr(rc));
        // Start failed, so relinquish allocated resources.
        qvi_rpc_server_destruct(server);
        return rc;
    }
    return QV_SUCCESS;
}

int
qvi_rpc_client_construct(
    qvi_rpc_client_t **client
) {
    if (!client) return QV_ERR_INVLD_ARG;

    qvi_rpc_client_t *iclient = (qvi_rpc_client_t *)calloc(1, sizeof(*iclient));
    if (!iclient) {
        QVI_LOG_ERROR("calloc() failed");
        return QV_ERR_OOR;
    }
    register_atexit();
    *client = iclient;
    return QV_SUCCESS;
}

void
qvi_rpc_client_destruct(
    qvi_rpc_client_t *client
) {
    if (!client) return;

    nng_close(client->sock);
    free(client);
}

int
qvi_rpc_client_connect(
    qvi_rpc_client_t *client,
    const char *url
) {
    return client_connect(client, url);
}

/**
 * TODO(skg) Lots of error path cleanup required here.
 */
static inline void
server_msg_state_recv(
    qvi_rpc_wqi_t *wqi
) {
    int rc = QV_SUCCESS;
    char const *ers = nullptr;
    // NOTE: this call typically fails during teardown, so just return.
    if (nng_aio_result(wqi->aio) != 0) {
        return;
    }
    // TODO(skg) trim in dispatch to eat all of the body?
    // TODO(skg) Check for server_msg_state_recv errors.
    nng_msg *msg = nng_aio_get_msg(wqi->aio);

    qvi_msg_header_t msghdr;
    const size_t trim = rpc_unpack_msg_header(msg, &msghdr);
    // Trim message header because server_rpc_unpack() expects it.
    rc = nng_msg_trim(msg, trim);
    if (rc != 0) {
        ers = "nng_msg_trim() failed";
        QVI_LOG_ERROR("{} with rc={} ({})", ers, rc, nng_strerror(rc));
    }

    qvi_rpc_fun_args_t fun_args;
    // TODO(skg) Improve. I don't like the memset here.
    memset(&fun_args, 0, sizeof(fun_args));
    // Set hwloc instance pointer for use in server-side call.
    fun_args.hwloc = wqi->hwloc;

    rc = server_rpc_unpack(msg, msghdr, &fun_args);
    if (rc != QV_SUCCESS) {
        ers = "server_rpc_unpack() failed";
    }
    // Dispatch (call) the requested function.
    fun_args.rc = qvi_server_rpc_dispatch_table[msghdr.funid](&fun_args);
    if (fun_args.rc != QV_SUCCESS) {
        // TODO(skg) Improve.
    }
    // Done processing received message, so clear message body.
    nng_msg_clear(msg);
    // Prepare message for reply. In this case, set message content to the newly
    // populated function arguments.
    if (nng_msg_append(msg, &fun_args, sizeof(fun_args)) != 0) {
        QVI_LOG_ERROR("nng_msg_append() failed");
    }
    // Prepare the WQI for next state: SEND.
    wqi->msg = msg;
    nng_aio_set_msg(wqi->aio, wqi->msg);
    wqi->state = qvi_rpc_wqi_t::SEND;
    nng_send_aio(wqi->sock, wqi->aio);
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
