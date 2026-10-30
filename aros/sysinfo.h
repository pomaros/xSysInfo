/* SPDX-License-Identifier: MIT
 * Copyright (C) 2026 John KNIPPER. */
#ifndef XSYSINFO_H
#define XSYSINFO_H
#include <stdint.h>
#include <stddef.h>
#define AA_MAX_CPUS 256
#define AA_TRIALS 3
#define AA_BATCH 20000
typedef struct { char name[96]; uint64_t hz; unsigned cluster, l1d, l1i, l2; } AACpu;
typedef struct {
    unsigned present, online;
    uint64_t memory_total, memory_free;
    char platform[96], os[96];
    AACpu cpus[AA_MAX_CPUS];
    int affinity;
    void *inventory;
} AAInfo;
typedef struct { const char *name; uint64_t dhry; } AAReference;
#define AA_REFERENCE_COUNT 8
extern const AAReference aa_references[AA_REFERENCE_COUNT];
typedef struct AAWorker {
    unsigned cpu, first_cpu, last_cpu, ready, done, valid;
    uint64_t iterations, ended;
    struct AARun *run;
    void *handle;
} AAWorker;
typedef struct AARun {
    AAWorker workers[AA_MAX_CPUS];
    unsigned count, trial, phase, cancel, release;
    uint64_t start, deadline, duration;
    double rates[AA_TRIALS], worker_rates[AA_TRIALS][AA_MAX_CPUS];
    double rate, per_cpu[AA_MAX_CPUS];
    char error[128];
} AARun;
int aa_platform_init(AAInfo *info);
void aa_platform_close(void);
void aa_inventory_result(AAInfo *info, const AARun *run);
uint64_t aa_now(void);
void aa_sleep(void);
unsigned aa_cpu(void);
int aa_spawn(AAWorker *worker);
int aa_join(AAWorker *worker);
void aa_worker(AAWorker *worker);
int aa_begin(AARun *run, const AAInfo *info, const unsigned *cpus, unsigned count, unsigned milliseconds);
/* 0 running, 1 passed, -1 failed/cancelled. Poll from the GUI event loop. */
int aa_poll(AARun *run);
void aa_cancel(AARun *run);
void aa_report(const AAInfo *info, const AARun *run, double single);
int aa_gui(AAInfo *info);
#endif
