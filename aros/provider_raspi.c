// SPDX-License-Identifier: BSD-2-Clause
// SPDX-FileCopyrightText: 2026 John Knipper

/*
 * xSysInfo for AROS: native Raspberry Pi provider.
 *
 * The Pi kernel publishes the SoC peripheral base; the board revision and
 * model live in the firmware mailbox and the device tree, which AROS does
 * not expose to applications. Those fields are reported unavailable, not
 * guessed from the peripheral base.
 */

#include <string.h>

#include <aros/kernel.h>

#include "collect.h"

int xsi_provider_raspi(xsi_model *m, const char *architecture)
{
    const int P = XSI_SEC_PLATFORM;
    intptr_t base;
    char text[40];

    /* Upstream kernel uses CPU-board order; existing builds also use board-CPU. */
    if (!architecture || (strncmp(architecture, "raspi-", 6) != 0
        && strcmp(architecture, "aarch64-raspi") != 0
        && strcmp(architecture, "arm-raspi") != 0))
        return 0;
    m->platform = XSI_PLATFORM_RASPI;
    m->platform_name = "raspi";
    xsi_set(m, P, "platform.kind", "Platform", "kernel.resource", "Raspberry Pi, native");
    xsi_set(m, P, "platform.owner", "Hardware owner", "kernel.resource", "AROS");
    base = xsi_kernel_attr(KATTR_PeripheralBase);
    if (base != -1 && base != 0)
        xsi_set(m, P, "soc.peripherals", "Peripheral base", "kernel.resource", "%s",
                xsi_format_address((uint64_t)base, m->pointer_bits, text, sizeof text));
    else
        xsi_missing(m, P, "soc.peripherals", "Peripheral base", XSI_UNKNOWN, "kernel.resource");
    xsi_missing(m, P, "board.model", "Board", XSI_UNAVAILABLE, "no application API (firmware mailbox)");
    xsi_missing(m, P, "board.revision", "Board revision", XSI_UNAVAILABLE,
                "no application API (firmware mailbox)");
    xsi_missing(m, P, "sensor.temperature", "Temperature", XSI_UNAVAILABLE, "no application API");
    return 1;
}
