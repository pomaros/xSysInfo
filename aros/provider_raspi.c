// SPDX-License-Identifier: BSD-2-Clause
// SPDX-FileCopyrightText: 2026 John Knipper

/* Raspberry Pi identification through public AROS resources only. */

#include <string.h>

#include <aros/kernel.h>
#include <proto/exec.h>
#include <proto/openfirmware.h>

#include "collect.h"

int xsi_provider_raspi(xsi_model *m, const char *architecture)
{
    const int P = XSI_SEC_PLATFORM;
    intptr_t base;
    char text[40];
    APTR OpenFirmwareBase;
    void *root, *prop;
    const char *model = NULL;
    uint32_t length;

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
    OpenFirmwareBase = OpenResource("openfirmware.resource");
    root = OpenFirmwareBase ? OF_OpenKey("/") : NULL;
    prop = root ? OF_FindProperty(root, "model") : NULL;
    if (prop) {
        length = OF_GetPropLen(prop);
        model = OF_GetPropValue(prop);
        /* Firmware data is borrowed. Validate its bounded string before copying. */
        if (!model || length == 0 || length > 4096 || !model[0]
            || !memchr(model, '\0', length))
            model = NULL;
    }
    if (model)
        xsi_set(m, P, "board.model", "Board", "openfirmware.resource (Device Tree /model)", "%s", model);
    else
        xsi_missing(m, P, "board.model", "Board", XSI_UNAVAILABLE, "Device Tree /model unavailable");
    if (root) OF_CloseKey(root);
    xsi_missing(m, P, "board.revision", "Board revision", XSI_UNAVAILABLE,
                "no application API (firmware mailbox)");
    xsi_missing(m, P, "sensor.temperature", "Temperature", XSI_UNAVAILABLE, "no application API");
    return 1;
}
