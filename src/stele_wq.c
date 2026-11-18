/*
 * Copyright (c) 2026, tty-pt
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 */

#include "../include/ttypt/wq.h"
#include <stdlib.h>
#include <string.h>

#if defined(__wasm__)
ISO_IMPORT(int, stele, stl_host_cpu_count, void);
ISO_IMPORT(int, stele, stl_host_spawn_worker, uintptr_t wq_ptr, unsigned int thread_idx);
#elif defined(_WIN32)
#include <windows.h>
#include <process.h>
#else
#include <pthread.h>
#include <unistd.h>
#include <sched.h>
#endif

#define STL_WQ_DEFAULT_RING_CAP 256u
#define STL_WQ_MAX_THREADS      16u

typedef struct {
	_Atomic uint32_t seq;
	_Atomic uintptr_t task_ptr;
} stl_wq_slot_t;

struct stl_wq {
	char              name[32];
	uint32_t          cap;
	uint32_t          mask;
	_Atomic uint32_t  head;        /* Producer enqueue cursor */
	_Atomic uint32_t  tail;        /* Consumer dequeue cursor */
	_Atomic uint32_t  in_flight;   /* Queued + currently executing tasks */
	_Atomic uint32_t  stop;        /* 1 when destroying */
	unsigned int      nthreads;
	stl_wq_slot_t    *ring;
	struct stl_wq    *next_active; /* Intrusive list of active workqueues for cooperative stepping */

#if !defined(__wasm__) && defined(_WIN32)
	SRWLOCK            lock;
	CONDITION_VARIABLE cv_work;
	CONDITION_VARIABLE cv_idle;
	HANDLE             threads[STL_WQ_MAX_THREADS];
#elif !defined(__wasm__)
	pthread_mutex_t    lock;
	pthread_cond_t     cv_work;
	pthread_cond_t     cv_idle;
	pthread_t          threads[STL_WQ_MAX_THREADS];
#else
	_Atomic int32_t    futex_word;
#endif
};

static stl_wq_t *s_active_wq_head = NULL;

static uint32_t next_pow2_u32(uint32_t v)
{
	if (v < 4u)
		v = 4u;
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v++;
	return v;
}

ISO_EXPORT(unsigned int, stl_cpu_count, void)
{
#if defined(__wasm__)
	int n = stl_host_cpu_count();
	return (n > 0) ? (unsigned int)n : 1u;
#elif defined(_WIN32)
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	return (si.dwNumberOfProcessors > 0) ? (unsigned int)si.dwNumberOfProcessors : 1u;
#else
	long n = sysconf(_SC_NPROCESSORS_ONLN);
	return (n > 0) ? (unsigned int)n : 1u;
#endif
}

static bool stl_wq_pop_and_run(stl_wq_t *wq)
{
	stl_wq_slot_t *slot = NULL;
	uint32_t pos = atomic_load_explicit(&wq->tail, memory_order_relaxed);

	for (;;) {
		slot = &wq->ring[pos & wq->mask];
		uint32_t seq = atomic_load_explicit(&slot->seq, memory_order_acquire);
		int32_t diff = (int32_t)(seq - (pos + 1u));

		if (diff == 0) {
			if (atomic_compare_exchange_weak_explicit(
			        &wq->tail, &pos, pos + 1u,
			        memory_order_relaxed, memory_order_relaxed)) {
				break;
			}
		} else if (diff < 0) {
			return false; /* Queue is empty */
		} else {
			pos = atomic_load_explicit(&wq->tail, memory_order_relaxed);
		}
	}

	stl_wq_task_t *task = (stl_wq_task_t *)atomic_load_explicit(&slot->task_ptr, memory_order_relaxed);
	atomic_store_explicit(&slot->task_ptr, (uintptr_t)0, memory_order_relaxed);
	atomic_store_explicit(&slot->seq, pos + wq->cap, memory_order_release);

	if (task) {
		stl_wq_fn fn = task->fn;
		void *arg = task->arg;
		atomic_store_explicit(&task->state, (uint8_t)STL_WQ_RUNNING, memory_order_release);
		if (fn)
			fn(arg);
		atomic_store_explicit(&task->state, (uint8_t)STL_WQ_DONE, memory_order_release);
	}

	uint32_t prev = atomic_fetch_sub_explicit(&wq->in_flight, 1u, memory_order_acq_rel);
#if !defined(__wasm__)
	if (prev == 1u && wq->nthreads > 0) {
#if defined(_WIN32)
		AcquireSRWLockExclusive(&wq->lock);
		WakeAllConditionVariable(&wq->cv_idle);
		ReleaseSRWLockExclusive(&wq->lock);
#else
		pthread_mutex_lock(&wq->lock);
		pthread_cond_broadcast(&wq->cv_idle);
		pthread_mutex_unlock(&wq->lock);
#endif
	}
#else
	(void)prev;
#endif
	return true;
}

#if !defined(__wasm__) && defined(_WIN32)
static DWORD WINAPI stl_wq_win32_worker(LPVOID param)
{
	stl_wq_t *wq = (stl_wq_t *)param;
	for (;;) {
		if (stl_wq_pop_and_run(wq))
			continue;
		AcquireSRWLockExclusive(&wq->lock);
		while (!atomic_load_explicit(&wq->stop, memory_order_acquire) &&
		       atomic_load_explicit(&wq->head, memory_order_acquire) ==
		       atomic_load_explicit(&wq->tail, memory_order_acquire)) {
			SleepConditionVariableSRW(&wq->cv_work, &wq->lock, INFINITE, 0);
		}
		bool stopping = (atomic_load_explicit(&wq->stop, memory_order_acquire) != 0);
		ReleaseSRWLockExclusive(&wq->lock);
		if (stopping &&
		    atomic_load_explicit(&wq->head, memory_order_acquire) ==
		    atomic_load_explicit(&wq->tail, memory_order_acquire)) {
			break;
		}
	}
	return 0;
}
#elif !defined(__wasm__)
static void *stl_wq_posix_worker(void *param)
{
	stl_wq_t *wq = (stl_wq_t *)param;
	for (;;) {
		if (stl_wq_pop_and_run(wq))
			continue;
		pthread_mutex_lock(&wq->lock);
		while (!atomic_load_explicit(&wq->stop, memory_order_acquire) &&
		       atomic_load_explicit(&wq->head, memory_order_acquire) ==
		       atomic_load_explicit(&wq->tail, memory_order_acquire)) {
			pthread_cond_wait(&wq->cv_work, &wq->lock);
		}
		bool stopping = (atomic_load_explicit(&wq->stop, memory_order_acquire) != 0);
		pthread_mutex_unlock(&wq->lock);
		if (stopping &&
		    atomic_load_explicit(&wq->head, memory_order_acquire) ==
		    atomic_load_explicit(&wq->tail, memory_order_acquire)) {
			break;
		}
	}
	return NULL;
}
#endif

ISO_EXPORT(void, stl_wq_worker_entry, uintptr_t wq_ptr, unsigned int thread_idx)
{
	(void)thread_idx;
	stl_wq_t *wq = (stl_wq_t *)wq_ptr;
	if (!wq)
		return;
	while (!atomic_load_explicit(&wq->stop, memory_order_acquire)) {
		if (stl_wq_pop_and_run(wq))
			continue;
#if defined(__wasm__) && defined(__wasm_atomics__)
		int32_t cur = atomic_load_explicit(&wq->futex_word, memory_order_acquire);
		if (atomic_load_explicit(&wq->head, memory_order_acquire) ==
		    atomic_load_explicit(&wq->tail, memory_order_acquire) &&
		    !atomic_load_explicit(&wq->stop, memory_order_acquire)) {
			__builtin_wasm_memory_atomic_wait32((int *)&wq->futex_word, cur, 10000000LL);
		}
#else
		break;
#endif
	}
}

ISO_EXPORT(stl_wq_t *, stl_wq_create, const char *name, unsigned int nthreads, unsigned int ring_cap)
{
	uint32_t cap = next_pow2_u32(ring_cap ? ring_cap : STL_WQ_DEFAULT_RING_CAP);
	stl_wq_t *wq = (stl_wq_t *)calloc(1, sizeof(stl_wq_t));
	if (!wq)
		return NULL;

	wq->ring = (stl_wq_slot_t *)calloc(cap, sizeof(stl_wq_slot_t));
	if (!wq->ring) {
		free(wq);
		return NULL;
	}

	if (name && name[0]) {
		strncpy(wq->name, name, sizeof(wq->name) - 1);
		wq->name[sizeof(wq->name) - 1] = '\0';
	} else {
		strcpy(wq->name, "stl_wq");
	}

	wq->cap = cap;
	wq->mask = cap - 1u;
	atomic_store_explicit(&wq->head, 0u, memory_order_relaxed);
	atomic_store_explicit(&wq->tail, 0u, memory_order_relaxed);
	atomic_store_explicit(&wq->in_flight, 0u, memory_order_relaxed);
	atomic_store_explicit(&wq->stop, 0u, memory_order_relaxed);

	for (uint32_t i = 0; i < cap; i++) {
		atomic_store_explicit(&wq->ring[i].seq, i, memory_order_relaxed);
		atomic_store_explicit(&wq->ring[i].task_ptr, (uintptr_t)0, memory_order_relaxed);
	}

	if (nthreads == 0) {
		unsigned int cpus = stl_cpu_count();
		nthreads = (cpus > 1u) ? (cpus - 1u) : 0u;
		if (nthreads > 4u)
			nthreads = 4u;
	}
	if (nthreads > STL_WQ_MAX_THREADS)
		nthreads = STL_WQ_MAX_THREADS;

	unsigned int spawned = 0;
#if defined(__wasm__)
	atomic_store_explicit(&wq->futex_word, 0, memory_order_relaxed);
	for (unsigned int i = 0; i < nthreads; i++) {
		if (stl_host_spawn_worker((uintptr_t)wq, i) == 0)
			spawned++;
		else
			break;
	}
#elif defined(_WIN32)
	InitializeSRWLock(&wq->lock);
	InitializeConditionVariable(&wq->cv_work);
	InitializeConditionVariable(&wq->cv_idle);
	for (unsigned int i = 0; i < nthreads; i++) {
		HANDLE h = CreateThread(NULL, 0, stl_wq_win32_worker, wq, 0, NULL);
		if (!h)
			break;
		wq->threads[spawned++] = h;
	}
#else
	pthread_mutex_init(&wq->lock, NULL);
	pthread_cond_init(&wq->cv_work, NULL);
	pthread_cond_init(&wq->cv_idle, NULL);
	for (unsigned int i = 0; i < nthreads; i++) {
		if (pthread_create(&wq->threads[spawned], NULL, stl_wq_posix_worker, wq) == 0)
			spawned++;
		else
			break;
	}
#endif
	wq->nthreads = spawned;
	wq->next_active = s_active_wq_head;
	s_active_wq_head = wq;
	return wq;
}

ISO_EXPORT(int, stl_wq_submit, stl_wq_t *wq, stl_wq_task_t *task, stl_wq_fn fn, void *arg)
{
	if (!task || !fn)
		return -1;

	/* Prevent duplicate enqueue if already queued or running */
	uint8_t cur = atomic_load_explicit(&task->state, memory_order_acquire);
	while (cur == STL_WQ_IDLE || cur == STL_WQ_DONE) {
		if (atomic_compare_exchange_weak_explicit(
		        &task->state, &cur, (uint8_t)STL_WQ_QUEUED,
		        memory_order_acq_rel, memory_order_acquire)) {
			break;
		}
	}
	if (cur == STL_WQ_QUEUED || cur == STL_WQ_RUNNING)
		return 1; /* Already queued or executing */

	task->fn = fn;
	task->arg = arg;

	if (!wq) {
		atomic_store_explicit(&task->state, (uint8_t)STL_WQ_RUNNING, memory_order_release);
		fn(arg);
		atomic_store_explicit(&task->state, (uint8_t)STL_WQ_DONE, memory_order_release);
		return 0;
	}

	atomic_fetch_add_explicit(&wq->in_flight, 1u, memory_order_acq_rel);

	stl_wq_slot_t *slot = NULL;
	uint32_t pos = atomic_load_explicit(&wq->head, memory_order_relaxed);
	for (;;) {
		slot = &wq->ring[pos & wq->mask];
		uint32_t seq = atomic_load_explicit(&slot->seq, memory_order_acquire);
		int32_t diff = (int32_t)(seq - pos);

		if (diff == 0) {
			if (atomic_compare_exchange_weak_explicit(
			        &wq->head, &pos, pos + 1u,
			        memory_order_relaxed, memory_order_relaxed)) {
				break;
			}
		} else if (diff < 0) {
			/* Ring full: execute inline on caller thread so work is never dropped */
			atomic_store_explicit(&task->state, (uint8_t)STL_WQ_RUNNING, memory_order_release);
			fn(arg);
			atomic_store_explicit(&task->state, (uint8_t)STL_WQ_DONE, memory_order_release);
			atomic_fetch_sub_explicit(&wq->in_flight, 1u, memory_order_acq_rel);
			return 0;
		} else {
			pos = atomic_load_explicit(&wq->head, memory_order_relaxed);
		}
	}

	atomic_store_explicit(&slot->task_ptr, (uintptr_t)task, memory_order_relaxed);
	atomic_store_explicit(&slot->seq, pos + 1u, memory_order_release);

	if (wq->nthreads > 0) {
#if defined(__wasm__)
		atomic_fetch_add_explicit(&wq->futex_word, 1, memory_order_release);
#if defined(__wasm_atomics__)
		__builtin_wasm_memory_atomic_notify((int *)&wq->futex_word, 1);
#endif
#elif defined(_WIN32)
		AcquireSRWLockExclusive(&wq->lock);
		WakeConditionVariable(&wq->cv_work);
		ReleaseSRWLockExclusive(&wq->lock);
#else
		pthread_mutex_lock(&wq->lock);
		pthread_cond_signal(&wq->cv_work);
		pthread_mutex_unlock(&wq->lock);
#endif
	}
	return 0;
}

ISO_EXPORT(unsigned int, stl_wq_step, stl_wq_t *wq, unsigned int max_tasks)
{
	unsigned int ran = 0;
	if (max_tasks == 0)
		max_tasks = 0xFFFFFFFFu;

	if (wq) {
		while (ran < max_tasks && stl_wq_pop_and_run(wq))
			ran++;
		return ran;
	}

	/* When wq == NULL, step across all active registered workqueues */
	for (stl_wq_t *cur = s_active_wq_head; cur && ran < max_tasks; cur = cur->next_active) {
		while (ran < max_tasks && stl_wq_pop_and_run(cur))
			ran++;
	}
	return ran;
}

ISO_EXPORT(void, stl_wq_wait, stl_wq_t *wq, stl_wq_task_t *task)
{
	if (!task)
		return;
	while (!stl_wq_done(task)) {
		if (!wq || !stl_wq_pop_and_run(wq)) {
#if !defined(__wasm__) && defined(_WIN32)
			SwitchToThread();
#elif !defined(__wasm__)
			sched_yield();
#else
			break;
#endif
		}
	}
}

ISO_EXPORT(void, stl_wq_drain, stl_wq_t *wq)
{
	if (!wq)
		return;
	while (atomic_load_explicit(&wq->in_flight, memory_order_acquire) > 0u) {
		if (stl_wq_pop_and_run(wq))
			continue;
		if (wq->nthreads > 0) {
#if !defined(__wasm__) && defined(_WIN32)
			AcquireSRWLockExclusive(&wq->lock);
			if (atomic_load_explicit(&wq->in_flight, memory_order_acquire) > 0u &&
			    atomic_load_explicit(&wq->head, memory_order_acquire) ==
			    atomic_load_explicit(&wq->tail, memory_order_acquire)) {
				SleepConditionVariableSRW(&wq->cv_idle, &wq->lock, 1, 0);
			}
			ReleaseSRWLockExclusive(&wq->lock);
#elif !defined(__wasm__)
			pthread_mutex_lock(&wq->lock);
			if (atomic_load_explicit(&wq->in_flight, memory_order_acquire) > 0u &&
			    atomic_load_explicit(&wq->head, memory_order_acquire) ==
			    atomic_load_explicit(&wq->tail, memory_order_acquire)) {
				pthread_cond_wait(&wq->cv_idle, &wq->lock);
			}
			pthread_mutex_unlock(&wq->lock);
#else
			break;
#endif
		} else {
			break;
		}
	}
}

ISO_EXPORT(void, stl_wq_destroy, stl_wq_t *wq)
{
	if (!wq)
		return;

	stl_wq_drain(wq);
	atomic_store_explicit(&wq->stop, 1u, memory_order_release);

	stl_wq_t **pp = &s_active_wq_head;
	while (*pp) {
		if (*pp == wq) {
			*pp = wq->next_active;
			break;
		}
		pp = &(*pp)->next_active;
	}

#if defined(__wasm__)
	atomic_fetch_add_explicit(&wq->futex_word, 1, memory_order_release);
#if defined(__wasm_atomics__)
	__builtin_wasm_memory_atomic_notify((int *)&wq->futex_word, wq->nthreads);
#endif
#elif defined(_WIN32)
	AcquireSRWLockExclusive(&wq->lock);
	WakeAllConditionVariable(&wq->cv_work);
	ReleaseSRWLockExclusive(&wq->lock);
	for (unsigned int i = 0; i < wq->nthreads; i++) {
		WaitForSingleObject(wq->threads[i], INFINITE);
		CloseHandle(wq->threads[i]);
	}
#else
	pthread_mutex_lock(&wq->lock);
	pthread_cond_broadcast(&wq->cv_work);
	pthread_mutex_unlock(&wq->lock);
	for (unsigned int i = 0; i < wq->nthreads; i++) {
		pthread_join(wq->threads[i], NULL);
	}
	pthread_mutex_destroy(&wq->lock);
	pthread_cond_destroy(&wq->cv_work);
	pthread_cond_destroy(&wq->cv_idle);
#endif

	free(wq->ring);
	free(wq);
}
