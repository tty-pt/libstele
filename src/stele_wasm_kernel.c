/*
 * Copyright (c) 2026, tty-pt
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 */

#include "../include/ttypt/stele_wasm.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#define MAX_REGISTERED_MODULES 64

#ifdef __GNUC__
#define WEAK_DECL __attribute__((weak))
#else
#define WEAK_DECL
#endif

/* Slot runtime state */
typedef struct stl_slot {
    int active;
    const stl_module_ops_t *ops;
    char name[64];
} stl_slot_t;

static stl_slot_t g_slots[STL_MAX_SLOTS];
static const stl_module_ops_t *g_registered[MAX_REGISTERED_MODULES];
static int g_registered_count = 0;

/* Weakly linked static module table from application if present */
extern const stl_module_ops_t *g_stl_modules[] WEAK_DECL;

ISO_EXPORT(int, stl_kernel_register_module, const stl_module_ops_t *ops)
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

static int stl_mod_matches(const char *mod_name, const char *query_name)
{
    if (!mod_name || !query_name) return 0;
    if (strcmp(mod_name, query_name) == 0) return 1;
    size_t qlen = strlen(query_name);
    if (strncmp(mod_name, query_name, qlen) == 0 && mod_name[qlen] == '_') return 1;
    return 0;
}

static const stl_module_ops_t *stl_find_module(const char *name)
{
    if (!name) return NULL;
    for (int i = 0; i < g_registered_count; i++) {
        if (stl_mod_matches(g_registered[i]->name, name)) {
            return g_registered[i];
        }
    }
    /* Fallback search in static module table */
    if (g_stl_modules) {
        for (int i = 0; g_stl_modules[i] != NULL; i++) {
            if (stl_mod_matches(g_stl_modules[i]->name, name)) {
                return g_stl_modules[i];
            }
        }
    }
    if (g_registered_count == 1) {
        return g_registered[0];
    }
    return NULL;
}

ISO_EXPORT(int, stl_kernel_boot, void)
{
    memset(g_slots, 0, sizeof(g_slots));

    /* Enumerate static modules if present */
    if (g_stl_modules) {
        for (int i = 0; g_stl_modules[i] != NULL; i++) {
            stl_kernel_register_module(g_stl_modules[i]);
        }
    }
    return 0;
}

ISO_EXPORT(int, stl_kernel_mount, int slot, const char *name, const char *json, int len)
{
    if (slot < 0 || slot >= STL_MAX_SLOTS) return -1;
    const stl_module_ops_t *ops = stl_find_module(name);
    if (!ops) return -2;

    /* Unmount existing module on this slot */
    if (g_slots[slot].active) {
        stl_kernel_unmount(slot);
    }

    g_slots[slot].ops = ops;
    g_slots[slot].active = 1;
    strncpy(g_slots[slot].name, name, sizeof(g_slots[slot].name) - 1);
    g_slots[slot].name[sizeof(g_slots[slot].name) - 1] = '\0';

    if (ops->init) {
        ops->init(json, len);
    }

    if (ops->mount) {
        ops->mount(slot);
    }
    return 0;
}

ISO_EXPORT(int, stl_kernel_mount_bin, int slot, const char *name, const void *bin, size_t len)
{
    if (slot < 0 || slot >= STL_MAX_SLOTS) return -1;
    const stl_module_ops_t *ops = stl_find_module(name);
    if (!ops) return -2;

    /* Unmount existing module on this slot */
    if (g_slots[slot].active) {
        stl_kernel_unmount(slot);
    }

    g_slots[slot].ops = ops;
    g_slots[slot].active = 1;
    strncpy(g_slots[slot].name, name, sizeof(g_slots[slot].name) - 1);
    g_slots[slot].name[sizeof(g_slots[slot].name) - 1] = '\0';

    if (ops->init_bin) {
        ops->init_bin(bin, len);
    } else if (ops->init) {
        ops->init((const char *)bin, (int)len);
    }

    if (ops->mount) {
        ops->mount(slot);
    }
    return 0;
}

ISO_EXPORT(int, stl_kernel_update, int slot)
{
    if (slot < 0 || slot >= STL_MAX_SLOTS || !g_slots[slot].active) return -1;
    stl_slot_t *s = &g_slots[slot];

    if (s->ops && s->ops->update) {
        return s->ops->update(slot);
    }
    return 0;
}

ISO_EXPORT(int, stl_kernel_action, int slot, int action_id, const char *data)
{
    if (slot < 0 || slot >= STL_MAX_SLOTS || !g_slots[slot].active) return -1;
    stl_slot_t *s = &g_slots[slot];

    if (s->ops && s->ops->action) {
        s->ops->action(action_id, data);
    }
    return stl_kernel_update(slot);
}

ISO_EXPORT(int, stl_kernel_unmount, int slot)
{
    if (slot < 0 || slot >= STL_MAX_SLOTS) return -1;
    if (!g_slots[slot].active) return 0;
    stl_slot_t *s = &g_slots[slot];

    if (s->ops && s->ops->cleanup) {
        s->ops->cleanup();
    }
    s->active = 0;
    s->ops = NULL;
    s->name[0] = '\0';
    return 0;
}

ISO_EXPORT(int, stl_kernel_dispatch_event, int slot, unsigned int node_id, const char *event_name, int bubbles, void *event_data)
{
    if (slot < 0 || slot >= STL_MAX_SLOTS || !g_slots[slot].active)
        return -1;
    stl_slot_t *s = &g_slots[slot];
    if (s->ops && s->ops->dispatch_event) {
        return s->ops->dispatch_event(node_id, event_name, bubbles, event_data);
    }
    return -2;
}

ISO_EXPORT(void, stl_kernel_tick, float dt, int width, int height)
{
    for (int i = 0; i < STL_MAX_SLOTS; i++) {
        if (g_slots[i].active && g_slots[i].ops && g_slots[i].ops->tick) {
            g_slots[i].ops->tick(dt, width, height);
        }
    }
}

ISO_EXPORT(int, stl_kernel_has_tick, void)
{
    for (int i = 0; i < STL_MAX_SLOTS; i++) {
        if (g_slots[i].active && g_slots[i].ops && g_slots[i].ops->tick) {
            return 1;
        }
    }
    return 0;
}

ISO_EXPORT(void, wasm_fetch_callback, int request_id, const char *data, int data_len)
{
    for (int i = 0; i < STL_MAX_SLOTS; i++) {
        if (g_slots[i].active && g_slots[i].ops && g_slots[i].ops->fetch_callback) {
            g_slots[i].ops->fetch_callback(request_id, data, data_len);
        }
    }
}

ISO_EXPORT(uint64_t, stl_kernel_version, void)
{
    return 2;
}

/* stele_* aliases */
ISO_EXPORT(int, stele_kernel_boot, void) { return stl_kernel_boot(); }
ISO_EXPORT(int, stele_kernel_mount, int slot, const char *name, const char *json, int len) {
    return stl_kernel_mount(slot, name, json, len);
}
ISO_EXPORT(int, stele_kernel_mount_bin, int slot, const char *name, const void *bin, size_t len) {
    return stl_kernel_mount_bin(slot, name, bin, len);
}
ISO_EXPORT(int, stele_kernel_unmount, int slot) { return stl_kernel_unmount(slot); }
ISO_EXPORT(int, stele_kernel_update, int slot) { return stl_kernel_update(slot); }
ISO_EXPORT(int, stele_kernel_action, int slot, int action_id, const char *data) {
    return stl_kernel_action(slot, action_id, data);
}
ISO_EXPORT(int, stele_kernel_dispatch_event, int slot, unsigned int node_id, const char *event_name, int bubbles, void *event_data) {
    return stl_kernel_dispatch_event(slot, node_id, event_name, bubbles, event_data);
}
ISO_EXPORT(void, stele_kernel_tick, float dt, int width, int height) {
    stl_kernel_tick(dt, width, height);
}
ISO_EXPORT(int, stele_kernel_has_tick, void) { return stl_kernel_has_tick(); }
ISO_EXPORT(uint64_t, stele_kernel_version, void) { return stl_kernel_version(); }
ISO_EXPORT(int, stele_kernel_register_module, const stl_module_ops_t *ops) {
    return stl_kernel_register_module(ops);
}
