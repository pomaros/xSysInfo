// SPDX-License-Identifier: BSD-2-Clause
// SPDX-FileCopyrightText: 2026 John Knipper

/*
 * xSysInfo for AROS: entry point.
 *
 *   xSysInfo [BRIEF] [FULL] [TO <file>] [DARK] [WHICH] [SCSI] [DEBUG]
 *
 * As upstream: BRIEF and FULL write a report without opening the GUI, FULL
 * wins over BRIEF. TO sends it to a file instead of the Shell. WHICH and
 * SCSI are Amiga-hardware reports and are refused here. From Workbench, or
 * with no report keyword, the GUI opens. The report goes through dos
 * Output(), which Shell redirection follows.
 */

#include <exec/types.h>
#include <dos/dos.h>
#include <dos/rdargs.h>
#include <workbench/startup.h>
#include <proto/exec.h>
#include <proto/dos.h>

#include <string.h>

#include "collect.h"
#include "gui.h"
#include "report.h"

#ifndef XSYSINFO_VERSION
#define XSYSINFO_VERSION "0.0"
#endif
#ifndef XSYSINFO_DATE
#define XSYSINFO_DATE "0.0.0"
#endif

const char version_string[] = "$VER: xSysInfo " XSYSINFO_VERSION " (" XSYSINFO_DATE ") AROS";

static xsi_model model;

static void write_handle(void *ctx, const char *text)
{
    FPuts((BPTR)ctx, (CONST_STRPTR)text);
}

enum { A_BRIEF, A_FULL, A_TO, A_DARK, A_WHICH, A_SCSI, A_DEBUG, A_COUNT };

int main(int argc, char **argv)
{
    IPTR args[A_COUNT] = { 0 };
    struct RDArgs *rda = NULL;
    int rc = RETURN_OK;
    int dark = 0;
    (void)argv;

    if (argc != 0) {
        rda = ReadArgs((CONST_STRPTR)"BRIEF/S,FULL/S,TO/K,DARK/S,WHICH/S,SCSI/S,DEBUG/S",
                       (SIPTR *)args, NULL);
        if (!rda) {
            PrintFault(IoErr(), (CONST_STRPTR)"xSysInfo");
            return RETURN_FAIL;
        }
        if (args[A_WHICH] || args[A_SCSI]) {
            PutStr((CONST_STRPTR)"xSysInfo: WHICH and SCSI read Amiga hardware and are not "
                                 "available on AROS AArch64\n");
            FreeArgs(rda);
            return RETURN_WARN;
        }
        dark = args[A_DARK] != 0;
    }

    xsi_model_init(&model);
    xsi_collect(&model);

    if (argc != 0 && (args[A_BRIEF] || args[A_FULL])) {
        BPTR out = Output(), file = BNULL;
        if (args[A_TO]) {
            file = Open((CONST_STRPTR)args[A_TO], MODE_NEWFILE);
            if (!file) {
                PrintFault(IoErr(), (CONST_STRPTR)"xSysInfo TO");
                FreeArgs(rda);
                return RETURN_FAIL;
            }
            out = file;
        }
        xsi_report(&model, args[A_FULL] ? XSI_REPORT_FULL : XSI_REPORT_BRIEF,
                   write_handle, (void *)out);
        if (file && !Close(file))
            rc = RETURN_FAIL;
        else if (!file)
            Flush(out);
    } else {
        rc = xsi_gui_run(&model, dark) ? RETURN_OK : RETURN_FAIL;
    }
    if (rda)
        FreeArgs(rda);
    return rc;
}
