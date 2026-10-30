/* SPDX-License-Identifier: BSD-2-Clause */
#ifndef XSYSINFO_ROM_H
#define XSYSINFO_ROM_H

#include <exec/types.h>

/* Size of the mapped Kickstart and its extensions, in KiB. */
ULONG detect_rom_size(BOOL a1200, BOOL romy);

#endif
