// SPDX-License-Identifier: MIT
// Copyright (c) 2026 John Knipper
#include <stdint.h>
void *xsi_test_openkey(void *base, const char *path);
#define OF_OpenKey(path) xsi_test_openkey(OpenFirmwareBase, path)
void *OF_FindProperty(void *node, const char *name);
uint32_t OF_GetPropLen(void *prop);
void *OF_GetPropValue(void *prop);
void OF_CloseKey(void *node);
