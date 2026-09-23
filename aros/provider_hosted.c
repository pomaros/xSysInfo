// SPDX-License-Identifier: BSD-2-Clause
// SPDX-FileCopyrightText: 2026 John Knipper

/*
 * xSysInfo for AROS: hosted provider (Macaros and other hosted AROS).
 *
 * A hosted AROS is a guest: the host operating system owns the CPU,
 * memory map and devices. This provider says so, names the host family
 * from the kernel's architecture string, and marks board-level values as
 * not applicable instead of presenting host hardware as AROS drivers.
 * Host-supplied details (host OS version, host model) would come through
 * hostlib.resource; they are not queried in this build.
 */

#include <string.h>

#include "collect.h"

static const char *host_family(const char *architecture)
{
    if (!architecture)
        return NULL;
    if (strncmp(architecture, "darwin-", 7) == 0)
        return "macOS (Darwin)";
    if (strncmp(architecture, "linux-", 6) == 0)
        return "Linux";
    if (strncmp(architecture, "mingw32-", 8) == 0 || strncmp(architecture, "windows-", 8) == 0)
        return "Windows";
    return NULL;
}

int xsi_provider_hosted(xsi_model *m, const char *architecture)
{
    const int P = XSI_SEC_PLATFORM;
    const char *family = host_family(architecture);
    int hostlib = xsi_has_resource("hostlib.resource");

    if (!family && !hostlib)
        return 0;
    m->platform = XSI_PLATFORM_HOSTED;
    m->platform_name = "hosted";
    xsi_set(m, P, "platform.kind", "Platform", hostlib ? "hostlib.resource" : "kernel.resource",
            "hosted AROS, the host owns the hardware");
    if (family)
        xsi_set(m, P, "host.family", "Host", "kernel.resource", "%s", family);
    else
        xsi_missing(m, P, "host.family", "Host", XSI_UNKNOWN, "kernel.resource");
    xsi_missing(m, P, "host.version", "Host version", XSI_UNAVAILABLE,
                "host, not queried in this build");
    xsi_missing(m, P, "board.model", "Board", XSI_NOT_APPLICABLE, "host-managed");
    xsi_missing(m, P, "sensor.temperature", "Temperature", XSI_NOT_APPLICABLE, "host-managed");
    xsi_missing(m, P, "power.battery", "Battery", XSI_NOT_APPLICABLE, "host-managed");
    return 1;
}
