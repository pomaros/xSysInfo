/* SPDX-License-Identifier: MIT
 * Copyright (C) 2026 John KNIPPER. */
#include "sysinfo.h"
#include "collect.h"
#include <exec/memory.h>
#include <exec/tasks.h>
#include <dos/dostags.h>
#include <devices/timer.h>
#include <resources/processor.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/kernel.h>
#include <proto/processor.h>
#include <proto/timer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

APTR KernelBase;
extern APTR ProcessorBase;
struct Device *TimerBase;
static struct MsgPort *timer_port;
static struct timerequest *timer_request;
static uint64_t frequency;
static xsi_model *inventory;
typedef struct { ULONG tid; } AAChild;

int aa_platform_init(AAInfo *info)
{
    ULONG present = 0;
    unsigned i;
    struct EClockVal clock;
    struct TagItem tags[] = {{GCIT_NumberOfProcessors,(IPTR)&present},{TAG_DONE,0}};
    memset(info,0,sizeof(*info));
    inventory = calloc(1,sizeof(*inventory));
    if (!inventory) return 0;
    xsi_model_init(inventory);
    xsi_collect(inventory);
    info->inventory = inventory;
    KernelBase = OpenResource("kernel.resource");
    ProcessorBase = OpenResource(PROCESSORNAME);
    if (ProcessorBase) GetCPUInfo(tags);
    info->present = present;
    info->online = KernelBase ? KrnGetCPUCount() : 1;
    if (!info->online || info->online > AA_MAX_CPUS || present > AA_MAX_CPUS) return 0;
#ifdef __AROSEXEC_SMP__
    info->affinity = KernelBase != NULL;
#endif
    {
        const xsi_field *board = xsi_find(inventory,"board.model");
        snprintf(info->platform,sizeof(info->platform),"%s",board && board->state == XSI_OK ? board->value : inventory->platform_name);
    }
    snprintf(info->os,sizeof(info->os),"Exec %u.%u",SysBase->LibNode.lib_Version,SysBase->LibNode.lib_Revision);
    info->memory_total = AvailMem(MEMF_ANY | MEMF_TOTAL);
    info->memory_free = AvailMem(MEMF_ANY);
    for (i = 0; i < info->online; ++i) {
        CONST_STRPTR model = NULL;
        UQUAD hz = 0;
        ULONG cluster = 0, l1d = 0, l1i = 0, l2 = 0;
        struct TagItem core[] = {
            {GCIT_SelectedProcessor,i}, {GCIT_ModelString,(IPTR)&model},
            {GCIT_ProcessorSpeed,(IPTR)&hz}, {GCIT_ClusterID,(IPTR)&cluster},
            {GCIT_L1DataCacheSize,(IPTR)&l1d}, {GCIT_L1InstructionCacheSize,(IPTR)&l1i},
            {GCIT_L2CacheSize,(IPTR)&l2}, {TAG_DONE,0}
        };
        if (ProcessorBase) GetCPUInfo(core);
        snprintf(info->cpus[i].name,sizeof(info->cpus[i].name),"%s",model ? (const char *)model : "Unknown CPU");
        info->cpus[i].hz = hz; info->cpus[i].cluster = cluster;
        info->cpus[i].l1d = l1d; info->cpus[i].l1i = l1i; info->cpus[i].l2 = l2;
    }
    timer_port = CreateMsgPort();
    if (!timer_port) return 0;
    timer_request = (struct timerequest *)CreateIORequest(timer_port,sizeof(*timer_request));
    if (!timer_request || OpenDevice("timer.device",UNIT_MICROHZ,(struct IORequest *)timer_request,0)) return 0;
    TimerBase = timer_request->tr_node.io_Device;
    frequency = ReadEClock(&clock);
    return frequency != 0;
}

void aa_platform_close(void)
{
    if (TimerBase) CloseDevice((struct IORequest *)timer_request);
    if (timer_request) DeleteIORequest((struct IORequest *)timer_request);
    if (timer_port) DeleteMsgPort(timer_port);
    TimerBase = NULL; timer_request = NULL; timer_port = NULL;
    free(inventory); inventory = NULL;
}

void aa_inventory_result(AAInfo *info, const AARun *run)
{
    if (!info->inventory || run->phase != 3) return;
    xsi_model *model = info->inventory;
    xsi_section *section = &model->sections[XSI_SEC_BENCHMARK];
    unsigned i;
    for(i=0;i<section->count;++i) if(!strcmp(section->fields[i].key,"bench.cpu")) {
        xsi_field *field=&section->fields[i];
        field->label="Dhrystone 2.1"; field->source="xSysInfo measured"; field->state=XSI_OK;
        snprintf(field->value,sizeof(field->value),"%.2f D/s, %u worker(s)",run->rate,run->count);
        return;
    }
}

uint64_t aa_now(void)
{
    struct EClockVal clock;
    uint64_t ticks;
    ReadEClock(&clock);
    ticks = ((uint64_t)clock.ev_hi << 32) | clock.ev_lo;
    return (ticks / frequency) * 1000000000 + (ticks % frequency) * 1000000000 / frequency;
}
void aa_sleep(void) { Delay(1); }
unsigned aa_cpu(void) { return KernelBase ? KrnGetCPUNumber() : 0; }

static LONG entry(void)
{
    struct Task *self = FindTask(NULL);
    AAWorker *w = self->tc_UserData;
    AAChild *child = w->handle;
    __atomic_store_n(&child->tid,self->tc_UnionETask.tc_ETask->et_UniqueID,__ATOMIC_RELEASE);
    aa_worker(w);
    return 0;
}

int aa_spawn(AAWorker *w)
{
    struct Process *process;
    APTR affinity = (APTR)TASKAFFINITY_ANY;
    AAChild *child;
    if (!(FindTask(NULL)->tc_Flags & TF_ETASK)) return 0;
    child = calloc(1,sizeof(*child));
    if (!child) return 0;
#ifdef __AROSEXEC_SMP__
    if (!KernelBase || !(affinity = KrnAllocCPUMask())) { free(child); return 0; }
    KrnClearCPUMask(affinity);
    KrnGetCPUMask(w->cpu,affinity);
#else
    if (w->cpu != 0) { free(child); return 0; }
#endif
    w->handle = child;
    process = CreateNewProcTags(NP_Entry,(IPTR)entry, NP_Name,(IPTR)"xSysInfo worker",
        NP_StackSize,65536, NP_UserData,(IPTR)w, NP_Affinity,(IPTR)affinity,
        NP_NotifyOnDeath,TRUE, TAG_DONE);
    if (!process) {
#ifdef __AROSEXEC_SMP__
        KrnFreeCPUMask(affinity);
#endif
        free(child); w->handle = NULL; return 0;
    }
    return 1;
}

int aa_join(AAWorker *w)
{
    AAChild *child = w->handle;
    ULONG tid = __atomic_load_n(&child->tid,__ATOMIC_ACQUIRE);
    struct ETask *exited = (struct ETask *)ChildWait(tid);
    /* Do not unload code or free shared memory while a child can reference it. */
    if (!tid || (IPTR)exited <= CHILD_ACTIVE) {
        PutStr("xSysInfo: invalid child exit notification; retaining worker memory\n");
        for (;;) Wait(0);
    }
    ChildFree(exited->et_UniqueID);
    free(child); w->handle = NULL;
    return 1;
}
