/*
 * Copyright (c) 2026, tty-pt
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 */

#include "../include/ttypt/stele_wasm.h"
#include <string.h>
#include <stdlib.h>

#define MAX_STL_EVENT_LISTENERS 64

typedef struct stl_event_listener {
    int active;
    char topic[64];
    stl_event_fn fn;
    void *user_data;
} stl_event_listener_t;

static stl_event_listener_t g_stl_listeners[MAX_STL_EVENT_LISTENERS];

ISO_EXPORT(int, stl_event_on, const char *topic, stl_event_fn handler, void *user_data)
{
    if (!topic || !handler) return -1;
    for (int i = 0; i < MAX_STL_EVENT_LISTENERS; i++) {
        if (!g_stl_listeners[i].active) {
            g_stl_listeners[i].active = 1;
            strncpy(g_stl_listeners[i].topic, topic, sizeof(g_stl_listeners[i].topic) - 1);
            g_stl_listeners[i].topic[sizeof(g_stl_listeners[i].topic) - 1] = '\0';
            g_stl_listeners[i].fn = handler;
            g_stl_listeners[i].user_data = user_data;
            return 0;
        }
    }
    return -2; /* table full */
}

ISO_EXPORT(int, stl_event_off, const char *topic, stl_event_fn handler)
{
    if (!topic) return -1;
    int count = 0;
    for (int i = 0; i < MAX_STL_EVENT_LISTENERS; i++) {
        if (g_stl_listeners[i].active && strcmp(g_stl_listeners[i].topic, topic) == 0) {
            if (!handler || g_stl_listeners[i].fn == handler) {
                g_stl_listeners[i].active = 0;
                g_stl_listeners[i].fn = NULL;
                g_stl_listeners[i].topic[0] = '\0';
                count++;
            }
        }
    }
    return count;
}

static void stl_dispatch_local_event(const char *topic, const void *payload, size_t len)
{
    for (int i = 0; i < MAX_STL_EVENT_LISTENERS; i++) {
        if (g_stl_listeners[i].active) {
            if (strcmp(g_stl_listeners[i].topic, topic) == 0 ||
                strcmp(g_stl_listeners[i].topic, "*") == 0) {
                g_stl_listeners[i].fn(topic, payload, len, g_stl_listeners[i].user_data);
            }
        }
    }
}

ISO_EXPORT(int, stl_on_event, const char *topic, size_t topic_len, const void *payload, size_t payload_len)
{
    (void)topic_len;
    if (!topic) return -1;
    stl_dispatch_local_event(topic, payload, payload_len);
    return 0;
}

ISO_EXPORT(int, stl_event_emit, const char *topic, const void *payload, size_t len)
{
    if (!topic) return -1;

    /* 1. Dispatch locally to in-instance listeners */
    stl_dispatch_local_event(topic, payload, len);

    /* 2. Broadcast via host to sibling slot instances */
#if defined(__wasm__)
    stl_host_event_emit(topic, strlen(topic), payload, len);
#endif
    return 0;
}

/* stele_* aliases */
ISO_EXPORT(int, stele_event_on, const char *topic, stl_event_fn handler, void *user_data)
{
    return stl_event_on(topic, handler, user_data);
}

ISO_EXPORT(int, stele_event_off, const char *topic, stl_event_fn handler)
{
    return stl_event_off(topic, handler);
}

ISO_EXPORT(int, stele_on_event, const char *topic, size_t topic_len, const void *payload, size_t payload_len)
{
    return stl_on_event(topic, topic_len, payload, payload_len);
}

ISO_EXPORT(int, stele_event_emit, const char *topic, const void *payload, size_t len)
{
    return stl_event_emit(topic, payload, len);
}
