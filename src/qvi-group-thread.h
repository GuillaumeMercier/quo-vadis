/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2024 Triad National Security, LLC
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
 * @file qvi-group-thread.h
 *
 */

#ifndef QVI_GROUP_THREAD_H
#define QVI_GROUP_THREAD_H

#include "qvi-group.h"
#include "qvi-thread.h"

struct qvi_group_thread_s : public qvi_group_s {
    /**
     * Initialized qvi_thread_t instance
     * embedded in thread group instances.
     */
    qvi_thread_t *th = nullptr;
    /** Underlying group instance. */
    qvi_thread_group_t *th_group = nullptr;
    /** Base constructor that does minimal work. */
    qvi_group_thread_s(void)
    {
        const int rc = qvi_thread_group_new(&th_group);
        if (rc != QV_SUCCESS) throw qvi_runtime_error();
    }
    /** Virtual destructor. */
    virtual ~qvi_group_thread_s(void)
    {
        qvi_thread_group_free(&th_group);
    }
    /** Initializes the instance. */
    int initialize(qvi_thread_t *th_a)
    {
        if (!th_a) qvi_abort();

        th = th_a;
        return QV_SUCCESS;
    }
    /** Returns the caller's task_id. */
    virtual qvi_task_id_t task_id(void)
    {
        return qvi_task_task_id(qvi_thread_task_get(th));
    }
    /** Returns the caller's group ID. */
    virtual int id(void)
    {
        return qvi_thread_group_id(th_group);
    }
    /** Returns the number of members in this group. */
    virtual int size(void)
    {
        return qvi_thread_group_size(th_group);
    }
    /** Performs node-local group barrier. */
    virtual int barrier(void)
    {
        return qvi_thread_group_barrier(th_group);
    }
    /**
     * Creates a new self group with a single member: the caller.
     * Returns the appropriate newly created child group to the caller.
     */
    virtual int
    self(
        qvi_group_s **child
    );
    /**
     * Creates new groups by splitting this group based on color, key.
     * Returns the appropriate newly created child group to the caller.
     */
    virtual int
    split(
        int color,
        int key,
        qvi_group_s **child
    );
    /** Gathers bbuffs to specified root. */
    virtual int
    gather(
        qvi_bbuff_t *txbuff,
        int root,
        qvi_bbuff_t ***rxbuffs,
        int *shared
    ) {
        return qvi_thread_group_gather_bbuffs(
           th_group, txbuff, root, rxbuffs, shared
        );
    }
    /** Scatters bbuffs from specified root. */
    virtual int
    scatter(
        qvi_bbuff_t **txbuffs,
        int root,
        qvi_bbuff_t **rxbuff
    ) {
        return qvi_thread_group_scatter_bbuffs(
            th_group, txbuffs, root, rxbuff
        );
    }
};

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
