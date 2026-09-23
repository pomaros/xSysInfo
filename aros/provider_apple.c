// SPDX-License-Identifier: BSD-2-Clause
// SPDX-FileCopyrightText: 2026 John Knipper

/*
 * xSysInfo for AROS: native Apple Silicon provider.
 *
 * Reads what the Apple kernel recorded at EL1 for EL0 tasks (board, CPU
 * identity, counter frequency) through KrnGetSystemAttr(), and which Apple
 * driver resources are loaded. A loaded resource means an initialised
 * driver, not a tested function; nothing here talks to a controller.
 * The attribute numbers are the private core's public ABI
 * (aros/apple/startup.h); they are repeated here so that the common build
 * compiles against any SDK, and are only asked when the kernel says
 * "apple-".
 */

#include <string.h>

#include "collect.h"

#define APPLE_KATTR_CPU_MIDR          0x83f10003u
#define APPLE_KATTR_CURRENT_EL        0x83f10004u
#define APPLE_KATTR_COUNTER_FREQUENCY 0x83f10005u
#define APPLE_KATTR_BOARD_MODEL       0x83f10006u
#define APPLE_KATTR_BOARD_COMPATIBLE  0x83f10007u

static const char *part_name(unsigned part)
{
    /* MIDR part numbers of Apple cores, as the Linux Apple port documents
     * them (facts, not code). */
    switch (part) {
    case 0x022: return "Icestorm (M1 efficiency)";
    case 0x023: return "Firestorm (M1 performance)";
    case 0x024: return "Icestorm (M1 Pro/Max efficiency)";
    case 0x025: return "Firestorm (M1 Pro/Max performance)";
    case 0x028: return "Icestorm (M1 Max efficiency)";
    case 0x029: return "Firestorm (M1 Max performance)";
    case 0x032: return "Blizzard (M2 efficiency)";
    case 0x033: return "Avalanche (M2 performance)";
    default: return NULL;
    }
}

static const char *const apple_resources[] = {
    "pmgr.resource", "rtkit.resource", "smc.resource", "dart.resource",
    "spmi.resource", "appleusb.resource", NULL
};

int xsi_provider_apple(xsi_model *m, const char *architecture)
{
    const int P = XSI_SEC_PLATFORM, C = XSI_SEC_CPU;
    intptr_t model, compatible, midr, el, freq;
    char present[XSI_VALUE_LEN];
    size_t used = 0;
    int i;

    if (!architecture || strncmp(architecture, "apple-", 6) != 0)
        return 0;
    m->platform = XSI_PLATFORM_APPLE_NATIVE;
    m->platform_name = "apple-native";
    xsi_set(m, P, "platform.kind", "Platform", "kernel.resource", "Apple Silicon, native");
    xsi_set(m, P, "platform.owner", "Hardware owner", "kernel.resource", "AROS");

    model = xsi_kernel_attr(APPLE_KATTR_BOARD_MODEL);
    compatible = xsi_kernel_attr(APPLE_KATTR_BOARD_COMPATIBLE);
    if (model != -1 && model != 0)
        xsi_set(m, P, "board.model", "Board", "kernel.resource (FDT)", "%s", (const char *)model);
    else
        xsi_missing(m, P, "board.model", "Board", XSI_UNKNOWN, "kernel.resource (FDT)");
    if (compatible != -1 && compatible != 0)
        xsi_set(m, P, "board.compatible", "Compatible", "kernel.resource (FDT)", "%s",
                (const char *)compatible);

    midr = xsi_kernel_attr(APPLE_KATTR_CPU_MIDR);
    if (midr != -1) {
        unsigned implementer = (unsigned)((midr >> 24) & 0xff);
        unsigned variant = (unsigned)((midr >> 20) & 0xf);
        unsigned part = (unsigned)((midr >> 4) & 0xfff);
        unsigned revision = (unsigned)(midr & 0xf);
        const char *name = implementer == 0x61 ? part_name(part) : NULL;
        xsi_set(m, C, "cpu.midr", "Boot core", "kernel.resource (MIDR_EL1)",
                "%s%s part 0x%03x r%up%u", implementer == 0x61 ? "Apple " : "",
                name ? name : "core", part, variant, revision);
    } else {
        xsi_missing(m, C, "cpu.midr", "Boot core", XSI_UNKNOWN, "kernel.resource (MIDR_EL1)");
    }
    el = xsi_kernel_attr(APPLE_KATTR_CURRENT_EL);
    if (el != -1)
        xsi_set(m, C, "cpu.el", "Kernel runs at", "kernel.resource", "EL%ld", (long)el);
    freq = xsi_kernel_attr(APPLE_KATTR_COUNTER_FREQUENCY);
    if (freq > 0)
        xsi_set(m, C, "cpu.counter", "Counter frequency", "kernel.resource (CNTFRQ_EL0)",
                "%ld.%03ld MHz", (long)(freq / 1000000), (long)((freq / 1000) % 1000));
    else
        xsi_missing(m, C, "cpu.counter", "Counter frequency", XSI_UNKNOWN, "kernel.resource");

    present[0] = '\0';
    for (i = 0; apple_resources[i]; i++) {
        if (!xsi_has_resource(apple_resources[i]))
            continue;
        size_t n = strcspn(apple_resources[i], ".");
        if (used + n + 2 >= sizeof present)
            break;
        if (used) {
            present[used++] = ',';
            present[used++] = ' ';
        }
        memcpy(present + used, apple_resources[i], n);
        used += n;
        present[used] = '\0';
    }
    if (used)
        xsi_set(m, P, "drivers.loaded", "Driver resources", "exec ResourceList", "%s", present);
    else
        xsi_missing(m, P, "drivers.loaded", "Driver resources", XSI_UNKNOWN, "exec ResourceList");

    /* Optional capabilities with no public read interface yet. */
    xsi_missing(m, P, "sensor.temperature", "Temperature", XSI_UNAVAILABLE, "no application API");
    xsi_missing(m, P, "power.battery", "Battery", XSI_UNAVAILABLE, "no application API");
    return 1;
}
