// SPDX-License-Identifier: BSD-2-Clause
// SPDX-FileCopyrightText: 2026 xSysInfo contributors

/*
 * xSysInfo - MUI interface header
 */

#ifndef MUI_GUI_H
#define MUI_GUI_H

#include "xsysinfo.h"

/*
 * Run the MUI interface until the user quits.
 * Returns FALSE before entering the event loop when MUI is unavailable or the
 * interface could not be created, so the caller can fall back to the
 * classic interface.
 */
BOOL mui_gui_run(const char *version_string, const char *startup_warning);

#endif /* MUI_GUI_H */
