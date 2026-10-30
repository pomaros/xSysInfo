// SPDX-License-Identifier: BSD-2-Clause
// SPDX-FileCopyrightText: 2026 Stefan Reinauer

#include <exec/execbase.h>
#include <exec/resident.h>
#include <proto/exec.h>

#include "rom.h"

/* A readable header alone cannot distinguish a ROM from its mirrors.
 * Use Exec's registered residents: a relocated MatchTag must point back
 * to the address being examined. RAM residents are not ROM extensions.
 * This also avoids probing absent extension hardware. */
static BOOL rom_resident(ULONG address)
{
    const struct Resident *resident = (const struct Resident *)address;

    return !(address & 1) && !TypeOfMem((APTR)address) &&
           resident->rt_MatchWord == RTC_MATCHWORD &&
           resident->rt_MatchTag == resident;
}

/* ROMY decodes a 4 MiB window at $01000000. Its 2/4 MiB images consist
 * of a 1536/3584 KiB extension followed by a mirror of the main ROM.
 * Only inspect this window after finding a resident relocated into it.
 * The extension footer distinguishes sparse images and rejects the
 * repeated footer of a smaller ROM. amitools writes size at end - 20. */
static ULONG romy_size(ULONG resident_end)
{
    const volatile ULONG *header = (const volatile ULONG *)0x01000000;
    static const ULONG sizes[] = { 0x00180000, 0x00380000 };
    unsigned int i, word;

    if (header[0] != 0x11144ef9 || header[1] != 0x00f80002 ||
        header[2] != 0x0000ffff)
        return 0;

    for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
        ULONG end = 0x01000000 + sizes[i];
        const volatile UWORD *footer = (const volatile UWORD *)(end - 16);

        if (resident_end > end ||
            *(const volatile ULONG *)(end - 20) != sizes[i])
            continue;
        for (word = 0; word < 8; ++word) {
            if (footer[word] != 0x18 + word)
                break;
        }
        if (word == 8)
            return sizes[i] / 1024;
    }
    return 0;
}

ULONG detect_rom_size(BOOL a1200, BOOL romy)
{
    const ULONG *modules;
    ULONG size, entry, high_end = 0;
    BOOL e0 = FALSE, a8 = FALSE;
    unsigned int remaining = 4096;

    size = *(const volatile UWORD *)0x00f80000 == 0x1111 ? 256 : 512;
    Forbid();
    modules = (const ULONG *)SysBase->ResModules;
    /* Bit 31 denotes a link to another resident array, not a ROM tag.
     * Bound traversal as well, so a damaged link cannot hang detection. */
    while (modules && remaining-- && (entry = *modules++)) {
        const struct Resident *resident;
        ULONG end;

        if (entry & 0x80000000UL) {
            modules = (const ULONG *)(entry & 0x7fffffffUL);
            continue;
        }
        if (!((entry >= 0x00e00000 && entry < 0x00e80000) ||
              (a1200 && entry >= 0x00a80000 && entry < 0x00b80000) ||
              (romy && entry >= 0x01000000 && entry < 0x01380000)))
            continue;
        if (!rom_resident(entry))
            continue;
        resident = (const struct Resident *)entry;
        end = (ULONG)resident->rt_EndSkip;
        if (end < entry + sizeof(*resident))
            continue;
        if (entry >= 0x01000000) {
            if (end <= 0x01380000 && end > high_end)
                high_end = end;
        } else if (entry >= 0x00e00000) {
            if (end <= 0x00e80000)
                e0 = TRUE;
        } else if (end <= 0x00b80000) {
            a8 = TRUE;
        }
    }
    Permit();

    /* Only count banks containing residents at their actual addresses.
     * In particular, do not count the F8 image mirrored inside ROMY. */
    /* The A1200's larger chips also supply the E0 bank, even when all
     * extension residents happen to lie in the A8/B0 portion. */
    if (a8)
        size += 1536;
    else if (e0)
        size += 512;
    if (high_end)
        size += romy_size(high_end);
    return size;
}
