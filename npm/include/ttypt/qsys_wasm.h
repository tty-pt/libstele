/*
 * Copyright (c) 2026, tty-pt
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 */

#ifndef TTYPT_QSYS_WASM_H
#define TTYPT_QSYS_WASM_H

/**
 * @file qsys_wasm.h
 * @brief Universal WebAssembly kernel substrate, slot manager, and module ABI.
 *
 * Provides single-instantiation runtime substrate for WebAssembly reactors,
 * shared linear memory management, multi-slot UI hydration, and declarative
 * capability host imports.
 */

#include <stddef.h>
#include <stdint.h>

#if __has_include(<ttypt/iso.h>)
#include <ttypt/iso.h>
#else
#include "iso.h"
#endif

#ifndef WEAK
#if defined(__APPLE__)
#define WEAK __attribute__((weak_import))
#else
#define WEAK __attribute__((weak))
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration of bud_node for UI modules */
struct bud_node;

/* Standard Substrate Slot Identifiers */
#define QSYS_SLOT_CHROME  0  /* Persistent chrome slot (#chrome-root) */
#define QSYS_SLOT_PAGE    1  /* Active route view slot (#bud-root) */
#define QSYS_SLOT_MODAL   2  /* Dialogs, toasts, floating HUDs */
#define QSYS_MAX_SLOTS    4

/**
 * @brief Universal Isomorphic UI Module Descriptor
 */
typedef struct qsys_module_ops {
    const char *name;                                   /* e.g. "site_chrome", "song_detail" */
    int (*init)(const char *json, int len);             /* State hydration from SSR JSON */
    struct bud_node *(*render)(void);                   /* Pure-C VDOM generation */
    void (*action)(int action_id, const char *data);    /* Event action dispatcher */
    void (*tick)(float dt, int width, int height);      /* Frame tick (canvas/animation) */
    void (*cleanup)(void);                              /* Teardown hook */
    void (*fetch_callback)(int request_id, const char *data, int data_len); /* Async fetch callback */
} qsys_module_ops_t;

/**
 * @brief Declarative Module Registration Macro
 */
#define QSYS_WASM_MODULE(mod_name, init_fn, render_fn, action_fn) \
    const qsys_module_ops_t qsys_mod_##mod_name = { \
        .name = #mod_name, \
        .init = init_fn, \
        .render = (struct bud_node *(*)(void))(render_fn), \
        .action = action_fn, \
        .tick = NULL, \
        .cleanup = NULL, \
        .fetch_callback = NULL \
    }

#define QSYS_WASM_MODULE_EXT(mod_name, init_fn, render_fn, action_fn, tick_fn, cleanup_fn, fetch_fn) \
    const qsys_module_ops_t qsys_mod_##mod_name = { \
        .name = #mod_name, \
        .init = init_fn, \
        .render = (struct bud_node *(*)(void))(render_fn), \
        .action = action_fn, \
        .tick = tick_fn, \
        .cleanup = cleanup_fn, \
        .fetch_callback = fetch_fn \
    }

/* Kernel Lifecycle & Substrate Operations */
ISO_EXPORT(int, qsys_kernel_boot, void);
ISO_EXPORT(int, qsys_kernel_mount, int slot, const char *name, const char *json, int len);
ISO_EXPORT(int, qsys_kernel_unmount, int slot);
ISO_EXPORT(int, qsys_kernel_update, int slot);
ISO_EXPORT(int, qsys_kernel_action, int slot, int action_id, const char *data);
ISO_EXPORT(void, qsys_kernel_tick, float dt, int width, int height);
ISO_EXPORT(uint64_t, qsys_kernel_version, void);
ISO_EXPORT(int, qsys_kernel_register_module, const qsys_module_ops_t *ops);

/* Host Platform Imports */
#if defined(__wasm__)
ISO_IMPORT(void, qsys, qsys_host_emit_patch,
           int slot,
           const char *op, size_t op_len,
           const char *a, size_t a_len,
           const char *b, size_t b_len,
           const char *c, size_t c_len);

ISO_IMPORT(void, qsys, qsys_host_log, const char *msg, size_t len);
ISO_IMPORT(void, qsys, qsys_host_fetch, const char *url, size_t url_len, int req_id);
ISO_IMPORT(void, qsys, qsys_host_set_location, const char *url, size_t url_len);

/* Standard Generic Capability Imports */
ISO_IMPORT(int, audio, qsys_audio_play, const char *url);
ISO_IMPORT(int, audio, qsys_audio_pause, void);
ISO_IMPORT(int, audio, qsys_audio_seek, double seconds);
ISO_IMPORT(double, audio, qsys_audio_get_time, void);

ISO_IMPORT(int, storage, qsys_storage_get, const char *key, char *buf, int max_len);
ISO_IMPORT(int, storage, qsys_storage_set, const char *key, const char *val);

ISO_IMPORT(int, canvas, qsys_canvas_blit, int canvas_id, const void *pixels, int w, int h);
ISO_IMPORT(int, clipboard, qsys_clipboard_copy, const char *text);
#endif

#ifdef __cplusplus
}
#endif

#endif /* TTYPT_QSYS_WASM_H */
