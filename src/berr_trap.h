/* SPDX-License-Identifier: BSD-2-Clause
 * SPDX-FileCopyrightText: 2026 Stefan Reinauer
 */

#ifndef BERR_TRAP_H
#define BERR_TRAP_H

#include <exec/types.h>

/*
 * Probe a byte or longword at 'addr' under bus/address-error protection.
 * Missing hardware must terminate the access (BERR or DSACK); these
 * routines do not change Gary's timeout mode or enable disabled timeouts.
 *
 * Returns 0 on success (*out holds the value), -1 on bus error
 * or address error (*out unchanged).
 *
 * Supports 68000 through 68060, using the active VBR on 68010+.
 * Interrupts are masked for the read; vectors and SR are restored on
 * both paths. Intended for uncached hardware registers, not writes.
 */
int berr_probe_byte(ULONG addr, UBYTE *out);
int berr_probe_long(ULONG addr, ULONG *out);

#endif /* BERR_TRAP_H */
