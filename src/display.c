// SPDX-License-Identifier: BSD-2-Clause
// SPDX-FileCopyrightText: 2026 xSysInfo contributors

#include "xsysinfo.h"
#include "display.h"
#include "loading.h"

/* Called only after a frontend has successfully opened and drawn its UI.
 * A failed MUI attempt leaves the loader running for the classic fallback. */
void display_ready(struct Screen *screen)
{
    struct Task *scroller;
    Forbid();
    scroller = FindTask((CONST_STRPTR)LOADING_TASK_NAME);
    Permit();
    if (scroller && screen != NULL) {
        ScreenToFront(screen);
        /* Let Intuition install the new display before the loader
         * closes its screen and rebuilds the merged Copper list. */
        WaitTOF();
        WaitTOF();
    }
    Forbid();
    scroller = FindTask((CONST_STRPTR)LOADING_TASK_NAME);
    if (scroller)
        Signal(scroller, SIGBREAKF_CTRL_C);
    Permit();
    if (scroller && screen != NULL) {
        unsigned int frame;
        for (frame = 0; frame < 100; ++frame) {
            Forbid();
            scroller = FindTask((CONST_STRPTR)LOADING_TASK_NAME);
            Permit();
            if (!scroller)
                break;
            ScreenToFront(screen);
            WaitTOF();
        }
        ScreenToFront(screen);
    }
}
