/* SPDX-License-Identifier: MIT
 * Copyright (C) 2026 John KNIPPER.
 * HOST correctness adapter. Logical IDs are simulated, not CPU affinity evidence. */
#include "sysinfo.h"
#include "dhry.h"
#include <pthread.h>
#include <time.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static _Thread_local unsigned cpu;
static unsigned live, launched;
static int fail_after = -1, wrong_cpu;
uint64_t aa_now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return (uint64_t)t.tv_sec*1000000000+t.tv_nsec; }
void aa_sleep(void) { struct timespec t = {0,1000000}; nanosleep(&t,NULL); }
unsigned aa_cpu(void) { return cpu; }
static void *entry(void *arg) { AAWorker *w=arg; cpu=w->cpu+(unsigned)wrong_cpu; aa_worker(w); return NULL; }
int aa_spawn(AAWorker *w) {
    pthread_t *t;
    if(fail_after>=0 && launched++==(unsigned)fail_after) return 0;
    t=malloc(sizeof(*t)); assert(t); w->handle=t;
    if(pthread_create(t,NULL,entry,w)) { free(t); return 0; }
    live++; return 1;
}
int aa_join(AAWorker *w) { int e=pthread_join(*(pthread_t *)w->handle,NULL); free(w->handle); w->handle=NULL; live--; return e==0; }
static int finish(AARun *r) {
    int result; uint64_t limit=aa_now()+10000000000ULL;
    while((result=aa_poll(r))==0) { assert(aa_now()<limit); aa_sleep(); }
    assert(live==0); return result;
}
int main(void) {
    AAInfo info={0}; AARun *r=calloc(1,sizeof(*r)); unsigned ids[]={0,1,2,3};
    DhryState a={0}, b={0}; unsigned i;
    assert(r); info.online=info.present=4; info.affinity=1;
    Dhry_Initialize(&a); Dhry_Initialize(&b);
    Dhry_Run(&a,20000); Dhry_Run(&b,100);
    assert(a.arr2[8][7]==20010 && b.arr2[8][7]==110);
    for(i=0;i<3;i++) { Dhry_Run(&a,20000); assert(a.arr2[8][7]==20010); }
    assert(aa_begin(r,&info,ids,1,100)); assert(finish(r)==1); assert(r->rate>0);
    assert(aa_begin(r,&info,ids,4,100)); assert(finish(r)==1);
    { double sum=0; for(i=0;i<4;i++) sum+=r->per_cpu[i]; assert(sum/r->rate>0.999999 && sum/r->rate<1.000001); }
    for(i=0;i<4;i++) {
        fail_after=i; launched=0; assert(!aa_begin(r,&info,ids,4,100)); assert(finish(r)==-1);
    }
    fail_after=-1;
    assert(aa_begin(r,&info,ids,4,100)); aa_cancel(r); assert(finish(r)==-1);
    assert(aa_begin(r,&info,ids,4,100));
    while(r->phase==1) { assert(aa_poll(r)==0); aa_sleep(); }
    aa_cancel(r); assert(finish(r)==-1);
    wrong_cpu=1; assert(aa_begin(r,&info,ids,4,100)); assert(finish(r)==-1); wrong_cpu=0;
    ids[1]=0; assert(!aa_begin(r,&info,ids,2,100)); ids[1]=1;
    ids[0]=4; assert(!aa_begin(r,&info,ids,1,100)); ids[0]=0;
    info.affinity=0; assert(!aa_begin(r,&info,ids,2,100));
    assert(!aa_begin(r,&info,ids,0,100)); assert(!aa_begin(r,&info,ids,1,0));
    assert(aa_references[0].dhry==1001 && aa_references[5].dhry==91000);
    free(r); puts("HOST RESULT PASS: integrity, independent state, SINGLE/SMP, shared accounting, cancellation, partial creation, affinity rejection, input bounds");
    return 0;
}
