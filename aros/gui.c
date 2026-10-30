/* SPDX-License-Identifier: MIT
 * Copyright (C) 2026 John KNIPPER. */
#include "sysinfo.h"
#include "gui.h"
#include "report.h"
#include <intuition/intuition.h>
#include <graphics/text.h>
#include <graphics/gfxmacros.h>
#include <graphics/view.h>
#include <diskfont/diskfont.h>
#include <proto/diskfont.h>
#include <dos/dosextens.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/utility.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { BG, BLACK, WHITE, BLUE, RED, DARK, PEN_COUNT };
typedef struct {
    struct Window *window;
    struct Screen *screen;
    struct TextFont *font, *base_font;
    int scale;
    LONG pen[PEN_COUNT];
    AAInfo *info;
    AARun *run;
    unsigned selected[AA_MAX_CPUS], cpu_page;
    int smp, active, closing, pending_smp;
    double single;
    char status[128];
} View;

static int px(View *v,int x) { int inner=v->window->Width-v->window->BorderLeft-v->window->BorderRight; return v->window->BorderLeft+(inner-800*v->scale)/2+x*v->scale; }
static int py(View *v,int y) { int inner=v->window->Height-v->window->BorderTop-v->window->BorderBottom; return v->window->BorderTop+(inner-480*v->scale)/2+y*v->scale; }
static void set_scale(View *v,int scale)
{
    struct TextAttr attr = {(STRPTR)"topaz.font",(UWORD)(8*scale),0,0};
    struct DiskFontHeader *scaled = NULL;
    if(scale<1) scale=1;
    if(scale>4) scale=4;
    attr.ta_YSize=8*scale;
    if(v->scale==scale && v->font) return;
    if(scale>1) scaled=NewScaledDiskFont(v->base_font,&attr);
    if(v->font && v->font!=v->base_font) CloseFont(v->font);
    v->font=scaled?&scaled->dfh_TF:v->base_font;
    v->scale=scaled?scale:1;
    if(v->window) SetFont(v->window->RPort,v->font);
}
static void rect(View *v,int x,int y,int w,int h,int color)
{
    SetAPen(v->window->RPort,v->pen[color]);
    RectFill(v->window->RPort,px(v,x),py(v,y),px(v,x+w)-1,py(v,y+h)-1);
}
static void line(View *v,int x,int y,int xx,int yy,int color)
{
    SetAPen(v->window->RPort,v->pen[color]);
    Move(v->window->RPort,px(v,x),py(v,y)); Draw(v->window->RPort,px(v,xx),py(v,yy));
}
static void label(View *v,int x,int y,const char *s,int color)
{
    SetAPen(v->window->RPort,v->pen[color]); SetDrMd(v->window->RPort,JAM1);
    Move(v->window->RPort,px(v,x),py(v,y)); Text(v->window->RPort,s,strlen(s));
}
static void bevel(View *v,int x,int y,int w,int h,int sunken)
{
    line(v,x,y,x+w,y,sunken?DARK:WHITE); line(v,x,y,x,y+h,sunken?DARK:WHITE);
    line(v,x,y+h,x+w,y+h,sunken?WHITE:DARK); line(v,x+w,y,x+w,y+h,sunken?WHITE:DARK);
}
static void button(View *v,int x,int y,int w,const char *s,int selected)
{
    rect(v,x,y,w,30,BG); bevel(v,x,y,w,30,selected);
    label(v,x+8,y+20,s,BLACK);
}

static void draw(View *v)
{
    unsigned i, count = 0;
    char text[128];
    double rate = v->run->phase == 3 ? v->run->rate : 0;
    double maximum = rate > 127077293 ? rate : 127077293;
    SetAPen(v->window->RPort,v->pen[BG]);
    RectFill(v->window->RPort,v->window->BorderLeft,v->window->BorderTop,v->window->Width-v->window->BorderRight-1,v->window->Height-v->window->BorderBottom-1);
    bevel(v,8,8,784,225,1);
    rect(v,10,10,780,28,DARK);
    label(v,20,29,"xSysInfo - AArch64 SYSTEM INFORMATION",WHITE);
    {
        label(v,22,65,v->info->platform,BLACK);
        label(v,22,85,v->info->os,BLACK);
        snprintf(text,sizeof(text),"CPUs present: %u   online: %u",v->info->present,v->info->online);
        label(v,22,108,text,BLACK);
        snprintf(text,sizeof(text),"RAM: %llu MiB   Free: %llu MiB",(unsigned long long)(v->info->memory_total>>20),(unsigned long long)(v->info->memory_free>>20));
        label(v,22,131,text,BLACK);
        label(v,22,157,"Dhrystone 2.1 / 3 trials / median",BLACK);
        label(v,22,180,"Amiga + measured M1 reference scores",BLACK);
        label(v,22,204,"Select cores on the right; SPEED to run",BLACK);
    }
    line(v,427,40,427,230,DARK);
    for (i = 0; i < 7 && v->cpu_page+i < v->info->online; ++i) {
        unsigned cpu = v->cpu_page+i;
        AACpu *c = &v->info->cpus[cpu];
        snprintf(text,sizeof(text),"[%c] CPU %u C%u %.26s",v->selected[cpu]?'x':' ',cpu,c->cluster,c->name);
        label(v,440,57+i*21,text,BLACK);
        if (v->run->phase == 3) {
            unsigned k;
            for(k=0;k<v->run->count;++k) if(v->run->workers[k].cpu==cpu) {
                snprintf(text,sizeof(text),"    %.3f M Dhrystones/s",v->run->per_cpu[k]/1000000.0);
                label(v,440,66+i*21,text,BLUE);
            }
        }
    }
    button(v,440,204,105,"PREV",0); button(v,555,204,105,"NEXT",0);
    button(v,670,204,110,"ALL",0);
    rect(v,8,242,784,27,DARK); label(v,18,261,"SPEED COMPARISONS",WHITE);
    label(v,440,261,(v->run->phase == 3 ? v->run->count > 1 : v->smp)?"SMP - cumulative throughput":"SINGLE - one CPU",WHITE);
    snprintf(text,sizeof(text),"Dhrystones/s  %.0f",rate); label(v,18,291,text,BLACK);
    rect(v,370,275,410,20,BG); bevel(v,370,275,410,20,1);
    if (rate > 0) rect(v,372,277,(int)(406*rate/maximum),16,RED);
    for (i = 0; i < AA_REFERENCE_COUNT; ++i) {
        int y = 300+i*14;
        label(v,18,y+10,aa_references[i].name,BLACK);
        if (rate > 0) snprintf(text,sizeof(text),"%8.2fx",rate/aa_references[i].dhry);
        else strcpy(text,"      --");
        label(v,245,y+10,text,BLACK);
        bevel(v,370,y,410,13,1);
        rect(v,372,y+2,(int)(406*aa_references[i].dhry/maximum)+1,9,BLUE);
    }
    for (i=0;i<v->info->online;++i) count += !!v->selected[i];
    snprintf(text,sizeof(text),"DMIPS %.2f  | selected %u",rate/1757.0,count);
    label(v,18,426,text,BLACK);
    if (v->single > 0 && v->run->count > 1 && rate > 0) {
        snprintf(text,sizeof(text),"Speedup %.2fx",rate/v->single); label(v,440,426,text,BLACK);
    }
    button(v,8,439,77,"QUIT",0); button(v,92,439,95,"MEMORY",0);
    button(v,194,439,95,"BOARDS",0); button(v,296,439,90,"DRIVES",0);
    button(v,393,439,90,v->active?"CANCEL":"SPEED",v->active);
    button(v,490,439,90,"PRINT",0); button(v,587,439,95,"SINGLE",!v->smp);
    button(v,689,439,100,"SMP",v->smp);
    /* Window title carries status without changing the classic panel layout. */
    SetWindowTitles(v->window,(CONST_STRPTR)v->status,(CONST_STRPTR)-1);
}

static void start(View *v,int baseline)
{
    unsigned ids[AA_MAX_CPUS], n = 0, i;
    for (i=0;i<v->info->online;++i) if (v->selected[i]) ids[n++] = i;
    if (!n) { strcpy(v->status,"Select at least one CPU"); return; }
    if (!v->smp || baseline) n = 1;
    v->active = aa_begin(v->run,v->info,ids,n,1000);
    if (!v->active && v->run->phase == 2) v->active = 1;
    snprintf(v->status,sizeof(v->status),"%s",v->active ? (baseline?"Measuring single CPU baseline...":"Measuring Dhrystone...") : v->run->error);
}

static void inventory_write(void *ctx,const char *s) { fputs(s,(FILE *)ctx); }

static void save_report(View *v)
{
    FILE *f = fopen("RAM:xSysInfo.txt","w");
    unsigned i;
    if (!f) { strcpy(v->status,"Cannot create RAM:xSysInfo.txt"); return; }
    if (v->info->inventory) xsi_report(v->info->inventory,XSI_REPORT_FULL,inventory_write,f);
    fprintf(f,"xSysInfo | %s | %s\n",v->info->platform,v->info->os);
    fprintf(f,"Dhrystone 2.1, explicit per-worker state, -O2 -fno-builtin -fno-lto -fno-inline\nCompiler: %s\n",__VERSION__);
    fprintf(f,"%s; %u workers; 3 trials, median; Dhrystones/s %.2f; DMIPS %.2f\n",
        v->run->count>1?"SMP cumulative throughput":"SINGLE",v->run->count,v->run->rate,v->run->rate/1757.0);
    for(i=0;i<v->run->count;++i) fprintf(f,"CPU %u: %.2f Dhrystones/s\n",v->run->workers[i].cpu,v->run->per_cpu[i]);
    for(i=0;i<AA_REFERENCE_COUNT;++i) fprintf(f,"%s: %llu reference Dhrystones/s; %.2fx\n",aa_references[i].name,
        (unsigned long long)aa_references[i].dhry,v->run->rate/aa_references[i].dhry);
    if(v->single>0 && v->run->count>1) fprintf(f,"Speedup %.2fx relative to CPU %u (%.2f Dhrystones/s)\n",v->run->rate/v->single,v->run->workers[0].cpu,v->single);
    fprintf(f,"RESULT %s\n",v->run->phase==3?"PASS":v->run->error[0]?v->run->error:"NOT RUN");
    { int failed = ferror(f); if (fclose(f)) failed = 1;
      strcpy(v->status,failed?"Report write failed":"Report saved: RAM:xSysInfo.txt"); }
}

int aa_gui(AAInfo *info)
{
    View *v = calloc(1,sizeof(*v));
    struct TextAttr attr = {(STRPTR)"topaz.font",8,0,0};
    const unsigned rgb[PEN_COUNT] = {0xaaaaaa,0x000000,0xffffff,0x08657f,0xff2020,0x555555};
    unsigned i;
    int result = 20;
    if (!v) return result;
    for (i=0;i<PEN_COUNT;++i) v->pen[i] = -1;
    v->info = info; v->run = calloc(1,sizeof(*v->run));
    v->screen = LockPubScreen(NULL); v->base_font = OpenFont(&attr);
    if (!v->run || !v->screen || !v->base_font) goto out;
    set_scale(v,v->screen->Width>=1640 && v->screen->Height>=1000?2:1);
    for (i=0;i<PEN_COUNT;++i) {
        ULONG r = ((rgb[i]>>16)&255)*0x01010101UL, g = ((rgb[i]>>8)&255)*0x01010101UL, b = (rgb[i]&255)*0x01010101UL;
        v->pen[i] = ObtainBestPen(v->screen->ViewPort.ColorMap,r,g,b,TAG_DONE);
        if (v->pen[i] < 0) goto out;
    }
    strcpy(v->status,"xSysInfo - ready");
    v->window = OpenWindowTags(NULL,WA_PubScreen,(IPTR)v->screen,WA_Title,(IPTR)v->status,
        WA_InnerWidth,800*v->scale,WA_InnerHeight,480*v->scale,WA_MinWidth,800,WA_MinHeight,480,
        WA_MaxWidth,-1,WA_MaxHeight,-1,WA_CloseGadget,TRUE,WA_DragBar,TRUE,
        WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_Activate,TRUE,
        WA_IDCMP,IDCMP_CLOSEWINDOW|IDCMP_REFRESHWINDOW|IDCMP_NEWSIZE|IDCMP_MOUSEBUTTONS|IDCMP_VANILLAKEY,
        TAG_DONE);
    if (!v->window) goto out;
    SetFont(v->window->RPort,v->font);
    for(i=0;i<info->online;++i) v->selected[i] = 1;
    draw(v);
    while (!v->closing || v->active) {
        struct IntuiMessage *m;
        int redraw = 0;
        while ((m = (struct IntuiMessage *)GetMsg(v->window->UserPort))) {
            ULONG cls = m->Class; UWORD code = m->Code;
            int x = (m->MouseX-px(v,0))/v->scale;
            int y = (m->MouseY-py(v,0))/v->scale;
            ReplyMsg((struct Message *)m);
            if (cls == IDCMP_CLOSEWINDOW || (cls==IDCMP_VANILLAKEY && (code=='q'||code==27))) {
                v->closing=1; v->pending_smp=0; if(v->active) aa_cancel(v->run);
            } else if (cls == IDCMP_REFRESHWINDOW) {
                BeginRefresh(v->window); draw(v); EndRefresh(v->window,TRUE);
            } else if (cls == IDCMP_NEWSIZE) {
                int sx=(v->window->Width-v->window->BorderLeft-v->window->BorderRight)/800;
                int sy=(v->window->Height-v->window->BorderTop-v->window->BorderBottom)/480;
                set_scale(v,sx<sy?sx:sy); redraw=1;
            }
            else if (cls == IDCMP_VANILLAKEY && !v->closing) {
                redraw=1;
                if (code=='s') {
                    if(v->active) { v->pending_smp=0; aa_cancel(v->run); }
                    else { v->pending_smp=v->smp; start(v,v->smp); }
                } else if(code=='m' && !v->active) v->smp=!v->smp;
                else if(code=='p' && !v->active) save_report(v);
                else if(!v->active && (code=='b'||code=='d'||code=='e'))
                    xsi_gui_run(v->info->inventory,0,code=='e'?1:code=='d'?2:0);
            }
            else if (cls == IDCMP_MOUSEBUTTONS && code == SELECTUP && !v->closing) {
                redraw=1;
                if (y>=439 && y<469) {
                    if (x<85) { v->closing=1; v->pending_smp=0; if(v->active) aa_cancel(v->run); }
                    else if (x>=92 && x<187) (!v->active ? xsi_gui_run(v->info->inventory,0,1) : 0);
                    else if (x>=194 && x<289) (!v->active ? xsi_gui_run(v->info->inventory,0,0) : 0);
                    else if (x>=296 && x<386) (!v->active ? xsi_gui_run(v->info->inventory,0,2) : 0);
                    else if (x>=393 && x<483) {
                        if(v->active) { v->pending_smp=0; aa_cancel(v->run); }
                        else { v->pending_smp=v->smp; start(v,v->smp); }
                    } else if (x>=490 && x<580 && !v->active) save_report(v);
                    else if (x>=587 && !v->active) v->smp=x>=689;
                } else if (!v->active && x>=440 && y>=47 && y<194) {
                    unsigned cpu = v->cpu_page+(y-47)/21;
                    if(cpu<info->online) v->selected[cpu]^=1;
                } else if (y>=204 && y<234 && x>=440) {
                    if(x<545 && v->cpu_page>=7) v->cpu_page-=7;
                    else if(x>=555 && x<660 && v->cpu_page+7<info->online) v->cpu_page+=7;
                    else if(x>=670 && !v->active) for(i=0;i<info->online;++i) v->selected[i]=1;
                }
            }
        }
        if(v->active) {
            int state = aa_poll(v->run);
            if(state) {
                v->active=0; redraw=1;
                if(state>0) aa_inventory_result(v->info,v->run);
                if(state>0 && v->run->count==1) v->single=v->run->rate;
                if(state>0 && v->pending_smp && !v->closing) { v->pending_smp=0; start(v,0); }
                else { v->pending_smp=0; snprintf(v->status,sizeof(v->status),"%s",state>0?"xSysInfo - measurement complete":v->run->error); }
            }
        }
        if (SetSignal(0,SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C) {
            v->closing=1; v->pending_smp=0;
            if(v->active) aa_cancel(v->run);
        }
        if(redraw) draw(v);
        aa_sleep();
    }
    result=0;
out:
    if(v->window) CloseWindow(v->window);
    if(v->screen) {
        for(i=0;i<PEN_COUNT;++i) if(v->pen[i]>=0) ReleasePen(v->screen->ViewPort.ColorMap,v->pen[i]);
        UnlockPubScreen(NULL,v->screen);
    }
    if(v->font && v->font!=v->base_font) CloseFont(v->font);
    if(v->base_font) CloseFont(v->base_font);
    free(v->run); free(v); return result;
}
