// SPDX-License-Identifier: BSD-2-Clause
#ifndef DISPLAY_H
#define DISPLAY_H
struct Screen;
/* Pass a screen to bring forward, or NULL for a Workbench window. */
void display_ready(struct Screen *screen);
#endif
