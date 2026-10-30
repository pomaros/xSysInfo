// SPDX-License-Identifier: BSD-2-Clause
// SPDX-FileCopyrightText: 2026 John Knipper

#ifndef XSI_GUI_H
#define XSI_GUI_H

#include "model.h"

/* Opens the xSysInfo window on the default public screen and runs until
 * the user quits. Returns 0 when the window could not be opened. */
int xsi_gui_run(xsi_model *m, int dark, int initial_view);

#endif
