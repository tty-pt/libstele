/*
 * Copyright (c) 2026, tty-pt
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 */

#include "../include/ttypt/qsys_wasm.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_REGISTERED_MODULES 64

#ifdef __GNUC__
#define WEAK_DECL __attribute__((weak))
#else
#define WEAK_DECL
#endif

/* Forward declarations for optional libbud VDOM integration */
typedef struct bud_node bud_node;
typedef struct bud_runtime bud_runtime;
typedef int (*bud_emit_fn)(void *user, const char *op, const char *a, const char *b, const char *c);

WEAK_DECL bud_runtime *bud_runtime_new(bud_node *root);
WEAK_DECL void bud_runtime_free(bud_runtime *runtime);
WEAK_DECL bud_node *bud_runtime_root(const bud_runtime *runtime);
WEAK_DECL int bud_runtime_mount(bud_runtime *runtime);
WEAK_DECL int bud_vdom_diff(bud_node *old_root, bud_node *new_root, bud_emit_fn emit, void *user);
WEAK_DECL unsigned int bud_node_id(const bud_node *node);
WEAK_DECL const bud_node *bud_node_child(const bud_node *node, size_t index);
WEAK_DECL size_t bud_node_child_count(const bud_node *node);
WEAK_DECL int bud_runtime_dispatch(bud_runtime *runtime, bud_node *target, const char *event, void *event_user);
WEAK_DECL int bud_render_ops(const bud_node *root, bud_emit_fn emit, void *user);
WEAK_DECL int bud_sprint_tree(const bud_node *node, char *buf, size_t bufsz);

/* Function pointers declared in bud.h */
extern void (*bud_host_fetch_fn)(const char *url, size_t url_len, int request_id) WEAK_DECL;
extern void (*bud_host_log_fn)(const char *msg, size_t msg_len) WEAK_DECL;
extern void (*bud_host_set_location_fn)(const char *url, size_t url_len) WEAK_DECL;

#if defined(__wasm__)
ISO_IMPORT(void, bud, bud_host_emit_patch,
        const char *op, size_t op_len, const char *a, size_t a_len,
        const char *b, size_t b_len, const char *c, size_t c_len);
ISO_IMPORT(void, bud, bud_host_fetch, const char *url, size_t url_len, int request_id);
ISO_IMPORT(void, bud, bud_host_log, const char *msg, size_t msg_len);
ISO_IMPORT(void, bud, bud_host_set_location, const char *url, size_t url_len);
#endif

/* Slot runtime state */
typedef struct qsys_slot {
    int active;
    const qsys_module_ops_t *ops;
    bud_runtime *runtime;
    char name[64];
} qsys_slot_t;

static qsys_slot_t g_slots[QSYS_MAX_SLOTS];
static const qsys_module_ops_t *g_registered[MAX_REGISTERED_MODULES];
static int g_registered_count = 0;

/* Weakly linked table from application site if present */
extern const qsys_module_ops_t *g_site_modules[] WEAK_DECL;

static int noop_emit(void *user, const char *op, const char *a, const char *b, const char *c)
{
    (void)user; (void)op; (void)a; (void)b; (void)c;
    return 0;
}

#if defined(__wasm__)
static int emit_slot_patch(void *user, const char *op, const char *a, const char *b, const char *c)
{
    int slot = (int)(intptr_t)user;
    qsys_host_emit_patch(slot,
                         op, op ? strlen(op) : 0,
                         a, a ? strlen(a) : 0,
                         b, b ? strlen(b) : 0,
                         c, c ? strlen(c) : 0);
    bud_host_emit_patch(op, op ? strlen(op) : 0,
                        a, a ? strlen(a) : 0,
                        b, b ? strlen(b) : 0,
                        c, c ? strlen(c) : 0);
    return 0;
}
#endif

ISO_EXPORT(int, qsys_kernel_register_module, const qsys_module_ops_t *ops)
{
    if (!ops || !ops->name) return -1;
    for (int i = 0; i < g_registered_count; i++) {
        if (strcmp(g_registered[i]->name, ops->name) == 0) {
            g_registered[i] = ops;
            return 0;
        }
    }
    if (g_registered_count < MAX_REGISTERED_MODULES) {
        g_registered[g_registered_count++] = ops;
        return 0;
    }
    return -1;
}

static const qsys_module_ops_t *qsys_find_module(const char *name)
{
    if (!name) return NULL;
    for (int i = 0; i < g_registered_count; i++) {
        if (strcmp(g_registered[i]->name, name) == 0) {
            return g_registered[i];
        }
    }
    /* Fallback search in static site table */
    if (g_site_modules) {
        for (int i = 0; g_site_modules[i] != NULL; i++) {
            if (strcmp(g_site_modules[i]->name, name) == 0) {
                return g_site_modules[i];
            }
        }
    }
    return NULL;
}

ISO_EXPORT(int, qsys_kernel_boot, void)
{
    memset(g_slots, 0, sizeof(g_slots));
    g_registered_count = 0;

#if defined(__wasm__)
    if (&bud_host_fetch_fn != NULL)
        bud_host_fetch_fn = bud_host_fetch;
    if (&bud_host_log_fn != NULL)
        bud_host_log_fn = bud_host_log;
    if (&bud_host_set_location_fn != NULL)
        bud_host_set_location_fn = bud_host_set_location;
#endif

    /* Enumerate static site modules if present */
    if (g_site_modules) {
        for (int i = 0; g_site_modules[i] != NULL; i++) {
            qsys_kernel_register_module(g_site_modules[i]);
        }
    }
    return 0;
}

ISO_EXPORT(int, qsys_kernel_mount, int slot, const char *name, const char *json, int len)
{
    if (slot < 0 || slot >= QSYS_MAX_SLOTS) return -1;
    const qsys_module_ops_t *ops = qsys_find_module(name);
    if (!ops) return -2;

    /* Unmount existing module on this slot */
    if (g_slots[slot].active) {
        qsys_kernel_unmount(slot);
    }

    g_slots[slot].ops = ops;
    g_slots[slot].active = 1;
    strncpy(g_slots[slot].name, name, sizeof(g_slots[slot].name) - 1);
    g_slots[slot].name[sizeof(g_slots[slot].name) - 1] = '\0';

    if (ops->init) {
        ops->init(json, len);
    }

    if (ops->render && bud_runtime_new != NULL && bud_runtime_mount != NULL) {
        bud_node *root = (bud_node *)ops->render();
        if (root) {
            if (bud_render_ops != NULL) {
                bud_render_ops(root, noop_emit, NULL);
            }
            g_slots[slot].runtime = bud_runtime_new(root);
            bud_runtime_mount(g_slots[slot].runtime);
        }
    }
    return 0;
}

ISO_EXPORT(int, qsys_kernel_update, int slot)
{
    if (slot < 0 || slot >= QSYS_MAX_SLOTS || !g_slots[slot].active) return -1;
    qsys_slot_t *s = &g_slots[slot];

#if defined(__wasm__)
    if (s->runtime && s->ops->render && bud_runtime_root && bud_vdom_diff && bud_runtime_free && bud_runtime_new) {
        bud_node *old_root = bud_runtime_root(s->runtime);
        bud_node *new_root = (bud_node *)s->ops->render();
        if (new_root) {
            bud_vdom_diff(old_root, new_root, emit_slot_patch, (void *)(intptr_t)slot);
            bud_runtime_free(s->runtime);
            s->runtime = bud_runtime_new(new_root);
            bud_runtime_mount(s->runtime);
        }
    }
#endif
    return 0;
}

ISO_EXPORT(int, qsys_kernel_action, int slot, int action_id, const char *data)
{
    if (slot < 0 || slot >= QSYS_MAX_SLOTS || !g_slots[slot].active) return -1;
    qsys_slot_t *s = &g_slots[slot];

    if (s->ops->action) {
        s->ops->action(action_id, data);
    }
    return qsys_kernel_update(slot);
}

ISO_EXPORT(int, qsys_kernel_unmount, int slot)
{
    if (slot < 0 || slot >= QSYS_MAX_SLOTS) return -1;
    if (!g_slots[slot].active) return 0;
    qsys_slot_t *s = &g_slots[slot];

    if (s->ops && s->ops->cleanup) {
        s->ops->cleanup();
    }
    if (s->runtime && bud_runtime_free != NULL) {
        bud_runtime_free(s->runtime);
        s->runtime = NULL;
    }
    s->active = 0;
    s->ops = NULL;
    s->name[0] = '\0';
    return 0;
}

ISO_EXPORT(void, qsys_kernel_tick, float dt, int width, int height)
{
    for (int i = 0; i < QSYS_MAX_SLOTS; i++) {
        if (g_slots[i].active && g_slots[i].ops && g_slots[i].ops->tick) {
            g_slots[i].ops->tick(dt, width, height);
        }
    }
}

ISO_EXPORT(void, wasm_fetch_callback, int request_id, const char *data, int data_len)
{
    for (int i = 0; i < QSYS_MAX_SLOTS; i++) {
        if (g_slots[i].active && g_slots[i].ops && g_slots[i].ops->fetch_callback) {
            g_slots[i].ops->fetch_callback(request_id, data, data_len);
        }
    }
}

static bud_node *find_node_by_id(const bud_node *node, unsigned int id)
{
    if (!node) return NULL;
    if (bud_node_id && bud_node_id(node) == id)
        return (bud_node *)node;
    if (bud_node_child_count && bud_node_child) {
        size_t count = bud_node_child_count(node);
        for (size_t i = 0; i < count; i++) {
            bud_node *found = find_node_by_id(bud_node_child(node, i), id);
            if (found) return found;
        }
    }
    return NULL;
}

ISO_EXPORT(int, qsys_kernel_dispatch_event, int slot, unsigned int node_id, const char *event_name, int bubbles, void *event_data)
{
    (void)bubbles;
    if (slot < 0 || slot >= QSYS_MAX_SLOTS || !g_slots[slot].active || !g_slots[slot].runtime)
        return -1;
    if (!bud_runtime_root || !bud_runtime_dispatch)
        return -2;
    bud_node *root = bud_runtime_root(g_slots[slot].runtime);
    bud_node *target = find_node_by_id(root, node_id);
    if (!target)
        return -3;
    return bud_runtime_dispatch(g_slots[slot].runtime, target, event_name, event_data);
}

ISO_EXPORT_AS(int, wasm_dispatch_event, bud_app_dispatch,
        unsigned int node_id, const char *event_name, int bubbles,
        void *event_data)
{
    (void)bubbles;
    int order[] = { 1, 0, 2, 3 };
    for (size_t i = 0; i < sizeof(order)/sizeof(order[0]); i++) {
        int s = order[i];
        if (s < QSYS_MAX_SLOTS && g_slots[s].active && g_slots[s].runtime && bud_runtime_root) {
            bud_node *root = bud_runtime_root(g_slots[s].runtime);
            bud_node *target = find_node_by_id(root, node_id);
            if (target && bud_runtime_dispatch) {
                return bud_runtime_dispatch(g_slots[s].runtime, target, event_name, event_data);
            }
        }
    }
    return -1;
}

void bud_app_set_state(void)
{
    if (g_slots[1].active) {
        qsys_kernel_update(1);
    } else if (g_slots[0].active) {
        qsys_kernel_update(0);
    }
}

ISO_EXPORT_AS(void, wasm_update, bud_app_update)
{
    bud_app_set_state();
}

ISO_EXPORT_AS(void, wasm_mount, bud_app_mount)
{
    if (g_slots[1].active && g_slots[1].runtime && bud_runtime_mount) {
        bud_runtime_mount(g_slots[1].runtime);
    }
    if (g_slots[0].active && g_slots[0].runtime && bud_runtime_mount) {
        bud_runtime_mount(g_slots[0].runtime);
    }
}

ISO_EXPORT_AS(void, wasm_unmount, bud_app_unmount)
{
    qsys_kernel_unmount(1);
    qsys_kernel_unmount(0);
}

void bud_patch_attr(bud_node *node, const char *name, const char *value)
{
    if (!node || !name || !value) return;
    char id_str[16];
    snprintf(id_str, sizeof(id_str), "%u", bud_node_id ? bud_node_id(node) : 0);
    bud_host_emit_patch("patch-attr", 10, id_str, strlen(id_str), name, strlen(name), value, strlen(value));
}

void bud_patch_text(bud_node *node, const char *value)
{
    if (!node || !value) return;
    char id_str[16];
    snprintf(id_str, sizeof(id_str), "%u", bud_node_id ? bud_node_id(node) : 0);
    bud_host_emit_patch("patch-text", 10, value, strlen(value), id_str, strlen(id_str), NULL, 0);
}

ISO_EXPORT(void, bud_patch_innerhtml, unsigned int node_id, const char *html)
{
    if (!html) return;
    char id_str[16];
    snprintf(id_str, sizeof(id_str), "%u", node_id);
    bud_host_emit_patch("patch-innerhtml", 15, id_str, strlen(id_str), html, strlen(html), NULL, 0);
}

ISO_EXPORT(void, wasm_tick, float dt, int width, int height)
{
    qsys_kernel_tick(dt, width, height);
}

static char wasm_dbg_buf[65536];

ISO_EXPORT(const char *, wasm_get_tree_text, void)
{
    int s = g_slots[1].active ? 1 : (g_slots[0].active ? 0 : -1);
    if (s < 0 || !g_slots[s].runtime || !bud_runtime_root || !bud_sprint_tree) {
        wasm_dbg_buf[0] = '\0';
        return wasm_dbg_buf;
    }
    bud_node *root = bud_runtime_root(g_slots[s].runtime);
    if (!root) {
        wasm_dbg_buf[0] = '\0';
        return wasm_dbg_buf;
    }
    bud_sprint_tree(root, wasm_dbg_buf, sizeof(wasm_dbg_buf));
    return wasm_dbg_buf;
}

ISO_EXPORT(void, wasm_init, const char *json, int len)
{
    if (g_slots[1].active && g_slots[1].ops && g_slots[1].ops->init) {
        g_slots[1].ops->init(json, len);
    }
}

ISO_EXPORT(uint64_t, qsys_kernel_version, void)
{
    return 2;
}
