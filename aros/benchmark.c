/* SPDX-License-Identifier: MIT
 * Copyright (C) 2026 John KNIPPER. */
#include "sysinfo.h"
#include "dhry.h"
#include <stdio.h>

/* Fixed xSysInfo historical data, not re-calibrated ARM measurements.
 * Source: benchmark.c, revision 0ee81383380251df0fc67f78c0282530a7dcff84.
 * Upstream labels these as placeholders to calibrate. */
const AAReference aa_references[AA_REFERENCE_COUNT] = {
    {"A600  68000  7MHz",1001}, {"B2000 68000  7MHz",1408},
    {"A1200 EC020 14MHz",2550}, {"A3000 68030 25MHz",8300},
    {"A4000 68040 25MHz",32809}, {"A4000 68060 50MHz",91000},
    {"M1 Firestorm 1 core",18242659}, {"M1 8 cores SMP",127077293}
};

static int verified(const DhryState *s)
{
    return s->integer == 5 && s->boolean == 1 && s->ch1 == 'A' && s->ch2 == 'B'
        && s->arr1[8] == 7 && s->arr2[8][7] == AA_BATCH + 10
        && s->ptr->Ptr_Comp == s->next && s->next->Ptr_Comp == s->next
        && s->ptr->Discr == Ident_1 && s->next->Discr == Ident_1
        && s->ptr->variant.var_1.Enum_Comp == Ident_3
        && s->next->variant.var_1.Enum_Comp == Ident_2
        && s->ptr->variant.var_1.Int_Comp == 17
        && s->next->variant.var_1.Int_Comp == 18
        && !strcmp(s->ptr->variant.var_1.Str_Comp,"DHRYSTONE PROGRAM, SOME STRING")
        && !strcmp(s->next->variant.var_1.Str_Comp,"DHRYSTONE PROGRAM, SOME STRING")
        && s->local1 == 5 && s->local2 == 13 && s->local3 == 7
        && s->enumeration == Ident_2;
}

void aa_worker(AAWorker *w)
{
    DhryState state = {0};
    AARun *r = w->run;
    Dhry_Initialize(&state);
    Dhry_Run(&state, AA_BATCH); /* untimed warmup + integrity */
    w->valid = verified(&state);
    w->first_cpu = aa_cpu();
    __atomic_store_n(&w->ready, 1, __ATOMIC_RELEASE);
    while (!__atomic_load_n(&r->release, __ATOMIC_ACQUIRE)) aa_sleep();
    if (!__atomic_load_n(&r->cancel, __ATOMIC_ACQUIRE) && w->valid) {
        while (aa_now() < r->start && !__atomic_load_n(&r->cancel, __ATOMIC_RELAXED)) {}
        while (aa_now() < r->deadline && !__atomic_load_n(&r->cancel, __ATOMIC_RELAXED)) {
            Dhry_Run(&state, AA_BATCH);
            w->iterations += AA_BATCH;
        }
        w->ended = aa_now();
        w->valid = verified(&state) && w->iterations != 0;
    }
    w->last_cpu = aa_cpu();
    w->valid = w->valid && w->first_cpu == w->cpu && w->last_cpu == w->cpu;
    __atomic_store_n(&w->done, 1, __ATOMIC_RELEASE);
}

static int launch(AARun *r)
{
    unsigned i;
    r->release = 0;
    for (i = 0; i < r->count; ++i) {
        unsigned cpu = r->workers[i].cpu;
        memset(&r->workers[i], 0, sizeof(r->workers[i]));
        r->workers[i].cpu = cpu;
        r->workers[i].run = r;
        if (!aa_spawn(&r->workers[i])) {
            snprintf(r->error, sizeof(r->error), "Cannot start worker on CPU %u", cpu);
            r->count = i;
            aa_cancel(r);
            return 0;
        }
    }
    r->phase = 1;
    return 1;
}

int aa_begin(AARun *r, const AAInfo *info, const unsigned *cpus, unsigned n, unsigned ms)
{
    unsigned i,j;
    memset(r, 0, sizeof(*r));
    if (!n || n > AA_MAX_CPUS || n > info->online || ms < 100 || ms > 10000) {
        strcpy(r->error,"Invalid worker count or duration"); return 0;
    }
    if (n > 1 && !info->affinity) {
        strcpy(r->error,"SMP affinity unavailable"); return 0;
    }
    for (i = 0; i < n; ++i) {
        if (cpus[i] >= info->online) { strcpy(r->error,"CPU is not online"); return 0; }
        for (j = 0; j < i; ++j) if (cpus[i] == cpus[j]) {
            strcpy(r->error,"Duplicate CPU"); return 0;
        }
        r->workers[i].cpu = cpus[i];
    }
    r->count = n; r->duration = (uint64_t)ms * 1000000;
    return launch(r);
}

void aa_cancel(AARun *r)
{
    __atomic_store_n(&r->cancel, 1, __ATOMIC_RELEASE);
    __atomic_store_n(&r->release, 1, __ATOMIC_RELEASE);
    r->phase = 2;
}

int aa_poll(AARun *r)
{
    unsigned i, median;
    uint64_t end, total = 0;
    if (r->phase == 3) return 1;
    if (!r->phase) return -1;
    if (r->phase == 1) {
        for (i = 0; i < r->count; ++i)
            if (!__atomic_load_n(&r->workers[i].ready, __ATOMIC_ACQUIRE)) return 0;
        r->start = aa_now() + 50000000; /* 50 ms rendezvous */
        r->deadline = r->start + r->duration;
        __atomic_store_n(&r->release, 1, __ATOMIC_RELEASE);
        r->phase = 2;
    }
    for (i = 0; i < r->count; ++i)
        if (!__atomic_load_n(&r->workers[i].done, __ATOMIC_ACQUIRE)) return 0;
    end = r->deadline;
    for (i = 0; i < r->count; ++i) {
        if (!aa_join(&r->workers[i]) || !r->workers[i].valid)
            strcpy(r->error,"Worker exit, Dhrystone integrity or affinity failed");
        if (r->workers[i].ended > end) end = r->workers[i].ended;
        total += r->workers[i].iterations;
    }
    if (r->cancel || r->error[0] || end <= r->start) {
        if (!r->error[0]) strcpy(r->error,"Cancelled");
        r->phase = 0; return -1;
    }
    /* All workers share one denominator, including final batch overrun. */
    r->rates[r->trial] = (double)total * 1e9 / (double)(end - r->start);
    for (i = 0; i < r->count; ++i)
        r->worker_rates[r->trial][i] = (double)r->workers[i].iterations * 1e9 / (double)(end - r->start);
    if (++r->trial < AA_TRIALS) { launch(r); return 0; }
    median = (r->rates[0] > r->rates[1]) ?
        ((r->rates[1] > r->rates[2]) ? 1 : (r->rates[0] > r->rates[2] ? 2 : 0)) :
        ((r->rates[0] > r->rates[2]) ? 0 : (r->rates[1] > r->rates[2] ? 2 : 1));
    r->rate = r->rates[median];
    memcpy(r->per_cpu, r->worker_rates[median], sizeof(r->per_cpu));
    r->phase = 3; return 1;
}

void aa_report(const AAInfo *info, const AARun *r, double single)
{
    unsigned i;
    printf("xSysInfo | %s | %s\n", info->platform,info->os);
    printf("CPUs: %u present, %u online; RAM: %llu MiB, %llu MiB free\n",info->present,info->online,
        (unsigned long long)(info->memory_total >> 20),(unsigned long long)(info->memory_free >> 20));
    printf("Dhrystone 2.1, explicit worker state; -O2 -fno-builtin -fno-lto -fno-inline\nCompiler: %s\n",__VERSION__);
    printf("Mode: %s; workers: %u; trials: 3; median aggregate; shared window: %llu ms\n",
        r->count > 1 ? "SMP cumulative throughput" : "SINGLE",r->count,(unsigned long long)(r->duration/1000000));
    if (r->phase != 3) { printf("RESULT FAIL: %s\n",r->error); return; }
    printf("Dhrystones/s: %.2f; DMIPS: %.2f\n",r->rate,r->rate/1757.0);
    if (single > 0 && r->count > 1) printf("Speedup: %.2fx (CPU %u single baseline: %.2f Dhrystones/s)\n",r->rate/single,r->workers[0].cpu,single);
    for (i = 0; i < r->count; ++i) printf("CPU %u %s: %.2f Dhrystones/s\n",r->workers[i].cpu,
        info->cpus[r->workers[i].cpu].name,r->per_cpu[i]);
    for (i = 0; i < AA_REFERENCE_COUNT; ++i) printf("%s: %llu reference Dhrystones/s; %.2fx\n",aa_references[i].name,
        (unsigned long long)aa_references[i].dhry,r->rate/aa_references[i].dhry);
    printf("Trial Dhrystones/s: %.2f, %.2f, %.2f\n",r->rates[0],r->rates[1],r->rates[2]);
    printf("RESULT PASS\n");
}
