/*
 * Copyright (c) 2020-2022 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c) 2020-2021 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file test-mpi-phases.c
 */

#include "quo-vadis-mpi.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define USE_AFFINITY_PRESERVING 1


#define panic(vargs...)                                                        \
do {                                                                           \
    fprintf(stderr, "\n%s@%d: ", __func__, __LINE__);                          \
    fprintf(stderr, vargs);                                                    \
    fprintf(stderr, "\n");                                                     \
    fflush(stderr);                                                            \
    exit(EXIT_FAILURE);                                                        \
} while (0)


static int
do_omp_things(int rank, int npus)
{
  printf("[%d] Doing OpenMP things with %d PUs\n", rank, npus);
  return 0;
}

static int
do_pthread_things(int rank, int ncores)
{
  printf("[%d] Doing pthread_things with %d cores\n", rank, ncores);
  return 0;
}



int
main(
    int argc,
    char **argv
) {
    char const *ers = NULL;
    MPI_Comm comm = MPI_COMM_WORLD;

    /* Initialization */
    int rc = MPI_Init(&argc, &argv);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Init() failed";
        panic("%s (rc=%d)", ers, rc);
    }

    int wsize;
    rc = MPI_Comm_size(comm, &wsize);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_size() failed";
        panic("%s (rc=%d)", ers, rc);
    }

    int wrank;
    rc = MPI_Comm_rank(comm, &wrank);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_rank() failed";
        panic("%s (rc=%d)", ers, rc);
    }

    setbuf(stdout, NULL);

    /* Create a QV context */
    qv_context_t *ctx;
    rc = qv_mpi_context_create(&ctx, comm);
    if (rc != QV_SUCCESS) {
        ers = "qv_mpi_context_create() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    /* Get base scope: RM-given resources */
    qv_scope_t *base_scope;
    rc = qv_scope_get(
        ctx,
        QV_SCOPE_USER,
        &base_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_get() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int ncores;
    rc = qv_scope_nobjs(
        ctx,
        base_scope,
        QV_HW_OBJ_CORE,
        &ncores
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_nobjs() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    if (wrank == 0)
      printf("\n===Phase 1: Regular split===\n"); 

    char *binds;
    rc = qv_bind_string(ctx, QV_BIND_STRING_AS_LIST, &binds);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_get_list_as_string() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("[%d] Base scope w/%d cores, running on %s\n",
       wrank, ncores, binds);
    free(binds);

    /* Split the base scope evenly across workers */
    qv_scope_t *sub_scope;
    rc = qv_scope_split(
        ctx,
        base_scope,
        wsize,        // Number of workers
#ifdef USE_AFFINITY_PRESERVING
	QV_SCOPE_SPLIT_AFFINITY_PRESERVING,
#else
	wrank,        // My group color
#endif 
        &sub_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_split() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    /* What resources did I get? */
    rc = qv_scope_nobjs(
        ctx,
        sub_scope,
        QV_HW_OBJ_CORE,
        &ncores
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_nobjs() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    /***************************************
     * Phase 1: Everybody works
     ***************************************/

    rc = qv_bind_push(ctx, sub_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_push() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    /* Where did I end up? */
    rc = qv_bind_string(ctx, QV_BIND_STRING_AS_LIST, &binds);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_get_list_as_string() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("=> [%d] Split: got %d cores, running on %s\n",
       wrank, ncores, binds);
    free(binds);

#if 1
    /* Launch one thread per core */
    do_pthread_things(wrank, ncores);

    /* Launch one kernel per GPU */
    int ngpus;
    rc = qv_scope_nobjs(
        ctx,
        sub_scope,
        QV_HW_OBJ_GPU,
        &ngpus
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_nobjs() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("[%d] Launching %d GPU kernels\n", wrank, ngpus);

    int i, dev;
    char *gpu;
    for (i=0; i<ngpus; i++) {
        qv_scope_get_device_id(ctx, sub_scope, QV_HW_OBJ_GPU, i, QV_DEVICE_ID_PCI_BUS_ID, &gpu);
        printf("GPU %d PCI Bus ID = %s\n", i, gpu);
        //cudaDeviceGetByPCIBusId(&dev, gpu);
        //cudaSetDevice(dev);
        /* Launch GPU kernels here */
        free(gpu);
    }
#endif

    rc = qv_bind_pop(ctx);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_pop() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_bind_string(ctx, QV_BIND_STRING_AS_LIST, &binds);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_get_list_as_string() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("[%d] Popped up to %s\n", wrank, binds);
    free(binds);

    /* Keep printouts separate for each phase */ 
    rc = qv_context_barrier(ctx);
    if (rc != QV_SUCCESS) {
      ers = "qv_context_barrier() failed";
      panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    /***************************************
     * Phase 2: One master per resource,
     *          others sleep, ay.
     ***************************************/

    /* We could also do this by finding how many
       NUMA objects are there in the scope, and
       then splitting over that number. Then,
       we could ask for a leader of each subscope.
       However, this does not guarantee a NUMA split.
       Thus, we use qv_scope_split_at. */
    if (wrank == 0)
      printf("\n===Phase 2: NUMA split===\n"); 

#if 1
    int nnumas, my_numa_id;
    qv_scope_t *numa_scope;

    /* Get the number of NUMA domains so that we can
       specify the color/groupid of split_at */
    rc = qv_scope_nobjs(
        ctx,
        base_scope,
	QV_HW_OBJ_NUMANODE,
        &nnumas
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_nobjs() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    /* Split at NUMA domains */
    rc = qv_scope_split_at(
        ctx,
        base_scope,
        QV_HW_OBJ_NUMANODE,
#ifdef USE_AFFINITY_PRESERVING
	QV_SCOPE_SPLIT_AFFINITY_PRESERVING,
#else
	wrank % nnumas,          // color or group id
#endif 
        &numa_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_split_at() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    /* Allow selecting a leader per NUMA */
    rc = qv_scope_taskid(
        ctx,
        numa_scope,
        &my_numa_id
    );

    printf("[%d]: #NUMAs=%d numa_scope_id=%d\n",
	   wrank, nnumas, my_numa_id); 
    
    rc = qv_bind_push(ctx, numa_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_push() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int my_nnumas;
    rc = qv_scope_nobjs(ctx,
            numa_scope,
            QV_HW_OBJ_NUMANODE,
            &my_nnumas);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_nobjs() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    /* Where did I end up? */
    rc = qv_bind_string(ctx, QV_BIND_STRING_AS_LIST, &binds);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_get_list_as_string() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("=> [%d] Split@NUMA: got %d NUMAs, running on %s\n",
       wrank, my_nnumas, binds);
    free(binds);


    int npus;
    if (my_numa_id == 0) {
        /* I am the process lead */
        rc = qv_scope_nobjs(
            ctx,
            numa_scope,
            QV_HW_OBJ_PU,
            &npus
        );
        if (rc != QV_SUCCESS) {
            ers = "qv_scope_nobjs() failed";
            panic("%s (rc=%s)", ers, qv_strerr(rc));
        }
        printf("=> [%d] NUMA leader: Launching OMP region\n", wrank);
        do_omp_things(wrank, npus);
    }

    /* Everybody else waits... */
    rc = qv_scope_barrier(ctx, numa_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_context_barrier() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_bind_pop(ctx);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_pop() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_bind_string(ctx, QV_BIND_STRING_AS_LIST, &binds);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_get_list_as_string() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("[%d] Popped up to %s\n", wrank, binds);
    free(binds);

    /* Keep printouts separate for each phase */ 
    rc = qv_context_barrier(ctx);
    if (rc != QV_SUCCESS) {
      ers = "qv_context_barrier() failed";
      panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
#endif

    /***************************************
     * Phase 3: 
     *   GPU work! 
     ***************************************/
    if (wrank == 0)
      printf("\n===Phase 3: GPU split===\n"); 
    
    int my_gpu_id;
    qv_scope_t *gpu_scope;

    /* Get the number of GPUs so that we can
       specify the color/groupid of split_at */
    rc = qv_scope_nobjs(ctx, base_scope,
			QV_HW_OBJ_GPU, &ngpus);
    if (rc != QV_SUCCESS) {
      ers = "qv_scope_nobjs() failed";
      panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    /* Split at GPUs */
    rc = qv_scope_split_at(
        ctx,
        base_scope,
        QV_HW_OBJ_GPU,
        // TODO Fix if no GPUs; Crashes with 0
#ifdef USE_AFFINITY_PRESERVING
	QV_SCOPE_SPLIT_AFFINITY_PRESERVING,
#else
	wrank % ngpus,          // color or group id
#endif 
        &gpu_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_split_at() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    /* Allow selecting a leader per NUMA */
    rc = qv_scope_taskid(
        ctx,
        gpu_scope,
        &my_gpu_id
    );

    rc = qv_bind_push(ctx, gpu_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_push() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int my_ngpus;
    rc = qv_scope_nobjs(ctx,
            gpu_scope,
            QV_HW_OBJ_GPU,
            &my_ngpus);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_nobjs() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    /* Where did I end up? */
    rc = qv_bind_string(ctx, QV_BIND_STRING_AS_LIST, &binds);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_get_list_as_string() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("=> [%d] Split@GPU: got %d GPUs, running on %s\n",
       wrank, my_ngpus, binds);
    free(binds);
    
    for (i=0; i<my_ngpus; i++) {
      qv_scope_get_device_id(ctx, gpu_scope, QV_HW_OBJ_GPU,
			  i, QV_DEVICE_ID_PCI_BUS_ID, &gpu);
      printf("   [%d] GPU %d PCI Bus ID = %s\n", wrank, i, gpu);
      free(gpu);
    }


    /***************************************
     * Clean up
     ***************************************/

    rc = qv_scope_free(ctx, numa_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_scope_free(ctx, sub_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_scope_free(ctx, base_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_context_barrier(ctx);
    if (rc != QV_SUCCESS) {
        ers = "qv_context_barrier() failed";
        panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
out:
    if (qv_mpi_context_free(ctx) != QV_SUCCESS) {
        ers = "qv_mpi_context_free() failed";
        panic("%s", ers);
    }
    MPI_Finalize();
    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
