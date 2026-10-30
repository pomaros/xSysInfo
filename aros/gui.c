/* SPDX-License-Identifier: MIT
 * Copyright (C) 2026 John KNIPPER. */
#include "sysinfo.h"
#include "gui.h"
#include "report.h"
#include "classic_logo.h"
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

/* Amiga high-resolution pixels are twice as tall as they are wide. */
static int px(View *v,int x) { int inner=v->window->Width-v->window->BorderLeft-v->window->BorderRight; return v->window->BorderLeft+(inner-640*v->scale)/2+x*v->scale; }
static int py(View *v,int y) { int inner=v->window->Height-v->window->BorderTop-v->window->BorderBottom; return v->window->BorderTop+(inner-432*v->scale)/2+y*2*v->scale; }
static void set_scale(View *v,int scale) { v->scale=scale<1?1:scale>4?4:scale; v->font=v->base_font; }
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
/* Draw the ROM bitmap without smoothing, preserving the Amiga pixel aspect. */
static void label(View *v,int x,int y,const char *s,int color)
{
    struct TextFont *f=v->base_font;
    const ULONG *loc=f->tf_CharLoc;
    const UBYTE *bits=f->tf_CharData;
    for(;*s && x+8<640;++s,x+=8) {
        unsigned ch=(unsigned char)*s;
        unsigned index=ch>=f->tf_LoChar && ch<=f->tf_HiChar?ch-f->tf_LoChar:f->tf_HiChar-f->tf_LoChar+1;
        unsigned off=loc[index]>>16, width=loc[index]&65535, r,c;
        for(r=0;r<f->tf_YSize;++r) for(c=0;c<width && c<8;++c)
            if(bits[r*f->tf_Modulo+(off+c)/8] & (128>>((off+c)%8))) rect(v,x+c,y-7+r,1,1,color);
    }
}
static void bevel(View *v,int x,int y,int w,int h,int sunken)
{
    rect(v,x,y,w,1,sunken?DARK:WHITE); rect(v,x,y,1,h,sunken?DARK:WHITE);
    rect(v,x,y+h-1,w,1,sunken?WHITE:DARK); rect(v,x+w-1,y,1,h,sunken?WHITE:DARK);
}
/* Centre the ink bounds, not the nominal advance (glyphs have side bearings). */
static void button(View *v,int x,int y,int w,const char *s,int selected)
{
    struct TextFont *f=v->base_font; const ULONG *loc=f->tf_CharLoc; const UBYTE *bits=f->tf_CharData;
    int lo=9999,hi=-1,top=9999,bottom=-1,n;
    for(n=0;s[n];++n) {
        unsigned ch=(unsigned char)s[n], index=ch>=f->tf_LoChar && ch<=f->tf_HiChar?ch-f->tf_LoChar:f->tf_HiChar-f->tf_LoChar+1;
        unsigned off=loc[index]>>16,r,c;
        for(r=0;r<f->tf_YSize;++r) for(c=0;c<(loc[index]&65535) && c<8;++c)
            if(bits[r*f->tf_Modulo+(off+c)/8] & (128>>((off+c)%8))) {
                int xx=n*8+c; if(xx<lo) lo=xx; if(xx>hi) hi=xx;
                if((int)r<top) top=r; if((int)r>bottom) bottom=r;
            }
    }
    rect(v,x,y,w,11,BG); bevel(v,x,y,w,11,selected);
    if(hi>=lo) label(v,x+(w-(hi-lo+1))/2-lo,y+(11-(bottom-top+1))/2-top+7,s,BLACK);
}
static void gradient(View *v,int x,int y,int w,int h,int left,int right)
{
    static const unsigned char bayer[4][4]={{0,8,2,10},{12,4,14,6},{3,11,1,9},{15,7,13,5}};
    int xx,yy; rect(v,x,y,w,h,left);
    for(yy=0;yy<h;++yy) for(xx=0;xx<w;++xx)
        if(bayer[yy&3][xx&3]*w < xx*16) rect(v,x+xx,y+yy,1,1,right);
}
static void heading(View *v,int x,int y,int w,const char *name,const char *action)
{
    gradient(v,x,y,w,13,DARK,BG); bevel(v,x,y,w,13,0);
    rect(v,x+2,y+2,(int)strlen(name)*8+2,9,DARK);
    label(v,x+5,y+9,name,BLACK); label(v,x+4,y+8,name,WHITE);
    if(action) { int aw=(int)strlen(action)*8+6; rect(v,x+w-aw,y+1,aw-1,11,DARK); bevel(v,x+w-aw,y,aw,13,0); label(v,x+w-aw+3,y+8,action,WHITE); }
}
static void field(View *v,int x,int y,int value_x,const char *name,const char *value)
{ label(v,x,y,name,BLACK); label(v,value_x,y,value,WHITE); }
static void draw(View *v)
{
    unsigned i,r,c,selected=0; char text[128];
    double rate=v->run->phase==3?v->run->rate:0;
    double maximum=rate>127077293?rate:127077293;
    SetAPen(v->window->RPort,v->pen[BG]);
    RectFill(v->window->RPort,v->window->BorderLeft,v->window->BorderTop,v->window->Width-v->window->BorderRight-1,v->window->Height-v->window->BorderBottom-1);
    gradient(v,1,1,319,21,BLUE,DARK); gradient(v,320,1,319,21,DARK,RED); bevel(v,0,0,640,23,0);
    for(i=0;i<2;++i) for(r=0;r<XSYSINFO_LOGO_H;++r) for(c=0;c<XSYSINFO_LOGO_W;++c)
        if(xsysinfo_logo_template[r][c/16] & (0x8000>>(c%16))) rect(v,7+c+(i?0:1),3+r+(i?0:1),1,1,i?WHITE:BLACK);
    label(v,270,9,"xSysInfo - PomAROS AArch64",WHITE);
    label(v,238,18,"https://github.com/pomaros/xSysInfo",WHITE);
    bevel(v,0,24,366,74,0); heading(v,0,24,366,"SYSTEM SOFTWARE INSTALLED",">OVERVIEW");
    field(v,4,45,104,"OS",v->info->os);
    field(v,4,53,104,"CPU ABI","AArch64 / 64 bit");
    field(v,4,61,104,"Graphics","Intuition / AROS");
    field(v,4,69,104,"Benchmark","Dhrystone 2.1");
    field(v,4,77,104,"Trials","3 / median");
    field(v,4,85,104,"Mode",v->smp?"SMP aggregate":"SINGLE core");
    snprintf(text,sizeof(text),"%u present / %u online",v->info->present,v->info->online); field(v,4,93,104,"CPUs",text);
    bevel(v,368,24,272,192,0); heading(v,368,24,272,"HARDWARE INFORMATION",">MORE");
    const char *machine=v->info->platform;
    if(!strncmp(machine,"Apple ",6)) machine+=6;
    snprintf(text,sizeof(text),"%.22s",machine); field(v,372,45,460,"Machine",text);
    snprintf(text,sizeof(text),"%llu MB",(unsigned long long)(v->info->memory_total>>20)); field(v,372,53,460,"RAM",text);
    snprintf(text,sizeof(text),"%llu MB",(unsigned long long)(v->info->memory_free>>20)); field(v,372,61,460,"Free",text);
    label(v,372,72,"CPU / SELECT / Dhrystones",BLACK);
    for(i=0;i<8 && v->cpu_page+i<v->info->online;++i) {
        unsigned cpu=v->cpu_page+i,k; double measured=0;
        for(k=0;k<v->run->count && v->run->phase==3;++k) if(v->run->workers[k].cpu==cpu) measured=v->run->per_cpu[k];
        const char *name=v->info->cpus[cpu].name;
        if(!strncmp(name,"Apple ",6)) name+=6;
        snprintf(text,sizeof(text),"[%c] %u %.*s",v->selected[cpu]?'x':' ',cpu,(int)(strcspn(name," (")<10?strcspn(name," ("):10),name); label(v,372,82+i*11,text,BLACK);
        if(v->run->phase==3) snprintf(text,sizeof(text),"%5.2fM",measured/1e6); else strcpy(text,"    --");
        label(v,576,82+i*11,text,WHITE);

    }
    button(v,372,189,80,"PREV",0); button(v,456,189,80,"NEXT",0); button(v,540,189,96,"ALL",0);
    button(v,372,203,128,"SINGLE",!v->smp); button(v,504,203,132,"SMP",v->smp);
    bevel(v,0,98,366,118,0); heading(v,0,98,366,"SPEED COMPARISONS",v->active?">BUSY":">READY");
    for(i=0;i<=4;++i) { int x=178+i*45; line(v,x,112,x,117,BLACK); snprintf(text,sizeof(text),"%u",i*25); label(v,x-(i==4?22:0),117,text,BLACK); }
    label(v,4,125,"Dhrystones",BLACK);
    if(rate>=1000000) snprintf(text,sizeof(text),"%.1fM",rate/1e6); else snprintf(text,sizeof(text),"%.0f",rate);
    label(v,88,125,text,WHITE);
    label(v,148,125,"You",WHITE);
    bevel(v,178,119,182,8,1); if(rate>0) rect(v,179,120,(int)(179*rate/maximum)+1,5,RED);
    for(i=0;i<AA_REFERENCE_COUNT;++i) {
        int y=128+i*8;
        const char *names[]={"A600 68000 7MHz","B2000 68000 7M","A1200 EC020 14M","A3000 68030 25M","A4000 68040 25M","A4000 68060 50M","M1 Firestorm x1","M1 SMP x8"};
        label(v,4,y+7,names[i],BLACK);
        if(rate>0) snprintf(text,sizeof(text),"%5.1f",rate/aa_references[i].dhry); else strcpy(text,"   --");
        /* Keep long modern/Amiga ratios within their column. */
        if(strlen(text)>6) snprintf(text,sizeof(text),"%.0fk",rate/aa_references[i].dhry/1000);
        label(v,128,y+7,text,WHITE);
        bevel(v,178,y,182,8,1); rect(v,179,y+1,(int)(179*aa_references[i].dhry/maximum)+1,5,BLUE);
    }
    for(i=0;i<v->info->online;++i) selected+=!!v->selected[i];
    snprintf(text,sizeof(text),"DMIPS %.0f",rate/1757); label(v,4,200,text,BLACK);
    snprintf(text,sizeof(text),"%u CPUs / %s",selected,v->smp?"SMP":"ONE"); label(v,4,211,text,BLACK);
    button(v,178,193,60,"QUIT",0); button(v,239,193,61,"MEMORY",0); button(v,301,193,61,"BOARDS",0);
    button(v,178,205,60,"DRIVES",0); button(v,239,205,61,v->active?"CANCEL":"SPEED",v->active); button(v,301,205,61,"PRINT",0);
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
    set_scale(v,v->screen->Width>=1320 && v->screen->Height>=880?2:1);
    for (i=0;i<PEN_COUNT;++i) {
        ULONG r = ((rgb[i]>>16)&255)*0x01010101UL, g = ((rgb[i]>>8)&255)*0x01010101UL, b = (rgb[i]&255)*0x01010101UL;
        v->pen[i] = ObtainBestPen(v->screen->ViewPort.ColorMap,r,g,b,TAG_DONE);
        if (v->pen[i] < 0) goto out;
    }
    strcpy(v->status,"xSysInfo - ready");
    v->window = OpenWindowTags(NULL,WA_PubScreen,(IPTR)v->screen,WA_Title,(IPTR)v->status,
        WA_InnerWidth,640*v->scale,WA_InnerHeight,432*v->scale,
        WA_CloseGadget,TRUE,WA_DragBar,TRUE,
        WA_DepthGadget,TRUE,WA_Activate,TRUE,
        WA_IDCMP,IDCMP_CLOSEWINDOW|IDCMP_REFRESHWINDOW|IDCMP_MOUSEBUTTONS|IDCMP_VANILLAKEY,
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
            int y = (m->MouseY-py(v,0))/(2*v->scale);
            ReplyMsg((struct Message *)m);
            if (cls == IDCMP_CLOSEWINDOW || (cls==IDCMP_VANILLAKEY && (code=='q'||code==27))) {
                v->closing=1; v->pending_smp=0; if(v->active) aa_cancel(v->run);
            } else if (cls == IDCMP_REFRESHWINDOW) {
                BeginRefresh(v->window); draw(v); EndRefresh(v->window,TRUE);
            }
            else if (cls == IDCMP_VANILLAKEY && !v->closing) {
                redraw=1;
                if (code=='s') {
                    if(v->active) { v->pending_smp=0; aa_cancel(v->run); }
                    else { v->pending_smp=v->smp; start(v,v->smp); }
                } else if(code=='m' && !v->active) v->smp=!v->smp;
                else if(code=='p' && !v->active) save_report(v);
                else if(!v->active && (code=='b'||code=='d'||code=='e'))
                    { xsi_gui_run(v->info->inventory,0,code=='e'?1:code=='d'?2:0); ActivateWindow(v->window); }
            }
            else if (cls == IDCMP_MOUSEBUTTONS && code == SELECTUP && !v->closing) {
                redraw=1;
                if (x>=178 && x<362 && y>=193 && y<216) {
                    int col=(x-178)/61, row=y>=205;
                    if(!row && col==0) { v->closing=1; v->pending_smp=0; if(v->active) aa_cancel(v->run); }
                    else if(row && col==1) {
                        if(v->active) { v->pending_smp=0; aa_cancel(v->run); }
                        else { v->pending_smp=v->smp; start(v,v->smp); }
                    } else if(!v->active) {
                        if(row && col==2) save_report(v);
                        else { xsi_gui_run(v->info->inventory,0,row?2:col==1?1:0); ActivateWindow(v->window); }
                    }
                } else if(!v->active && x>=372 && y>=75 && y<166) {
                    unsigned cpu=v->cpu_page+(y-75)/11; if(cpu<info->online) v->selected[cpu]^=1;
                } else if(x>=372 && y>=189 && y<200) {
                    if(x<452 && v->cpu_page>=8) v->cpu_page-=8;
                    else if(x>=456 && x<536 && v->cpu_page+8<info->online) v->cpu_page+=8;
                    else if(x>=540 && !v->active) for(i=0;i<info->online;++i) v->selected[i]=1;
                } else if(x>=372 && y>=203 && !v->active) v->smp=x>=504;
                else if(x>=592 && y>=24 && y<37 && !v->active) { xsi_gui_run(v->info->inventory,0,0); ActivateWindow(v->window); }
                else if(x>=284 && x<366 && y>=24 && y<37 && !v->active) { xsi_gui_run(v->info->inventory,0,0); ActivateWindow(v->window); }

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
