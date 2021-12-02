/*
 * Copyright (c)      2021 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-nvml.h
 */

#ifndef QVI_NVML_H
#define QVI_NVML_H

#include "qvi-common.h"
#include "qvi-hwloc.h"

#ifdef __cplusplus
extern "C" {
#endif

int
qvi_hwloc_nvml_get_device_cpuset_by_pci_bus_id(
    qvi_hwloc_t *hwl,
    const char *uuid,
    hwloc_cpuset_t cpuset
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */