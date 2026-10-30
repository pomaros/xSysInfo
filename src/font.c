// SPDX-License-Identifier: BSD-2-Clause
// SPDX-FileCopyrightText: 2026 Stefan Reinauer

#include <string.h>
#include <dos/dosextens.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/diskfont.h>

#include "font.h"

#define FONT_NAME_SIZE 256
#define FONT_MAX_HEIGHT 8
#define FONT_MAX_WIDTH 8

struct Library *DiskfontBase;
static char font_name[FONT_NAME_SIZE] = "topaz.font";
static struct TextAttr font_attr = {(STRPTR)font_name, 8, FS_NORMAL, 0};
static LocaleStringID option_error = MSG_COUNT;

static BOOL same_name(const char *a, const char *b)
{
    while (*a && *b) {
        unsigned char x = *a++, y = *b++;
        if (x >= 'A' && x <= 'Z') x += 'a' - 'A';
        if (y >= 'A' && y <= 'Z') y += 'a' - 'A';
        if (x != y) return FALSE;
    }
    return *a == *b;
}

void set_ui_font_option(const char *option)
{
    const char *dot = strrchr(option, '.');
    const char *p;
    ULONG height = 0;
    size_t length;

    option_error = MSG_FONT_INVALID;
    if (!dot || dot == option || !dot[1]) return;
    length = dot - option;
    if (length + sizeof(".font") > sizeof(font_name)) return;
    for (p = dot + 1; *p; p++) {
        if (*p < '0' || *p > '9' || height > 6553) return;
        height = height * 10 + (*p - '0');
        if (height > 65535) return;
    }
    if (!height) return;
    memcpy(font_name, option, length);
    memcpy(font_name + length, ".font", sizeof(".font"));
    font_attr.ta_YSize = height;
    option_error = MSG_COUNT;
}

/* Include the fallback glyph after HiChar. CharLoc stores offset:width;
 * CharKern moves before drawing and CharSpace advances after drawing.
 * Check ink bounds as well as advances, including proportional fonts. */
static BOOL font_fits(const struct TextFont *font)
{
    const ULONG *locations = font->tf_CharLoc;
    const WORD *spacing = font->tf_CharSpace;
    const WORD *kerning = font->tf_CharKern;
    unsigned i, count;

    if (!font->tf_YSize || font->tf_YSize > FONT_MAX_HEIGHT ||
        font->tf_Baseline >= font->tf_YSize ||
        font->tf_XSize > FONT_MAX_WIDTH ||
        (font->tf_Flags & FPF_REVPATH) || !locations ||
        font->tf_HiChar < font->tf_LoChar)
        return FALSE;
    count = (unsigned)font->tf_HiChar - font->tf_LoChar + 2;
    for (i = 0; i < count; i++) {
        LONG kern = kerning ? kerning[i] : 0;
        LONG advance = kern + (spacing ? spacing[i] : font->tf_XSize);
        LONG ink_right = kern + (locations[i] & 0xffff);
        if (kern < 0 || advance < 0 || advance > FONT_MAX_WIDTH ||
            ink_right > FONT_MAX_WIDTH)
            return FALSE;
    }
    return TRUE;
}

struct TextFont *open_ui_font(LocaleStringID *error)
{
    struct TextAttr topaz = {(STRPTR)"topaz.font", 8, FS_NORMAL, FPF_ROMFONT};
    struct TextFont *font = NULL;

    *error = option_error;
    if (*error == MSG_COUNT && font_attr.ta_YSize > FONT_MAX_HEIGHT)
        *error = MSG_FONT_TOO_LARGE;
    if (*error == MSG_COUNT) {
        if (same_name(font_name, "topaz.font") && font_attr.ta_YSize == 8) {
            font = OpenFont(&topaz);
        } else {
            struct Process *proc = (struct Process *)FindTask(NULL);
            APTR old_window = proc->pr_WindowPtr;
            proc->pr_WindowPtr = (APTR)-1;
            DiskfontBase = OpenLibrary((CONST_STRPTR)"diskfont.library", 34);
            if (DiskfontBase) {
                font = OpenDiskFont(&font_attr);
                CloseLibrary(DiskfontBase);
                DiskfontBase = NULL;
            }
            proc->pr_WindowPtr = old_window;
        }
        if (!font) {
            *error = MSG_FONT_UNAVAILABLE;
        } else if (!font_fits(font)) {
            CloseFont(font);
            font = NULL;
            *error = MSG_FONT_TOO_LARGE;
        }
    }
    if (!font) {
        font = OpenFont(&topaz);
        if (font && !font_fits(font)) {
            CloseFont(font);
            font = NULL;
        }
    }
    return font;
}
