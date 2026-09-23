// SPDX-License-Identifier: BSD-2-Clause
// SPDX-FileCopyrightText: 2026 John Knipper

/*
 * xSysInfo for AROS: the data model shared by the GUI and the text report.
 *
 * Plain C99 with fixed-width types, no AROS headers: collectors fill it on
 * the target, fixtures fill it in host tests. Addresses and sizes are 64-bit
 * whatever the pointer width. A value that could not be read is never zero:
 * every field carries its state and the source it came from.
 */

#ifndef XSI_MODEL_H
#define XSI_MODEL_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    XSI_OK,              /* read from the named source */
    XSI_UNAVAILABLE,     /* this system offers no way to read it */
    XSI_UNKNOWN,         /* a source exists but gave no answer */
    XSI_NOT_APPLICABLE   /* meaningless on this system (e.g. host-owned) */
} xsi_state;

#define XSI_VALUE_LEN   96
#define XSI_NAME_LEN    64

typedef struct {
    const char *key;            /* stable identifier, e.g. "cpu.model" */
    const char *label;          /* shown to the user */
    char value[XSI_VALUE_LEN];  /* empty unless state is XSI_OK */
    xsi_state state;
    const char *source;         /* "exec", "kernel.resource", "host", ... */
} xsi_field;

#define XSI_MAX_FIELDS  24

typedef struct {
    const char *id;             /* "system", "cpu", "platform", ... */
    const char *title;
    xsi_field fields[XSI_MAX_FIELDS];
    unsigned count;
    unsigned dropped;           /* fields that did not fit */
} xsi_section;

typedef struct {
    char name[XSI_NAME_LEN];
    uint64_t address;
    uint16_t version, revision;
    int has_version;            /* 0: the node is not known to be a Library */
    int8_t priority;
} xsi_module;

#define XSI_MAX_MODULES 256

typedef struct {
    const char *title;
    xsi_module items[XSI_MAX_MODULES];
    unsigned count;
    unsigned dropped;
} xsi_module_list;

typedef struct {
    char name[XSI_NAME_LEN];
    uint64_t lower, upper;      /* [lower, upper) as the MemHeader states */
    uint64_t total, free;
    uint32_t attributes;        /* MEMF_* bits as reported */
    int8_t priority;
    uint32_t chunks;            /* free chunks; only when freelist_known */
    uint64_t largest_free;      /* largest free chunk; only when freelist_known */
    int freelist_known;         /* 0: the free list was not walked, both unknown */
} xsi_memregion;

#define XSI_MAX_REGIONS 256     /* the M1 has 137 MemHeaders */

typedef enum {
    XSI_VOL_OK,
    XSI_VOL_READ_ONLY,
    XSI_VOL_NO_DISK,
    XSI_VOL_BUSY,
    XSI_VOL_UNKNOWN
} xsi_volume_state;

typedef struct {
    char device[32];            /* "WORK:" */
    int name_cut;               /* device name longer than the field */
    char volume[XSI_NAME_LEN];  /* label, or empty */
    char handler[XSI_NAME_LEN]; /* "fat-handler", or empty */
    uint32_t block_size;        /* bytes; 0 when unknown */
    uint64_t total_blocks, used_blocks;
    uint32_t dos_type;
    int has_info;               /* Info() answered */
    xsi_volume_state state;
} xsi_volume;

#define XSI_MAX_VOLUMES 32

enum {
    XSI_SEC_SYSTEM,
    XSI_SEC_CPU,
    XSI_SEC_MEMORY,
    XSI_SEC_PLATFORM,
    XSI_SEC_DISPLAY,
    XSI_SEC_BENCHMARK,
    XSI_SEC_COUNT
};

enum { XSI_LIST_LIBRARIES, XSI_LIST_DEVICES, XSI_LIST_RESOURCES, XSI_LIST_COUNT };

typedef enum {
    XSI_PLATFORM_UNKNOWN,
    XSI_PLATFORM_APPLE_NATIVE,
    XSI_PLATFORM_HOSTED,
    XSI_PLATFORM_RASPI,
    XSI_PLATFORM_OTHER_NATIVE
} xsi_platform;

typedef struct {
    xsi_platform platform;
    const char *platform_name;  /* chosen provider, "common" when none */
    unsigned pointer_bits;      /* of this build */
    xsi_section sections[XSI_SEC_COUNT];
    xsi_module_list lists[XSI_LIST_COUNT];
    xsi_memregion regions[XSI_MAX_REGIONS];
    unsigned region_count, regions_dropped;
    xsi_volume volumes[XSI_MAX_VOLUMES];
    unsigned volume_count, volumes_dropped;
} xsi_model;

void xsi_model_init(xsi_model *m);

/* Adds a field; returns it (for further edits) or NULL when the section is
 * full, in which case dropped is counted. */
/* InfoData32 declares its block counts LONG, but handlers store unsigned
 * counts there (FAT: a ULONG sector count). Widen them as unsigned, or a
 * volume past 2^31 blocks reads as 16 EiB. */
static inline uint64_t xsi_block_count(int32_t declared)
{
    return (uint64_t)(uint32_t)declared;
}

xsi_field *xsi_set(xsi_model *m, int section, const char *key, const char *label,
                   const char *source, const char *format, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 6, 7)))
#endif
    ;
xsi_field *xsi_missing(xsi_model *m, int section, const char *key, const char *label,
                       xsi_state state, const char *source);
const xsi_field *xsi_find(const xsi_model *m, const char *key);

xsi_module *xsi_add_module(xsi_model *m, int list);
xsi_memregion *xsi_add_region(xsi_model *m);
xsi_volume *xsi_add_volume(xsi_model *m);
void xsi_sort_modules(xsi_module_list *list);

/* Formatting shared by GUI and report; all return buf. */
char *xsi_format_size(uint64_t bytes, char *buf, size_t size);
char *xsi_format_address(uint64_t address, unsigned pointer_bits, char *buf, size_t size);
const char *xsi_state_name(xsi_state state);
const char *xsi_volume_state_name(xsi_volume_state state);
/* The displayed text of a field: its value, or the state in brackets. */
const char *xsi_field_text(const xsi_field *f);

#endif
