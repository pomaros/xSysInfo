// SPDX-License-Identifier: BSD-2-Clause
// SPDX-FileCopyrightText: 2026 John Knipper

#ifndef XSI_REPORT_H
#define XSI_REPORT_H

#include "model.h"

typedef void (*xsi_write_fn)(void *ctx, const char *text);

typedef enum { XSI_REPORT_BRIEF, XSI_REPORT_FULL } xsi_report_mode;

/* Writes the text report through write(ctx, line) calls, one line each,
 * newline included. BRIEF: the field sections and list counts. FULL: also
 * every source, library, device, resource, memory region and volume. */
void xsi_report(const xsi_model *m, xsi_report_mode mode, xsi_write_fn write, void *ctx);

#endif
