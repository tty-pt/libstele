/*
 * Copyright (c) 2026, tty-pt
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 */

#ifndef TTYPT_WQ_H
#define TTYPT_WQ_H

/**
 * @file wq.h
 * @brief Cross-platform lock-free workqueue & worker pool for libstele.
 *
 * Provides a unified, zero-allocation-per-job taskqueue (`stl_wq_t`) that maps
 * transparently to:
 *   - POSIX `pthread` worker pools on Linux, macOS, and BSDs
 *   - Win32 threads + `SRWLOCK` / `CONDITION_VARIABLE` on Windows
 *   - WebAssembly shared-memory worker pools (`SharedArrayBuffer` + atomics)
 *     with automatic frame-budgeted cooperative fallback (`stl_wq_step`) on
 *     single-core or non-isolated browser contexts.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>

#if __has_include(<ttypt/iso.h>)
#include <ttypt/iso.h>
#else
#include "iso.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Portable thread-local storage specifier for lock-free per-worker caches */
#ifndef STL_THREAD_LOCAL
#if defined(_MSC_VER)
#define STL_THREAD_LOCAL __declspec(thread)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_THREADS__)
#define STL_THREAD_LOCAL _Thread_local
#elif defined(__GNUC__) || defined(__clang__)
#define STL_THREAD_LOCAL __thread
#else
#define STL_THREAD_LOCAL
#endif
#endif

/**
 * @brief Task callback signature executed by a workqueue worker or cooperative step.
 */
typedef void (*stl_wq_fn)(void *arg);

/**
 * @brief Lifecycle states for a `stl_wq_task_t`.
 */
typedef enum {
	STL_WQ_IDLE    = 0, /* Task is not queued or running */
	STL_WQ_QUEUED  = 1, /* Task is in the workqueue ring awaiting execution */
	STL_WQ_RUNNING = 2, /* Task is currently executing on a worker */
	STL_WQ_DONE    = 3  /* Task has completed execution */
} stl_wq_state_t;

/**
 * @brief Embeddable zero-allocation workqueue task descriptor.
 *
 * Embed directly inside your struct (e.g. `ar_node_meta_t` or `block_bb_cache_slot_t`)
 * so submitting jobs requires zero `malloc`/`free` calls.
 */
typedef struct stl_wq_task {
	stl_wq_fn       fn;
	void           *arg;
	_Atomic uint8_t state;
} stl_wq_task_t;

typedef struct stl_wq stl_wq_t;

/**
 * @brief Query the number of hardware logical processors available.
 * Returns >= 1 on all platforms.
 */
ISO_EXPORT(unsigned int, stl_cpu_count, void);

/**
 * @brief Create a named workqueue with `nthreads` persistent worker threads
 * and a power-of-two ring buffer of capacity `ring_cap` (default 256 if 0).
 *
 * If `nthreads == 0`, automatically selects a balanced worker count
 * (`clamp(stl_cpu_count() - 1, 1, 4)` on native, or cooperative / host-backed on WASM).
 */
ISO_EXPORT(stl_wq_t *, stl_wq_create, const char *name, unsigned int nthreads, unsigned int ring_cap);

/**
 * @brief Submit a task to the workqueue.
 *
 * - If `task` is already `STL_WQ_QUEUED` or `STL_WQ_RUNNING`, returns 1 (no-op).
 * - If the ring is full, executes `fn(arg)` inline on the caller thread and marks
 *   `task->state = STL_WQ_DONE` so callers never deadlock or drop work.
 * - Returns 0 on success, -1 on invalid arguments.
 */
ISO_EXPORT(int, stl_wq_submit, stl_wq_t *wq, stl_wq_task_t *task, stl_wq_fn fn, void *arg);

/**
 * @brief Initialize a task struct to `STL_WQ_IDLE`.
 */
static inline void stl_wq_task_init(stl_wq_task_t *task)
{
	if (!task)
		return;
	task->fn = NULL;
	task->arg = NULL;
	atomic_store_explicit(&task->state, (uint8_t)STL_WQ_IDLE, memory_order_relaxed);
}

/**
 * @brief Non-blocking check whether a task has finished (`STL_WQ_DONE`) or is idle (`STL_WQ_IDLE`).
 */
static inline bool stl_wq_done(const stl_wq_task_t *task)
{
	if (!task)
		return true;
	uint8_t st = atomic_load_explicit(&((stl_wq_task_t *)task)->state, memory_order_acquire);
	return (st == STL_WQ_DONE || st == STL_WQ_IDLE);
}

/**
 * @brief Non-blocking check whether a task is currently queued or running.
 */
static inline bool stl_wq_busy(const stl_wq_task_t *task)
{
	if (!task)
		return false;
	uint8_t st = atomic_load_explicit(&((stl_wq_task_t *)task)->state, memory_order_acquire);
	return (st == STL_WQ_QUEUED || st == STL_WQ_RUNNING);
}

/**
 * @brief Execute up to `max_tasks` pending tasks from `wq` on the calling thread.
 * Returns the number of tasks executed.
 */
ISO_EXPORT(unsigned int, stl_wq_step, stl_wq_t *wq, unsigned int max_tasks);

/**
 * @brief Wait until a specific task has completed (`STL_WQ_DONE` or `STL_WQ_IDLE`).
 * Cooperatively helps drain the queue while waiting so the caller never deadlocks.
 */
ISO_EXPORT(void, stl_wq_wait, stl_wq_t *wq, stl_wq_task_t *task);

/**
 * @brief Wait until all queued and running tasks in `wq` have completed.
 */
ISO_EXPORT(void, stl_wq_drain, stl_wq_t *wq);

/**
 * @brief Drain all pending work, stop worker threads, and free `wq`.
 */
ISO_EXPORT(void, stl_wq_destroy, stl_wq_t *wq);

/* WASM worker entry point invoked when Web Worker sub-threads attach */
ISO_EXPORT(void, stl_wq_worker_entry, uintptr_t wq_ptr, unsigned int thread_idx);

/* Canonical stele_* & wq_* aliases matching OPTIMIZATION.md §5 */
typedef stl_wq_fn     wq_fn_t;
typedef stl_wq_task_t wq_task_t;
typedef stl_wq_t      wq_t;

#define stele_cpu_count   stl_cpu_count
#define stele_wq_create   stl_wq_create
#define stele_wq_submit   stl_wq_submit
#define stele_wq_done     stl_wq_done
#define stele_wq_busy     stl_wq_busy
#define stele_wq_step     stl_wq_step
#define stele_wq_wait     stl_wq_wait
#define stele_wq_drain    stl_wq_drain
#define stele_wq_destroy  stl_wq_destroy

#define wq_create         stl_wq_create
#define wq_submit         stl_wq_submit
#define wq_task_done      stl_wq_done
#define wq_step           stl_wq_step
#define wq_wait           stl_wq_wait
#define wq_drain          stl_wq_drain
#define wq_destroy        stl_wq_destroy

#ifdef __cplusplus
}
#endif

#endif /* TTYPT_WQ_H */
