/* SPDX-License-Identifier: MIT
 * Copyright (C) 2026 John KNIPPER. */
#include "sysinfo.h"
#include "report.h"
#include <proto/dos.h>
#include <proto/exec.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int __nostdiowin = 1;
static void write_inventory(void *ctx,const char *s) { fputs(s,(FILE *)ctx); }

int main(int argc, char **argv)
{
    AAInfo *info = calloc(1,sizeof(*info));
    AARun *run = calloc(1,sizeof(*run));
    unsigned cpus[AA_MAX_CPUS], n = 1, cpu = 0, ms = 1000, i;
    int cli = 0, smp = 0, workers_set = 0, full = 0, result = 20, poll;
    double single = 0;
    if (!info || !run) goto out;
    if (!aa_platform_init(info)) { puts("xSysInfo: system information or timer unavailable"); goto out; }
    for (i = 1; i < (unsigned)argc; ++i) {
        if (!strcmp(argv[i],"CLI") || !strcmp(argv[i],"BRIEF")) cli = 1;
        else if (!strcmp(argv[i],"FULL")) full = cli = 1;
        else if (!strcmp(argv[i],"SMP")) smp = cli = 1;
        else if ((!strcmp(argv[i],"WORKERS") || !strcmp(argv[i],"CPU") || !strcmp(argv[i],"MS")) && i+1 < (unsigned)argc) {
            char *end; const char *key = argv[i++];
            unsigned long value = strtoul(argv[i],&end,10);
            if (!argv[i][0] || *end || value > 10000) goto usage;
            if (!strcmp(key,"WORKERS")) { n = value; smp = 1; workers_set = 1; }
            else if (!strcmp(key,"CPU")) cpu = value;
            else ms = value;
            cli = 1;
        } else goto usage;
    }
    if (!cli) { result = aa_gui(info); goto out; }
    if (smp && !workers_set) n = cpu < info->online ? info->online - cpu : 0;
    if (!n || cpu >= info->online || n > info->online - cpu) goto usage;
    for (i = 0; i < n; ++i) cpus[i] = cpu + i;
    if (!aa_begin(run,info,cpus,1,ms)) goto failed;
    while ((poll = aa_poll(run)) == 0) {
        if (SetSignal(0,SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C) aa_cancel(run);
        aa_sleep();
    }
    if (poll < 0) goto failed;
    single = run->rate;
    if (n > 1) {
        if (!aa_begin(run,info,cpus,n,ms)) goto failed;
        while ((poll = aa_poll(run)) == 0) {
            if (SetSignal(0,SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C) aa_cancel(run);
            aa_sleep();
        }
        if (poll < 0) goto failed;
    }
    aa_inventory_result(info,run);
    if (full) xsi_report(info->inventory,XSI_REPORT_FULL,write_inventory,stdout);
    aa_report(info,run,single); result = 0; goto out;
failed:
    /* A partial launch must also be drained before the run is freed. */
    while (run->phase == 2 && aa_poll(run) == 0) aa_sleep();
    aa_report(info,run,0); goto out;
usage:
    puts("xSysInfo [CLI] [SMP] [WORKERS n] [CPU first] [MS 100..10000]");
out:
    aa_platform_close(); free(run); free(info); return result;
}
