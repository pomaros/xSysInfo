// SPDX-License-Identifier: BSD-2-Clause
// SPDX-FileCopyrightText: 2026 John Knipper

/* xSysInfo for AROS: the text report. Portable C99. */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "report.h"

typedef struct {
    xsi_write_fn write;
    void *ctx;
} out_t;

static void line(out_t *o, const char *format, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 2, 3)))
#endif
    ;

static void line(out_t *o, const char *format, ...)
{
    char buf[256];
    va_list args;
    size_t n;
    va_start(args, format);
    vsnprintf(buf, sizeof buf - 1, format, args);
    va_end(args);
    n = strlen(buf);
    buf[n] = '\n';
    buf[n + 1] = '\0';
    o->write(o->ctx, buf);
}

static void heading(out_t *o, const char *title)
{
    line(o, "%s", "");
    line(o, "%s", title);
    line(o, "%.*s", (int)strlen(title), "----------------------------------------");
}

static void section(out_t *o, const xsi_section *s, int with_source)
{
    unsigned i;
    heading(o, s->title);
    if (s->count == 0)
        line(o, "  %s", "[nothing reported]");
    for (i = 0; i < s->count; i++) {
        const xsi_field *f = &s->fields[i];
        if (with_source)
            line(o, "  %-22s %-40s (%s)", f->label, xsi_field_text(f), f->source);
        else
            line(o, "  %-22s %s", f->label, xsi_field_text(f));
    }
    if (s->dropped)
        line(o, "  [%u more fields not shown]", s->dropped);
}

static void modules(out_t *o, const xsi_model *m, const xsi_module_list *l)
{
    unsigned i;
    char addr[32];
    heading(o, l->title);
    for (i = 0; i < l->count; i++) {
        const xsi_module *x = &l->items[i];
        if (x->has_version)
            line(o, "  %-32s %s  %u.%u", x->name,
                 xsi_format_address(x->address, m->pointer_bits, addr, sizeof addr),
                 (unsigned)x->version, (unsigned)x->revision);
        else
            line(o, "  %-32s %s  -", x->name,
                 xsi_format_address(x->address, m->pointer_bits, addr, sizeof addr));
    }
    if (l->dropped)
        line(o, "  [%u more not listed]", l->dropped);
}

static void memory(out_t *o, const xsi_model *m)
{
    unsigned i;
    char lo[32], hi[32], total[24], freeb[24], largest[24];
    heading(o, "Memory regions");
    if (m->region_count == 0)
        line(o, "  %s", "[none reported]");
    for (i = 0; i < m->region_count; i++) {
        const xsi_memregion *r = &m->regions[i];
        line(o, "  %s", r->name[0] ? r->name : "(unnamed)");
        line(o, "    range      %s - %s",
             xsi_format_address(r->lower, m->pointer_bits, lo, sizeof lo),
             xsi_format_address(r->upper, m->pointer_bits, hi, sizeof hi));
        line(o, "    size       %s, free %s, largest %s",
             xsi_format_size(r->total, total, sizeof total),
             xsi_format_size(r->free, freeb, sizeof freeb),
             r->largest_known ? xsi_format_size(r->largest, largest, sizeof largest)
                              : "[unknown]");
        line(o, "    attributes 0x%08lx, priority %d, %lu free chunks",
             (unsigned long)r->attributes, (int)r->priority, (unsigned long)r->chunks);
    }
    if (m->regions_dropped)
        line(o, "  [%u more regions not listed]", m->regions_dropped);
}

static void volumes(out_t *o, const xsi_model *m)
{
    unsigned i;
    char total[24], used[24];
    heading(o, "Volumes");
    if (m->volume_count == 0)
        line(o, "  %s", "[none mounted]");
    for (i = 0; i < m->volume_count; i++) {
        const xsi_volume *v = &m->volumes[i];
        if (v->has_info && v->block_size) {
            uint64_t t = v->total_blocks * (uint64_t)v->block_size;
            uint64_t u = v->used_blocks * (uint64_t)v->block_size;
            line(o, "  %-10s %-20s %-10s %s used of %s, %lu-byte blocks, %s",
                 v->device, v->volume[0] ? v->volume : "-", xsi_volume_state_name(v->state),
                 xsi_format_size(u, used, sizeof used), xsi_format_size(t, total, sizeof total),
                 (unsigned long)v->block_size, v->handler[0] ? v->handler : "-");
        } else {
            line(o, "  %-10s %-20s %-10s [size unknown], %s", v->device,
                 v->volume[0] ? v->volume : "-", xsi_volume_state_name(v->state),
                 v->handler[0] ? v->handler : "-");
        }
    }
    if (m->volumes_dropped)
        line(o, "  [%u more volumes not listed]", m->volumes_dropped);
}

void xsi_report(const xsi_model *m, xsi_report_mode mode, xsi_write_fn write, void *ctx)
{
    out_t o;
    int i;
    o.write = write;
    o.ctx = ctx;
    line(&o, "xSysInfo for AROS - %s report, provider %s, %u-bit build",
         mode == XSI_REPORT_FULL ? "full" : "brief", m->platform_name, m->pointer_bits);
    for (i = 0; i < XSI_SEC_COUNT; i++)
        section(&o, &m->sections[i], mode == XSI_REPORT_FULL);
    if (mode == XSI_REPORT_BRIEF) {
        heading(&o, "Software");
        for (i = 0; i < XSI_LIST_COUNT; i++)
            line(&o, "  %-22s %u", m->lists[i].title, m->lists[i].count + m->lists[i].dropped);
        line(&o, "  %-22s %u", "Memory regions", m->region_count + m->regions_dropped);
        line(&o, "  %-22s %u", "Volumes", m->volume_count + m->volumes_dropped);
        return;
    }
    for (i = 0; i < XSI_LIST_COUNT; i++)
        modules(&o, m, &m->lists[i]);
    memory(&o, m);
    volumes(&o, m);
}
