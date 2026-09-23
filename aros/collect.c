// SPDX-License-Identifier: BSD-2-Clause
// SPDX-FileCopyrightText: 2026 John Knipper

/*
 * xSysInfo for AROS: the common collectors. Everything here reads OS
 * interfaces that every AROS build provides, or checks for an optional one
 * first. Lists are copied under Forbid() and formatted afterwards.
 * Ideas from src/software.c, src/memory.c and src/drives.c; the code is new
 * because those read Amiga hardware and 32-bit ROM addresses.
 */

#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/memory.h>
#include <exec/libraries.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <aros/inquire.h>
#include <aros/kernel.h>
#include <resources/processor.h>
#include <utility/tagitem.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/aros.h>
#include <proto/kernel.h>
#include <proto/processor.h>

#include <stdio.h>
#include <string.h>

#include "collect.h"

void *xsi_KernelBase;
#define KernelBase xsi_KernelBase
struct Library *ArosBase;
APTR ProcessorBase;

intptr_t xsi_kernel_attr(uint32_t id)
{
    if (!xsi_KernelBase)
        return -1;
    return KrnGetSystemAttr(id);
}

int xsi_has_resource(const char *name)
{
    return OpenResource((CONST_STRPTR)name) != NULL;
}

static void copy_name(char *out, size_t size, const char *name)
{
    size_t n = 0;
    if (name)
        for (; name[n] && n + 1 < size; n++)
            out[n] = name[n];
    out[n] = '\0';
}

/* ---- system ---------------------------------------------------------- */

static void collect_system(xsi_model *m, const char *architecture)
{
    const int S = XSI_SEC_SYSTEM;
    struct ExecBase *exec = SysBase;

    xsi_set(m, S, "os.exec", "exec.library", "exec", "%u.%u",
            (unsigned)exec->LibNode.lib_Version, (unsigned)exec->LibNode.lib_Revision);
    if (DOSBase)
        xsi_set(m, S, "os.dos", "dos.library", "exec", "%u.%u",
                (unsigned)DOSBase->dl_lib.lib_Version, (unsigned)DOSBase->dl_lib.lib_Revision);

    ArosBase = OpenLibrary((CONST_STRPTR)"aros.library", 0);
    if (ArosBase) {
        /* ArosInquire stores an IPTR for every AI_Aros* tag, whatever
         * aros/inquire.h says about ULONG: on a 64-bit AROS a ULONG
         * destination would be overwritten by eight bytes. */
        IPTR version = 0, major = 0, minor = 0;
        IPTR build = 0, variant = 0, arch = 0;
        struct TagItem tags[] = {
            { AI_ArosVersion, (IPTR)&version },
            { AI_ArosReleaseMajor, (IPTR)&major },
            { AI_ArosReleaseMinor, (IPTR)&minor },
            { AI_ArosBuildDate, (IPTR)&build },
            { AI_ArosVariant, (IPTR)&variant },
            { AI_ArosArchitecture, (IPTR)&arch },
            { TAG_DONE, 0 }
        };
        ArosInquireA(tags);
        if (version)
            xsi_set(m, S, "os.aros", "AROS", "aros.library", "%lu (release %lu.%lu)",
                    (unsigned long)version, (unsigned long)major, (unsigned long)minor);
        else
            xsi_missing(m, S, "os.aros", "AROS", XSI_UNKNOWN, "aros.library");
        if (build && ((const char *)build)[0])
            xsi_set(m, S, "os.build", "Build date", "aros.library", "%s", (const char *)build);
        else
            xsi_missing(m, S, "os.build", "Build date", XSI_UNKNOWN, "aros.library");
        if (variant && ((const char *)variant)[0])
            xsi_set(m, S, "os.variant", "Variant", "aros.library", "%s", (const char *)variant);
        if (!architecture && arch && ((const char *)arch)[0])
            architecture = (const char *)arch;
        CloseLibrary(ArosBase);
        ArosBase = NULL;
    } else {
        xsi_missing(m, S, "os.aros", "AROS", XSI_UNAVAILABLE, "aros.library");
    }

    if (architecture)
        xsi_set(m, S, "os.arch", "Architecture", "kernel.resource", "%s", architecture);
    else
        xsi_missing(m, S, "os.arch", "Architecture", XSI_UNKNOWN, "kernel.resource");
    xsi_set(m, S, "build.pointer", "Address width", "this build", "%u-bit", m->pointer_bits);
}

/* ---- processor ------------------------------------------------------- */

static const char *arch_name(ULONG a)
{
    switch (a) {
    case PROCESSORARCH_M68K: return "m68k";
    case PROCESSORARCH_PPC: return "PowerPC";
    case PROCESSORARCH_X86: return "x86";
    case PROCESSORARCH_ARM: return "ARM";
    default: return NULL;
    }
}

/* ULONG is 32 bits on every AROS, 64-bit ones included: an unanswered
 * tag leaves this marker in place. */
#define NOT_ANSWERED ((ULONG)0xffffffffU)

static void collect_cpu(xsi_model *m)
{
    const int C = XSI_SEC_CPU;
    ULONG count = NOT_ANSWERED, family = NOT_ANSWERED, architecture = NOT_ANSWERED,
          vendor = NOT_ANSWERED;
    CONST_STRPTR model = NULL;
    UQUAD speed = 0;

    ProcessorBase = OpenResource((CONST_STRPTR)PROCESSORNAME);
    if (!ProcessorBase) {
        xsi_missing(m, C, "cpu.count", "Processors", XSI_UNAVAILABLE, "processor.resource");
        xsi_missing(m, C, "cpu.model", "Model", XSI_UNAVAILABLE, "processor.resource");
        xsi_missing(m, C, "cpu.speed", "Clock", XSI_UNAVAILABLE, "processor.resource");
        return;
    }
    {
        struct TagItem tags[] = {
            { GCIT_NumberOfProcessors, (IPTR)&count },
            { GCIT_ModelString, (IPTR)&model },
            { GCIT_Family, (IPTR)&family },
            { GCIT_Architecture, (IPTR)&architecture },
            { GCIT_Vendor, (IPTR)&vendor },
            { GCIT_ProcessorSpeed, (IPTR)&speed },
            { TAG_DONE, 0 }
        };
        GetCPUInfo(tags);
    }
    if (count != NOT_ANSWERED && count != 0)
        xsi_set(m, C, "cpu.count", "Processors", "processor.resource", "%lu", (unsigned long)count);
    else
        xsi_missing(m, C, "cpu.count", "Processors", XSI_UNKNOWN, "processor.resource");
    /* processor.resource answers "Unknown" when it could not identify the
     * core; that is its unknown, not a model name. */
    if (model && model[0] && strcmp((const char *)model, "Unknown") != 0)
        xsi_set(m, C, "cpu.model", "Model", "processor.resource", "%s", (const char *)model);
    else
        xsi_missing(m, C, "cpu.model", "Model", XSI_UNKNOWN, "processor.resource");
    if (architecture != NOT_ANSWERED && arch_name(architecture))
        xsi_set(m, C, "cpu.arch", "Architecture", "processor.resource", "%s", arch_name(architecture));
    if (vendor != NOT_ANSWERED)
        xsi_set(m, C, "cpu.vendor", "Implementer", "processor.resource", "0x%02lx",
                (unsigned long)vendor);
    if (speed)
        xsi_set(m, C, "cpu.speed", "Clock", "processor.resource", "%lu MHz",
                (unsigned long)(speed / 1000000));
    else
        xsi_missing(m, C, "cpu.speed", "Clock", XSI_UNKNOWN, "processor.resource");
}

/* ---- memory ---------------------------------------------------------- */

static void collect_memory(xsi_model *m)
{
    const int M = XSI_SEC_MEMORY;
    struct MemHeader *mh;
    char a[24], b[24];
    IPTR total = AvailMem(MEMF_ANY | MEMF_TOTAL);
    IPTR avail = AvailMem(MEMF_ANY);
    IPTR largest = AvailMem(MEMF_ANY | MEMF_LARGEST);
    IPTR chip = AvailMem(MEMF_CHIP | MEMF_TOTAL);
    IPTR fast = AvailMem(MEMF_FAST | MEMF_TOTAL);

    xsi_set(m, M, "mem.total", "Total", "exec AvailMem", "%s",
            xsi_format_size((uint64_t)total, a, sizeof a));
    xsi_set(m, M, "mem.free", "Free", "exec AvailMem", "%s (largest block %s)",
            xsi_format_size((uint64_t)avail, a, sizeof a),
            xsi_format_size((uint64_t)largest, b, sizeof b));
    xsi_set(m, M, "mem.chip", "MEMF_CHIP total", "exec AvailMem", "%s",
            xsi_format_size((uint64_t)chip, a, sizeof a));
    xsi_set(m, M, "mem.fast", "MEMF_FAST total", "exec AvailMem", "%s",
            xsi_format_size((uint64_t)fast, a, sizeof a));

    /* Headers are copied, not walked: AROS may manage a header with its
     * own allocator, and SMP kernels guard free lists with spinlocks this
     * program does not take. mh_Free is a single word and safe to read. */
    Forbid();
    for (mh = (struct MemHeader *)SysBase->MemList.lh_Head;
         mh->mh_Node.ln_Succ; mh = (struct MemHeader *)mh->mh_Node.ln_Succ) {
        xsi_memregion *r = xsi_add_region(m);
        if (!r)
            continue;
        copy_name(r->name, sizeof r->name, mh->mh_Node.ln_Name);
        r->lower = (uint64_t)(IPTR)mh->mh_Lower;
        r->upper = (uint64_t)(IPTR)mh->mh_Upper;
        r->total = r->upper > r->lower ? r->upper - r->lower : 0;
        r->free = (uint64_t)mh->mh_Free;
        r->attributes = mh->mh_Attributes;
        r->priority = mh->mh_Node.ln_Pri;
        r->freelist_known = 0;
    }
    Permit();
    xsi_set(m, M, "mem.regions", "Regions", "exec MemList", "%u", m->region_count + m->regions_dropped);
}

/* ---- libraries, devices, resources ----------------------------------- */

static void collect_list(xsi_model *m, int which, struct List *list)
{
    struct Node *n;
    Forbid();
    for (n = list->lh_Head; n->ln_Succ; n = n->ln_Succ) {
        xsi_module *x = xsi_add_module(m, which);
        if (!x)
            continue;
        copy_name(x->name, sizeof x->name, n->ln_Name);
        x->address = (uint64_t)(IPTR)n;
        x->priority = n->ln_Pri;
        /* Libraries and devices are struct Library. A resource may be any
         * structure behind its Node, so its "version" would be whatever
         * bytes follow; resources are listed without one. */
        if (which != XSI_LIST_RESOURCES) {
            x->version = ((struct Library *)n)->lib_Version;
            x->revision = ((struct Library *)n)->lib_Revision;
            x->has_version = 1;
        }
    }
    Permit();
    xsi_sort_modules(&m->lists[which]);
}

/* ---- volumes --------------------------------------------------------- */

static void bstr_copy(char *out, size_t size, BSTR b)
{
    const UBYTE *s = AROS_BSTR_ADDR(b);
    size_t n = s ? AROS_BSTR_strlen(b) : 0, i;
    if (n >= size)
        n = size - 1;
    for (i = 0; i < n; i++)
        out[i] = (char)s[i];
    out[n] = '\0';
}

static void collect_volumes(xsi_model *m)
{
    struct DosList *dl;
    struct Process *me = (struct Process *)FindTask(NULL);
    APTR window = me->pr_WindowPtr;
    unsigned i;
    struct InfoData *info = AllocMem(sizeof *info, MEMF_ANY | MEMF_CLEAR);

    /* Names first, under the DOS list lock; no I/O while it is held. Only
     * devices whose handler already runs: asking a dormant one would start
     * it, which is not this program's decision. */
    dl = LockDosList(LDF_DEVICES | LDF_READ);
    while ((dl = NextDosEntry(dl, LDF_DEVICES)) != NULL) {
        xsi_volume *v;
        if (!dl->dol_Task)
            continue;
        v = xsi_add_volume(m);
        if (!v)
            continue;
        bstr_copy(v->device, sizeof v->device - 1, dl->dol_Name);
        strcat(v->device, ":");
        if (dl->dol_misc.dol_handler.dol_Handler)
            bstr_copy(v->handler, sizeof v->handler, dl->dol_misc.dol_handler.dol_Handler);
    }
    UnLockDosList(LDF_DEVICES | LDF_READ);

    me->pr_WindowPtr = (APTR)-1;    /* no "insert volume" requesters */
    for (i = 0; i < m->volume_count && info; i++) {
        xsi_volume *v = &m->volumes[i];
        BPTR lock = Lock((CONST_STRPTR)v->device, SHARED_LOCK);
        if (!lock) {
            v->state = IoErr() == ERROR_NO_DISK ? XSI_VOL_NO_DISK : XSI_VOL_UNKNOWN;
            continue;
        }
        if (Info(lock, info)) {
            v->has_info = 1;
            v->block_size = (uint32_t)info->id_BytesPerBlock;
            v->total_blocks = (uint64_t)info->id_NumBlocks;
            v->used_blocks = (uint64_t)info->id_NumBlocksUsed;
            v->dos_type = (uint32_t)info->id_DiskType;
            v->state = info->id_DiskState == ID_WRITE_PROTECTED ? XSI_VOL_READ_ONLY
                     : info->id_DiskState == ID_VALIDATING ? XSI_VOL_BUSY : XSI_VOL_OK;
            if (info->id_VolumeNode) {
                struct DosList *vol = (struct DosList *)BADDR(info->id_VolumeNode);
                bstr_copy(v->volume, sizeof v->volume, vol->dol_Name);
            }
        }
        UnLock(lock);
    }
    me->pr_WindowPtr = window;
    if (info)
        FreeMem(info, sizeof *info);
}

/* ---- display and benchmarks ------------------------------------------ */

static void collect_display(xsi_model *m)
{
    const int D = XSI_SEC_DISPLAY;
#if defined(KATTR_FrameBufferWidth) && defined(KATTR_FrameBufferHeight)
    intptr_t w = xsi_kernel_attr(KATTR_FrameBufferWidth);
    intptr_t h = xsi_kernel_attr(KATTR_FrameBufferHeight);
#ifdef KATTR_FrameBufferDepth
    intptr_t d = xsi_kernel_attr(KATTR_FrameBufferDepth);
#else
    intptr_t d = -1;
#endif
    if (w > 0 && h > 0) {
        if (d > 0)
            xsi_set(m, D, "fb.mode", "Boot framebuffer", "kernel.resource", "%ldx%ld, %ld bpp",
                    (long)w, (long)h, (long)d);
        else
            xsi_set(m, D, "fb.mode", "Boot framebuffer", "kernel.resource", "%ldx%ld",
                    (long)w, (long)h);
        return;
    }
#endif
    xsi_missing(m, D, "fb.mode", "Boot framebuffer", XSI_UNAVAILABLE, "kernel.resource");
}

static void collect_benchmarks(xsi_model *m)
{
    /* Upstream's scores are calibrated against m68k machines; nothing is
     * reported until AArch64 timers, units and cancellation are checked. */
    xsi_missing(m, XSI_SEC_BENCHMARK, "bench.cpu", "CPU", XSI_UNAVAILABLE, "not in this build");
    xsi_missing(m, XSI_SEC_BENCHMARK, "bench.memory", "Memory", XSI_UNAVAILABLE, "not in this build");
}

void xsi_collect(xsi_model *m)
{
    const char *architecture = NULL;
    intptr_t a;

    xsi_KernelBase = OpenResource((CONST_STRPTR)"kernel.resource");
    a = xsi_kernel_attr(KATTR_Architecture);
    if (a != -1 && a != 0)
        architecture = (const char *)a;

    collect_system(m, architecture);
    collect_cpu(m);
    collect_memory(m);
    collect_list(m, XSI_LIST_LIBRARIES, &SysBase->LibList);
    collect_list(m, XSI_LIST_DEVICES, &SysBase->DeviceList);
    collect_list(m, XSI_LIST_RESOURCES, &SysBase->ResourceList);
    collect_volumes(m);
    collect_display(m);
    collect_benchmarks(m);

    if (!xsi_provider_apple(m, architecture) && !xsi_provider_hosted(m, architecture)
        && !xsi_provider_raspi(m, architecture)) {
        m->platform = XSI_PLATFORM_UNKNOWN;
        m->platform_name = "common";
        xsi_missing(m, XSI_SEC_PLATFORM, "platform.provider", "Provider", XSI_UNAVAILABLE,
                    "no provider recognised this system");
    }
}
