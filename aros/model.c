// SPDX-License-Identifier: BSD-2-Clause
// SPDX-FileCopyrightText: 2026 John Knipper

/* xSysInfo for AROS: data model and its formatting. Portable C99. */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "model.h"

static const char *const section_titles[XSI_SEC_COUNT] = {
    "System", "Processor", "Memory", "Platform", "Display", "Benchmarks"
};
static const char *const section_ids[XSI_SEC_COUNT] = {
    "system", "cpu", "memory", "platform", "display", "benchmark"
};
static const char *const list_titles[XSI_LIST_COUNT] = {
    "Libraries", "Devices", "Resources"
};

void xsi_model_init(xsi_model *m)
{
    int i;
    memset(m, 0, sizeof *m);
    m->platform = XSI_PLATFORM_UNKNOWN;
    m->platform_name = "common";
    m->pointer_bits = (unsigned)(sizeof(void *) * 8);
    for (i = 0; i < XSI_SEC_COUNT; i++) {
        m->sections[i].id = section_ids[i];
        m->sections[i].title = section_titles[i];
    }
    for (i = 0; i < XSI_LIST_COUNT; i++)
        m->lists[i].title = list_titles[i];
}

static xsi_field *new_field(xsi_model *m, int section, const char *key,
                            const char *label, const char *source)
{
    xsi_section *s;
    xsi_field *f;
    if (section < 0 || section >= XSI_SEC_COUNT)
        return NULL;
    s = &m->sections[section];
    if (s->count >= XSI_MAX_FIELDS) {
        s->dropped++;
        return NULL;
    }
    f = &s->fields[s->count++];
    memset(f, 0, sizeof *f);
    f->key = key;
    f->label = label;
    f->source = source ? source : "-";
    return f;
}

xsi_field *xsi_set(xsi_model *m, int section, const char *key, const char *label,
                   const char *source, const char *format, ...)
{
    xsi_field *f = new_field(m, section, key, label, source);
    va_list args;
    if (!f)
        return NULL;
    va_start(args, format);
    vsnprintf(f->value, sizeof f->value, format, args);
    va_end(args);
    f->state = XSI_OK;
    return f;
}

xsi_field *xsi_missing(xsi_model *m, int section, const char *key, const char *label,
                       xsi_state state, const char *source)
{
    xsi_field *f = new_field(m, section, key, label, source);
    if (!f)
        return NULL;
    f->state = state == XSI_OK ? XSI_UNKNOWN : state;
    return f;
}

const xsi_field *xsi_find(const xsi_model *m, const char *key)
{
    int i;
    unsigned j;
    for (i = 0; i < XSI_SEC_COUNT; i++)
        for (j = 0; j < m->sections[i].count; j++)
            if (strcmp(m->sections[i].fields[j].key, key) == 0)
                return &m->sections[i].fields[j];
    return NULL;
}

xsi_module *xsi_add_module(xsi_model *m, int list)
{
    xsi_module_list *l;
    if (list < 0 || list >= XSI_LIST_COUNT)
        return NULL;
    l = &m->lists[list];
    if (l->count >= XSI_MAX_MODULES) {
        l->dropped++;
        return NULL;
    }
    memset(&l->items[l->count], 0, sizeof l->items[0]);
    return &l->items[l->count++];
}

xsi_memregion *xsi_add_region(xsi_model *m)
{
    if (m->region_count >= XSI_MAX_REGIONS) {
        m->regions_dropped++;
        return NULL;
    }
    memset(&m->regions[m->region_count], 0, sizeof m->regions[0]);
    return &m->regions[m->region_count++];
}

xsi_volume *xsi_add_volume(xsi_model *m)
{
    if (m->volume_count >= XSI_MAX_VOLUMES) {
        m->volumes_dropped++;
        return NULL;
    }
    memset(&m->volumes[m->volume_count], 0, sizeof m->volumes[0]);
    m->volumes[m->volume_count].state = XSI_VOL_UNKNOWN;
    return &m->volumes[m->volume_count++];
}

static int ascii_nocase(const char *a, const char *b)
{
    for (;; a++, b++) {
        int x = (*a >= 'A' && *a <= 'Z') ? *a + 32 : (unsigned char)*a;
        int y = (*b >= 'A' && *b <= 'Z') ? *b + 32 : (unsigned char)*b;
        if (x != y || x == 0)
            return x - y;
    }
}

static int by_name(const void *a, const void *b)
{
    return ascii_nocase(((const xsi_module *)a)->name, ((const xsi_module *)b)->name);
}

void xsi_sort_modules(xsi_module_list *list)
{
    qsort(list->items, list->count, sizeof list->items[0], by_name);
}

/* Binary units, one truncated decimal below 100 of a unit: "512 B", "63.5 KB",
 * "15.9 GB". Exact to the byte below 1 KB; never overflows for any uint64. */
char *xsi_format_size(uint64_t bytes, char *buf, size_t size)
{
    static const char *const units[] = { "B", "KB", "MB", "GB", "TB", "PB", "EB" };
    unsigned u = 0;
    uint64_t whole = bytes, rest = 0;
    while (whole >= 1024 && u < 6) {
        rest = whole & 1023;
        whole >>= 10;
        u++;
    }
    if (u == 0 || whole >= 100)
        snprintf(buf, size, "%llu %s", (unsigned long long)whole, units[u]);
    else
        snprintf(buf, size, "%llu.%llu %s", (unsigned long long)whole,
                 (unsigned long long)(rest * 10 / 1024), units[u]);
    return buf;
}

/* Full-width hex for the build's pointer size, so a 64-bit address is
 * never shown truncated: 0x00000008_12345678 on AArch64. */
char *xsi_format_address(uint64_t address, unsigned pointer_bits, char *buf, size_t size)
{
    if (pointer_bits > 32)
        snprintf(buf, size, "0x%08lx_%08lx", (unsigned long)(address >> 32),
                 (unsigned long)(address & 0xffffffffUL));
    else
        snprintf(buf, size, "0x%08lx", (unsigned long)(address & 0xffffffffUL));
    return buf;
}

const char *xsi_state_name(xsi_state state)
{
    switch (state) {
    case XSI_OK: return "ok";
    case XSI_UNAVAILABLE: return "unavailable";
    case XSI_UNKNOWN: return "unknown";
    case XSI_NOT_APPLICABLE: return "not applicable";
    }
    return "unknown";
}

const char *xsi_volume_state_name(xsi_volume_state state)
{
    switch (state) {
    case XSI_VOL_OK: return "read/write";
    case XSI_VOL_READ_ONLY: return "read-only";
    case XSI_VOL_NO_DISK: return "no disk";
    case XSI_VOL_BUSY: return "busy";
    case XSI_VOL_UNKNOWN: return "unknown";
    }
    return "unknown";
}

const char *xsi_field_text(const xsi_field *f)
{
    switch (f->state) {
    case XSI_OK: return f->value;
    case XSI_UNAVAILABLE: return "[unavailable]";
    case XSI_UNKNOWN: return "[unknown]";
    case XSI_NOT_APPLICABLE: return "[n/a]";
    }
    return "[unknown]";
}
