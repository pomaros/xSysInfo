/* SPDX-License-Identifier: BSD-2-Clause
 * SPDX-FileCopyrightText: 2026 Stefan Reinauer
 */
#ifndef XSYSINFO_FONT_H
#define XSYSINFO_FONT_H

#include <graphics/text.h>
#include "locale_str.h"

/* FONT=name.size; copied because Workbench frees the icon after parsing. */
void set_ui_font_option(const char *option);

/* Returns an owned font, falling back to Topaz 8 on invalid requests.
 * error is MSG_COUNT on success, otherwise a localized fallback message.
 * The caller must CloseFont() after closing its display. */
struct TextFont *open_ui_font(LocaleStringID *error);

#endif
