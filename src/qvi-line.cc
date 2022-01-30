/*
 * Copyright (c) 2020-2022 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-line.cc
 *
 * Line types and functions for sending and receiving data over the network.
 * More generally, they are types that can be easily serialized.
 */

#include "qvi-common.h"
#include "qvi-line.h"
#include "qvi-bbuff-rmi.h"
#include "qvi-utils.h"

int
qvi_line_config_new(
    qvi_line_config_t **config
) {
    int rc = QV_SUCCESS;
    qvi_line_config_t *ic = (qvi_line_config_t *)calloc(1, sizeof(*ic));
    if (!ic) rc = QV_ERR_OOR;
    // Do minimal initialization here because other routines will do the rest.
    if (rc != QV_SUCCESS) qvi_line_config_free(&ic);
    *config = ic;
    return rc;
}

void
qvi_line_config_free(
    qvi_line_config_t **config
) {
    if (!config) return;
    qvi_line_config_t *ic = *config;
    if (!ic) goto out;
    if (ic->url) free(ic->url);
    if (ic->hwtopo_path) free(ic->hwtopo_path);
    free(ic);
out:
    *config = nullptr;
}

int
qvi_line_config_cp(
    qvi_line_config_t *from,
    qvi_line_config_t *to
) {
    to->hwloc = from->hwloc;
    int nw = asprintf(&to->url, "%s", from->url);
    if (nw == -1) {
        to->url = nullptr;
        return QV_ERR_OOR;
    }
    nw = asprintf(&to->hwtopo_path, "%s", from->hwtopo_path);
    if (nw == -1) {
        to->hwtopo_path = nullptr;
        return QV_ERR_OOR;
    }
    return QV_SUCCESS;
}

int
qvi_line_config_pack(
    qvi_line_config_t *config,
    qvi_bbuff_t *buff
) {
    return qvi_bbuff_rmi_sprintf(
        buff,
        QVI_LINE_CONFIG_PICTURE,
        config->url,
        config->hwtopo_path
    );
}

int
qvi_line_config_unpack(
    void *buff,
    qvi_line_config_t **config
) {
    int rc = qvi_line_config_new(config);
    if (rc != QV_SUCCESS) return rc;

    return qvi_bbuff_rmi_sscanf(
        buff,
        QVI_LINE_CONFIG_PICTURE,
        &(*config)->url,
        &(*config)->hwtopo_path
    );
}

int
qvi_line_hwpool_new(
    qvi_line_hwpool_t **hwp
) {
    int rc = QV_SUCCESS;

    qvi_line_hwpool_t *ihwp = (qvi_line_hwpool_t *)calloc(1, sizeof(*ihwp));
    if (!ihwp) rc = QV_ERR_OOR;
    // Do minimal initialization here because other routines will do the rest.
    if (rc != QV_SUCCESS) qvi_line_hwpool_free(&ihwp);
    *hwp = ihwp;
    return rc;
}

void
qvi_line_hwpool_free(
    qvi_line_hwpool_t **hwp
) {
    if (!hwp) return;
    qvi_line_hwpool_t *ihwp = *hwp;
    if (!ihwp) goto out;
    hwloc_bitmap_free(ihwp->cpuset);
    if (ihwp->device_tab) {
        const int ndevt = qvi_hwloc_n_supported_devices();
        for (int i = 0; i < ndevt; ++i) {
            if (ihwp->device_tab[i]) free(ihwp->device_tab[i]);
        }
        free(ihwp->device_tab);
    }
    free(ihwp);
out:
    *hwp = nullptr;
}

int
qvi_line_hwpool_ndevids(
    qvi_line_hwpool_t *hwp,
    int devid_index
) {
    int *ids = hwp->device_tab[devid_index];
    assert(ids);

    int n = 0;
    for (; ids[n] != qvi_line_hwpool_devid_last; ++n);
    // Includes the sentinel value, so add 1.
    return n + 1;
}

int
qvi_line_hwpool_cp(
    qvi_line_hwpool_t *from,
    qvi_line_hwpool_t *to
) {
    int rc = QV_SUCCESS;
    // TODO(skg) Implement the rest
    return rc;
}

int
qvi_line_hwpool_pack(
    qvi_line_hwpool_t *hwp,
    qvi_bbuff_t *buff
) {
    return qvi_bbuff_rmi_sprintf(
        buff,
        QVI_LINE_HWPOOL_PICTURE,
        hwp
    );
}

int
qvi_line_hwpool_unpack(
    void *buff,
    qvi_line_hwpool_t **hwp
) {
    return qvi_bbuff_rmi_sscanf(
        buff,
        QVI_LINE_HWPOOL_PICTURE,
        hwp
    );
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 *
 */
