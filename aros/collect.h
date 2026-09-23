// SPDX-License-Identifier: BSD-2-Clause
// SPDX-FileCopyrightText: 2026 John Knipper

#ifndef XSI_COLLECT_H
#define XSI_COLLECT_H

#include "model.h"

/* Fills the model from AROS OS interfaces only: exec, dos, aros.library,
 * kernel.resource and processor.resource when present. Then asks each
 * platform provider whether it applies and lets the one that does add
 * its fields. Never touches hardware registers. */
void xsi_collect(xsi_model *m);

/* Providers (provider_*.c). Each returns nonzero when it recognised the
 * running system and added its fields; architecture is KATTR_Architecture,
 * or NULL when the kernel did not say. */
int xsi_provider_apple(xsi_model *m, const char *architecture);
int xsi_provider_hosted(xsi_model *m, const char *architecture);
int xsi_provider_raspi(xsi_model *m, const char *architecture);

/* Shared by the providers. */
extern void *xsi_KernelBase;
intptr_t xsi_kernel_attr(uint32_t id);    /* -1 when unanswered or no kernel.resource */
int xsi_has_resource(const char *name);

#endif
