/*
 * Copyright (c) 2026, tty-pt
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 */

#ifndef TTYPT_STELE_WASM_H
#define TTYPT_STELE_WASM_H

/**
 * @file stele_wasm.h
 * @brief Universal WebAssembly kernel substrate, slot manager, and module ABI.
 *
 * Canonical stele (stl_) substrate API providing single-instantiation runtime
 * for WebAssembly reactors, shared linear memory management, multi-slot UI hydration,
 * and declarative capability host imports.
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

/* Standard Substrate Slot Identifiers */
#define STL_SLOT_CHROME    0  /* Persistent chrome slot */
#define STL_SLOT_PAGE      1  /* Active route view slot */
#define STL_SLOT_MODAL     2  /* Dialogs, toasts, floating HUDs */
#define STL_MAX_SLOTS      4

#define STELE_SLOT_CHROME  STL_SLOT_CHROME
#define STELE_SLOT_PAGE    STL_SLOT_PAGE
#define STELE_SLOT_MODAL   STL_SLOT_MODAL
#define STELE_MAX_SLOTS    STL_MAX_SLOTS

/**
 * @brief Universal Isomorphic Module Descriptor
 */
typedef struct stl_module_ops {
    const char *name;                                   /* Module identifier (e.g. "app_shell", "item_detail") */
    int (*init)(const char *json, int len);             /* State hydration from SSR JSON */
    int (*init_bin)(const void *bin, size_t len);       /* Zero-copy binary state hydration */
    int (*mount)(int slot);                             /* Mount view in slot */
    int (*update)(int slot);                            /* Update view in slot */
    void (*action)(int action_id, const char *data);    /* Event action dispatcher */
    int (*dispatch_event)(unsigned int node_id, const char *event_name, int bubbles, void *event_data); /* DOM/UI event */
    void (*tick)(float dt, int width, int height);      /* Frame tick (canvas/animation) */
    void (*cleanup)(void);                              /* Teardown hook */
    void (*fetch_callback)(int request_id, const char *data, int data_len); /* Async fetch callback */
    void *(*render)(void);                              /* Generic render hook if present */
} stl_module_ops_t;

typedef stl_module_ops_t stele_module_ops_t;

/**
 * @brief Declarative Module Registration Macro
 */
#define STL_WASM_MODULE(mod_name, init_fn, render_fn, action_fn) \
    const stl_module_ops_t stl_mod_##mod_name = { \
        .name = #mod_name, \
        .init = init_fn, \
        .init_bin = NULL, \
        .mount = NULL, \
        .update = NULL, \
        .action = action_fn, \
        .dispatch_event = NULL, \
        .tick = NULL, \
        .cleanup = NULL, \
        .fetch_callback = NULL, \
        .render = (void *(*)(void))(render_fn) \
    }

#define STL_WASM_MODULE_BIN(mod_name, init_fn, init_bin_fn, render_fn, action_fn) \
    const stl_module_ops_t stl_mod_##mod_name = { \
        .name = #mod_name, \
        .init = init_fn, \
        .init_bin = init_bin_fn, \
        .mount = NULL, \
        .update = NULL, \
        .action = action_fn, \
        .dispatch_event = NULL, \
        .tick = NULL, \
        .cleanup = NULL, \
        .fetch_callback = NULL, \
        .render = (void *(*)(void))(render_fn) \
    }

#define STL_WASM_MODULE_EXT(mod_name, init_fn, render_fn, action_fn, tick_fn, cleanup_fn, fetch_fn) \
    const stl_module_ops_t stl_mod_##mod_name = { \
        .name = #mod_name, \
        .init = init_fn, \
        .init_bin = NULL, \
        .mount = NULL, \
        .update = NULL, \
        .action = action_fn, \
        .dispatch_event = NULL, \
        .tick = tick_fn, \
        .cleanup = cleanup_fn, \
        .fetch_callback = fetch_fn, \
        .render = (void *(*)(void))(render_fn) \
    }

#define STELE_WASM_MODULE STL_WASM_MODULE
#define STELE_WASM_MODULE_BIN STL_WASM_MODULE_BIN
#define STELE_WASM_MODULE_EXT STL_WASM_MODULE_EXT

/* Kernel Lifecycle & Substrate Operations */
ISO_EXPORT(int, stl_kernel_boot, void);
ISO_EXPORT(int, stl_kernel_mount, int slot, const char *name, const char *json, int len);
ISO_EXPORT(int, stl_kernel_mount_bin, int slot, const char *name, const void *bin, size_t len);
ISO_EXPORT(int, stl_kernel_unmount, int slot);
ISO_EXPORT(int, stl_kernel_update, int slot);
ISO_EXPORT(int, stl_kernel_action, int slot, int action_id, const char *data);
ISO_EXPORT(int, stl_kernel_dispatch_event, int slot, unsigned int node_id, const char *event_name, int bubbles, void *event_data);
ISO_EXPORT(void, stl_kernel_tick, float dt, int width, int height);
ISO_EXPORT(uint64_t, stl_kernel_version, void);
ISO_EXPORT(int, stl_kernel_register_module, const stl_module_ops_t *ops);
ISO_EXPORT(void, wasm_fetch_callback, int request_id, const char *data, int data_len);

/* stele_* aliases */
ISO_EXPORT_AS(int, stele_kernel_boot, stl_kernel_boot, void);
ISO_EXPORT_AS(int, stele_kernel_mount, stl_kernel_mount, int slot, const char *name, const char *json, int len);
ISO_EXPORT_AS(int, stele_kernel_mount_bin, stl_kernel_mount_bin, int slot, const char *name, const void *bin, size_t len);
ISO_EXPORT_AS(int, stele_kernel_unmount, stl_kernel_unmount, int slot);
ISO_EXPORT_AS(int, stele_kernel_update, stl_kernel_update, int slot);
ISO_EXPORT_AS(int, stele_kernel_action, stl_kernel_action, int slot, int action_id, const char *data);
ISO_EXPORT_AS(int, stele_kernel_dispatch_event, stl_kernel_dispatch_event, int slot, unsigned int node_id, const char *event_name, int bubbles, void *event_data);
ISO_EXPORT_AS(void, stele_kernel_tick, stl_kernel_tick, float dt, int width, int height);
ISO_EXPORT_AS(uint64_t, stele_kernel_version, stl_kernel_version, void);
ISO_EXPORT_AS(int, stele_kernel_register_module, stl_kernel_register_module, const stl_module_ops_t *ops);

/* Host Platform Imports */
#if defined(__wasm__)
ISO_IMPORT(void, stele, stl_host_emit_patch,
           int slot,
           const char *op, size_t op_len,
           const char *a, size_t a_len,
           const char *b, size_t b_len,
           const char *c, size_t c_len);

ISO_IMPORT(void, stele, stl_host_log, const char *msg, size_t len);
ISO_IMPORT(void, stele, stl_host_fetch, const char *url, size_t url_len, int req_id);
ISO_IMPORT(void, stele, stl_host_set_location, const char *url, size_t url_len);

/* Standard Generic Capability Imports */
ISO_IMPORT(int, audio, stl_audio_play, const char *url);
ISO_IMPORT(int, audio, stl_audio_pause, void);
ISO_IMPORT(int, audio, stl_audio_seek, double seconds);
ISO_IMPORT(double, audio, stl_audio_get_time, void);

ISO_IMPORT(int, storage, stl_storage_get, const char *key, char *buf, int max_len);
ISO_IMPORT(int, storage, stl_storage_set, const char *key, const char *val);

ISO_IMPORT(int, canvas, stl_canvas_blit, int canvas_id, const void *pixels, int w, int h);
ISO_IMPORT(int, clipboard, stl_clipboard_copy, const char *text);
ISO_IMPORT(void, stele, stl_host_invalidate, int slot);
ISO_IMPORT(void, stele, stl_host_event_emit,
           const char *topic, size_t topic_len,
           const void *payload, size_t payload_len);

static inline void stl_invalidate(void) { stl_host_invalidate(-1); }
static inline void stl_slot_invalidate(int slot) { stl_host_invalidate(slot); }

#define stele_invalidate stl_invalidate
#define stele_slot_invalidate stl_slot_invalidate
#define stele_host_emit_patch stl_host_emit_patch
#define stele_host_log stl_host_log
#define stele_host_fetch stl_host_fetch
#define stele_host_set_location stl_host_set_location
#define stele_host_invalidate stl_host_invalidate
#define stele_host_event_emit stl_host_event_emit

#else
static inline void stl_invalidate(void) {}
static inline void stl_slot_invalidate(int slot) { (void)slot; }
static inline void stele_invalidate(void) {}
static inline void stele_slot_invalidate(int slot) { (void)slot; }
#endif

/**
 * @brief In-WASM Cross-Slot Event Bus API
 */
typedef void (*stl_event_fn)(const char *topic, const void *payload, size_t len, void *user_data);

int stl_event_emit(const char *topic, const void *payload, size_t len);
int stl_event_on(const char *topic, stl_event_fn handler, void *user_data);
int stl_event_off(const char *topic, stl_event_fn handler);

ISO_EXPORT_AS(int, stele_event_emit, stl_event_emit, const char *topic, const void *payload, size_t len);
ISO_EXPORT_AS(int, stele_event_on, stl_event_on, const char *topic, stl_event_fn handler, void *user_data);
ISO_EXPORT_AS(int, stele_event_off, stl_event_off, const char *topic, stl_event_fn handler);

/* Exported hooks for event dispatch across WASM instances and host bridges */
ISO_EXPORT(int, stl_on_event,
           const char *topic, size_t topic_len,
           const void *payload, size_t payload_len);
ISO_EXPORT_AS(int, stele_on_event, stl_on_event,
           const char *topic, size_t topic_len,
           const void *payload, size_t payload_len);

#ifdef __cplusplus
}
#endif

#endif /* TTYPT_STELE_WASM_H */
