// SPDX-License-Identifier: BSD-2-Clause
// SPDX-FileCopyrightText: 2025 Stefan Reinauer
// SPDX-FileCopyrightText: 2026 John Knipper

/*
 * xSysInfo for AROS: the window.
 *
 * The look is upstream's: the eight-colour palettes, the title strips, 3D
 * panels and buttons, the software list on the left, the information panel
 * on the right, the Memory and Drives pages. The drawing primitives
 * (panel, 3D box, button, clipped text) are adapted from src/gui.c; what is
 * drawn comes from the model, so the window and the text report can never
 * disagree. Always a window on the public screen: every AArch64 AROS
 * display is RTG, which is where upstream opens a window too.
 */

#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <graphics/gfx.h>
#include <graphics/text.h>
#include <graphics/rastport.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/dos.h>

#include <stdio.h>
#include <string.h>

#include "gui.h"
#include "report.h"

#define WIN_W 640
#define WIN_H 256
#define NUM_COLORS 8
#define LINE_H 8

/* Upstream's palettes (src/main.c), 12-bit RGB. */
static const UWORD default_palette[NUM_COLORS] = {
    0x0AAA, 0x0666, 0x0000, 0x0FFF, 0x0068, 0x0F00, 0x0DDD, 0x0444
};
static const UWORD dark_palette[NUM_COLORS] = {
    0x0112, 0x0234, 0x0CCC, 0x0FFF, 0x0258, 0x0E43, 0x0777, 0x0445
};

enum { C_BG, C_TITLE, C_TEXT, C_HIGH, C_BAR, C_YOU, C_LIGHT, C_DARK };

typedef enum { VIEW_MAIN, VIEW_MEMORY, VIEW_DRIVES } view_t;

typedef enum {
    B_NONE, B_QUIT, B_MEMORY, B_DRIVES, B_PRINT, B_LIST_CYCLE, B_LIST_UP, B_LIST_DOWN,
    B_INFO_CYCLE, B_PREV, B_NEXT, B_EXIT, B_COUNT
} button_id;

typedef struct {
    WORD x, y, w, h;
    const char *label;
    int enabled, pressed;
} button_t;

static struct {
    xsi_model *m;
    struct Window *win;
    struct RastPort *rp;
    struct Screen *screen;
    struct TextFont *font;
    LONG pens[NUM_COLORS];
    int pens_ok[NUM_COLORS];
    WORD ox, oy;                /* inner origin */
    view_t view;
    int list, list_scroll, info_page, region, drive_scroll;
    button_t buttons[B_COUNT];
    char status[80];
} g;

#define PEN(c) (g.pens[c])

static const int info_pages[] = {
    XSI_SEC_SYSTEM, XSI_SEC_CPU, XSI_SEC_MEMORY, XSI_SEC_PLATFORM, XSI_SEC_DISPLAY, XSI_SEC_BENCHMARK
};
#define INFO_PAGE_COUNT ((int)(sizeof info_pages / sizeof info_pages[0]))

/* ---- primitives, adapted from src/gui.c ------------------------------- */

static void fill(WORD x, WORD y, WORD w, WORD h, int c)
{
    SetAPen(g.rp, PEN(c));
    RectFill(g.rp, g.ox + x, g.oy + y, g.ox + x + w - 1, g.oy + y + h - 1);
}

static void box3d(WORD x, WORD y, WORD w, WORD h, int recessed)
{
    struct RastPort *rp = g.rp;
    x += g.ox;
    y += g.oy;
    SetAPen(rp, PEN(recessed ? C_DARK : C_LIGHT));
    Move(rp, x, y + h - 1);
    Draw(rp, x, y);
    Draw(rp, x + w - 1, y);
    SetAPen(rp, PEN(recessed ? C_LIGHT : C_DARK));
    Move(rp, x + 1, y + h - 1);
    Draw(rp, x + w - 1, y + h - 1);
    Draw(rp, x + w - 1, y + 1);
}

static void text_clipped(WORD x, WORD y, const char *s, WORD max_w, int fg, int bg)
{
    struct RastPort *rp = g.rp;
    WORD len = (WORD)strlen(s);
    SetAPen(rp, PEN(fg));
    SetBPen(rp, PEN(bg));
    SetDrMd(rp, JAM2);
    while (len > 0 && TextLength(rp, (CONST_STRPTR)s, len) > max_w)
        len--;
    if (len > 0) {
        Move(rp, g.ox + x, g.oy + y);
        Text(rp, (CONST_STRPTR)s, len);
    }
}

static void strip(WORD x, WORD y, WORD w, WORD h, const char *title)
{
    fill(x, y, w, h, C_TITLE);
    box3d(x, y, w, h, 0);
    if (title) {
        SetDrMd(g.rp, JAM1);
        SetAPen(g.rp, PEN(C_DARK));
        Move(g.rp, g.ox + x + 5, g.oy + y + h - 3);
        Text(g.rp, (CONST_STRPTR)title, strlen(title));
        SetAPen(g.rp, PEN(C_HIGH));
        Move(g.rp, g.ox + x + 4, g.oy + y + h - 4);
        Text(g.rp, (CONST_STRPTR)title, strlen(title));
        SetDrMd(g.rp, JAM2);
    }
}

static void panel(WORD x, WORD y, WORD w, WORD h, const char *title)
{
    fill(x, y, w, h, C_BG);
    box3d(x, y, w, h, 0);
    if (title)
        strip(x + 2, y + 2, w - 4, 11, title);
}

static void button_draw(button_id id)
{
    button_t *b = &g.buttons[id];
    WORD tw;
    if (!b->label)
        return;
    fill(b->x, b->y, b->w, b->h, b->enabled ? C_BG : C_DARK);
    box3d(b->x, b->y, b->w, b->h, b->pressed);
    tw = TextLength(g.rp, (CONST_STRPTR)b->label, strlen(b->label));
    text_clipped(b->x + (b->w - tw) / 2, b->y + (b->h + 6) / 2, b->label, b->w - 2,
                 b->enabled ? C_TEXT : C_BG, b->enabled ? C_BG : C_DARK);
}

static void button_set(button_id id, WORD x, WORD y, WORD w, WORD h, const char *label, int enabled)
{
    button_t *b = &g.buttons[id];
    b->x = x;
    b->y = y;
    b->w = w;
    b->h = h;
    b->label = label;
    b->enabled = enabled;
    b->pressed = 0;
}

/* ---- views ----------------------------------------------------------- */

#define LIST_X 0
#define LIST_Y 24
#define LIST_W 366
#define LIST_H 176
#define LIST_LINES 17
#define INFO_X 368
#define INFO_W 272
#define INFO_LINES 17

static void draw_header(void)
{
    char line[96];
    strip(0, 0, WIN_W, 22, NULL);
    SetDrMd(g.rp, JAM1);
    SetAPen(g.rp, PEN(C_HIGH));
    Move(g.rp, g.ox + 8, g.oy + 10);
    Text(g.rp, (CONST_STRPTR)"xSysInfo", 8);
    SetDrMd(g.rp, JAM2);
    snprintf(line, sizeof line, "AROS %u-bit, provider %s", g.m->pointer_bits, g.m->platform_name);
    text_clipped(8, 19, line, 300, C_HIGH, C_TITLE);
    text_clipped(330, 14, g.status[0] ? g.status : "", 300, C_HIGH, C_TITLE);
}

static void draw_list(void)
{
    const xsi_module_list *l = &g.m->lists[g.list];
    char addr[32], ver[16];
    int i;
    panel(LIST_X, LIST_Y, LIST_W, LIST_H, NULL);
    fill(LIST_X + 2, LIST_Y + 16, LIST_W - 4, LIST_LINES * LINE_H + 4, C_BG);
    for (i = 0; i < LIST_LINES; i++) {
        int k = g.list_scroll + i;
        WORD y = LIST_Y + 24 + i * LINE_H;
        const xsi_module *x;
        if (k >= (int)l->count)
            break;
        x = &l->items[k];
        /* 19 characters of address at 8 pixels: the full 64-bit value. */
        text_clipped(LIST_X + 6, y, x->name, 142, C_TEXT, C_BG);
        text_clipped(LIST_X + 150, y,
                     xsi_format_address(x->address, g.m->pointer_bits, addr, sizeof addr),
                     156, C_HIGH, C_BG);
        if (x->has_version)
            snprintf(ver, sizeof ver, "%u.%u", (unsigned)x->version, (unsigned)x->revision);
        else
            snprintf(ver, sizeof ver, "-");
        text_clipped(LIST_X + 310, y, ver, 52, C_TEXT, C_BG);
    }
    g.buttons[B_LIST_CYCLE].label = l->title;
    button_draw(B_LIST_CYCLE);
    g.buttons[B_LIST_UP].enabled = g.list_scroll > 0;
    g.buttons[B_LIST_DOWN].enabled = g.list_scroll + LIST_LINES < (int)l->count;
    button_draw(B_LIST_UP);
    button_draw(B_LIST_DOWN);
}

static void draw_info(void)
{
    const xsi_section *s = &g.m->sections[info_pages[g.info_page]];
    unsigned i;
    panel(INFO_X, LIST_Y, INFO_W, LIST_H, NULL);
    g.buttons[B_INFO_CYCLE].label = s->title;
    button_draw(B_INFO_CYCLE);
    for (i = 0; i < s->count && i < INFO_LINES; i++) {
        const xsi_field *f = &s->fields[i];
        WORD y = LIST_Y + 24 + (WORD)i * LINE_H;
        text_clipped(INFO_X + 6, y, f->label, 108, C_TEXT, C_BG);
        text_clipped(INFO_X + 116, y, xsi_field_text(f), INFO_W - 122,
                     f->state == XSI_OK ? C_HIGH : C_DARK, C_BG);
    }
}

static void draw_bottom(void)
{
    fill(0, 202, WIN_W, WIN_H - 202, C_BG);
    button_draw(B_MEMORY);
    button_draw(B_DRIVES);
    button_draw(B_PRINT);
    button_draw(B_QUIT);
}

static void draw_memory(void)
{
    char a[32], b[32], line[96], t[24], f[24];
    panel(0, LIST_Y, WIN_W, LIST_H, "Memory");
    if (g.m->region_count == 0) {
        text_clipped(8, LIST_Y + 30, "No memory headers reported", 400, C_TEXT, C_BG);
    } else {
        const xsi_memregion *r = &g.m->regions[g.region];
        snprintf(line, sizeof line, "Region %d of %u: %s", g.region + 1, g.m->region_count,
                 r->name[0] ? r->name : "(unnamed)");
        text_clipped(8, LIST_Y + 30, line, 600, C_HIGH, C_BG);
        snprintf(line, sizeof line, "Range      %s - %s",
                 xsi_format_address(r->lower, g.m->pointer_bits, a, sizeof a),
                 xsi_format_address(r->upper, g.m->pointer_bits, b, sizeof b));
        text_clipped(8, LIST_Y + 46, line, 600, C_TEXT, C_BG);
        snprintf(line, sizeof line, "Size       %s", xsi_format_size(r->total, t, sizeof t));
        text_clipped(8, LIST_Y + 56, line, 600, C_TEXT, C_BG);
        snprintf(line, sizeof line, "Free       %s", xsi_format_size(r->free, f, sizeof f));
        text_clipped(8, LIST_Y + 66, line, 600, C_TEXT, C_BG);
        snprintf(line, sizeof line, "Largest    %s", r->freelist_known
                 ? xsi_format_size(r->largest_free, f, sizeof f) : "[unknown]");
        text_clipped(8, LIST_Y + 76, line, 600, C_TEXT, C_BG);
        snprintf(line, sizeof line, "Attributes 0x%08lx   Priority %d",
                 (unsigned long)r->attributes, (int)r->priority);
        text_clipped(8, LIST_Y + 86, line, 600, C_TEXT, C_BG);
    }
    g.buttons[B_PREV].enabled = g.region > 0;
    g.buttons[B_NEXT].enabled = g.region + 1 < (int)g.m->region_count;
    button_draw(B_PREV);
    button_draw(B_NEXT);
    fill(0, 202, WIN_W, WIN_H - 202, C_BG);
    button_draw(B_EXIT);
}

static void draw_drives(void)
{
    char line[128], total[24], used[24];
    int i;
    panel(0, LIST_Y, WIN_W, LIST_H, "Drives");
    for (i = 0; i < 18; i++) {
        int k = g.drive_scroll + i;
        const xsi_volume *v;
        if (k >= (int)g.m->volume_count)
            break;
        v = &g.m->volumes[k];
        if (v->has_info && v->block_size)
            snprintf(line, sizeof line, "%-10s %-18s %-10s %9s of %9s  %s", v->device,
                     v->volume[0] ? v->volume : "-", xsi_volume_state_name(v->state),
                     xsi_format_size(v->used_blocks * (uint64_t)v->block_size, used, sizeof used),
                     xsi_format_size(v->total_blocks * (uint64_t)v->block_size, total, sizeof total),
                     v->handler[0] ? v->handler : "-");
        else
            snprintf(line, sizeof line, "%-10s %-18s %-10s [size unknown]  %s", v->device,
                     v->volume[0] ? v->volume : "-", xsi_volume_state_name(v->state),
                     v->handler[0] ? v->handler : "-");
        text_clipped(8, LIST_Y + 24 + i * LINE_H, line, WIN_W - 16, C_TEXT, C_BG);
    }
    if (g.m->volume_count == 0)
        text_clipped(8, LIST_Y + 24, "No running filesystem handlers", 400, C_TEXT, C_BG);
    g.buttons[B_PREV].enabled = g.drive_scroll > 0;
    g.buttons[B_NEXT].enabled = g.drive_scroll + 18 < (int)g.m->volume_count;
    button_draw(B_PREV);
    button_draw(B_NEXT);
    fill(0, 202, WIN_W, WIN_H - 202, C_BG);
    button_draw(B_EXIT);
}

static void layout(void)
{
    memset(g.buttons, 0, sizeof g.buttons);
    if (g.view == VIEW_MAIN) {
        button_set(B_LIST_CYCLE, LIST_X + 4, LIST_Y + 3, 120, 11, "", 1);
        button_set(B_LIST_UP, LIST_X + LIST_W - 40, LIST_Y + 3, 16, 11, "^", 1);
        button_set(B_LIST_DOWN, LIST_X + LIST_W - 22, LIST_Y + 3, 16, 11, "v", 1);
        button_set(B_INFO_CYCLE, INFO_X + 4, LIST_Y + 3, 140, 11, "", 1);
        button_set(B_MEMORY, 4, 206, 100, 12, "Memory", 1);
        button_set(B_DRIVES, 110, 206, 100, 12, "Drives", 1);
        button_set(B_PRINT, 216, 206, 100, 12, "Print", 1);
        button_set(B_QUIT, 536, 206, 100, 12, "Quit", 1);
    } else {
        button_set(B_PREV, 520, LIST_Y + 3, 56, 11, "Prev", 1);
        button_set(B_NEXT, 580, LIST_Y + 3, 56, 11, "Next", 1);
        button_set(B_EXIT, 536, 206, 100, 12, "Exit", 1);
    }
}

static void redraw(void)
{
    layout();
    fill(0, 0, WIN_W, WIN_H, C_BG);
    draw_header();
    if (g.view == VIEW_MAIN) {
        draw_list();
        draw_info();
        draw_bottom();
    } else if (g.view == VIEW_MEMORY) {
        draw_memory();
    } else {
        draw_drives();
    }
}

/* ---- print ----------------------------------------------------------- */

static void write_handle(void *ctx, const char *text)
{
    FPuts((BPTR)ctx, (CONST_STRPTR)text);
}

static void print_report(void)
{
    BPTR fh = Open((CONST_STRPTR)"RAM:xsysinfo.txt", MODE_NEWFILE);
    if (!fh) {
        snprintf(g.status, sizeof g.status, "Could not write RAM:xsysinfo.txt");
        return;
    }
    xsi_report(g.m, XSI_REPORT_FULL, write_handle, (void *)fh);
    snprintf(g.status, sizeof g.status, Close(fh) ? "Saved RAM:xsysinfo.txt"
                                                  : "Writing RAM:xsysinfo.txt failed");
}

/* ---- events ---------------------------------------------------------- */

static button_id hit(WORD mx, WORD my)
{
    int i;
    mx -= g.ox;
    my -= g.oy;
    for (i = 1; i < B_COUNT; i++) {
        button_t *b = &g.buttons[i];
        if (b->label && b->enabled && mx >= b->x && mx < b->x + b->w && my >= b->y && my < b->y + b->h)
            return (button_id)i;
    }
    return B_NONE;
}

static int press(button_id id)
{
    const xsi_module_list *l = &g.m->lists[g.list];
    switch (id) {
    case B_QUIT: return 0;
    case B_MEMORY: g.view = VIEW_MEMORY; break;
    case B_DRIVES: g.view = VIEW_DRIVES; break;
    case B_EXIT: g.view = VIEW_MAIN; break;
    case B_PRINT: print_report(); break;
    case B_LIST_CYCLE: g.list = (g.list + 1) % XSI_LIST_COUNT; g.list_scroll = 0; break;
    case B_LIST_UP: g.list_scroll = g.list_scroll > LIST_LINES ? g.list_scroll - LIST_LINES : 0; break;
    case B_LIST_DOWN:
        if (g.list_scroll + LIST_LINES < (int)l->count)
            g.list_scroll += LIST_LINES;
        break;
    case B_INFO_CYCLE: g.info_page = (g.info_page + 1) % INFO_PAGE_COUNT; break;
    case B_PREV:
        if (g.view == VIEW_MEMORY && g.region > 0)
            g.region--;
        else if (g.view == VIEW_DRIVES)
            g.drive_scroll = g.drive_scroll > 18 ? g.drive_scroll - 18 : 0;
        break;
    case B_NEXT:
        if (g.view == VIEW_MEMORY && g.region + 1 < (int)g.m->region_count)
            g.region++;
        else if (g.view == VIEW_DRIVES && g.drive_scroll + 18 < (int)g.m->volume_count)
            g.drive_scroll += 18;
        break;
    default: break;
    }
    redraw();
    return 1;
}

static void obtain_pens(int dark)
{
    const UWORD *pal = dark ? dark_palette : default_palette;
    struct ColorMap *cm = g.screen->ViewPort.ColorMap;
    int i;
    for (i = 0; i < NUM_COLORS; i++) {
        ULONG r = (pal[i] >> 8) & 0xf, gg = (pal[i] >> 4) & 0xf, b = pal[i] & 0xf;
        g.pens[i] = ObtainBestPenA(cm, r * 0x11111111UL, gg * 0x11111111UL, b * 0x11111111UL, NULL);
        g.pens_ok[i] = g.pens[i] >= 0;
        if (!g.pens_ok[i])
            g.pens[i] = i == C_TEXT ? 1 : 0;
    }
}

static void release_pens(void)
{
    int i;
    for (i = 0; i < NUM_COLORS; i++)
        if (g.pens_ok[i])
            ReleasePen(g.screen->ViewPort.ColorMap, g.pens[i]);
}

int xsi_gui_run(xsi_model *m, int dark)
{
    static struct TextAttr topaz8 = { (STRPTR)"topaz.font", 8, FS_NORMAL, FPF_ROMFONT };
    int running = 1;
    memset(&g, 0, sizeof g);
    g.m = m;
    g.screen = LockPubScreen(NULL);
    if (!g.screen)
        return 0;
    g.win = OpenWindowTags(NULL,
        WA_Title, (IPTR)"xSysInfo for AROS",
        WA_InnerWidth, WIN_W, WA_InnerHeight, WIN_H,
        WA_PubScreen, (IPTR)g.screen,
        WA_DragBar, TRUE, WA_DepthGadget, TRUE, WA_CloseGadget, TRUE,
        WA_Activate, TRUE, WA_SmartRefresh, TRUE, WA_RMBTrap, TRUE,
        WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_MOUSEBUTTONS | IDCMP_VANILLAKEY,
        TAG_DONE);
    if (!g.win) {
        UnlockPubScreen(NULL, g.screen);
        return 0;
    }
    g.rp = g.win->RPort;
    g.ox = g.win->BorderLeft;
    g.oy = g.win->BorderTop;
    g.font = OpenFont(&topaz8);
    if (g.font)
        SetFont(g.rp, g.font);
    obtain_pens(dark);
    redraw();

    while (running) {
        struct IntuiMessage *msg;
        WaitPort(g.win->UserPort);
        while (running && (msg = (struct IntuiMessage *)GetMsg(g.win->UserPort)) != NULL) {
            ULONG cls = msg->Class;
            UWORD code = msg->Code;
            WORD mx = msg->MouseX, my = msg->MouseY;
            ReplyMsg((struct Message *)msg);
            if (cls == IDCMP_CLOSEWINDOW)
                running = 0;
            else if (cls == IDCMP_VANILLAKEY && (code == 'q' || code == 'Q' || code == 27))
                running = 0;
            else if (cls == IDCMP_MOUSEBUTTONS && code == SELECTDOWN) {
                button_id id = hit(mx, my);
                if (id != B_NONE) {
                    g.buttons[id].pressed = 1;
                    button_draw(id);
                    running = press(id);
                }
            }
        }
    }

    release_pens();
    CloseWindow(g.win);
    if (g.font)
        CloseFont(g.font);
    UnlockPubScreen(NULL, g.screen);
    return 1;
}
