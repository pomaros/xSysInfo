// SPDX-License-Identifier: BSD-2-Clause
// SPDX-FileCopyrightText: 2025 Stefan Reinauer

/*
 * xSysInfo - CPU cache control
 */

#include <exec/execbase.h>

#include <proto/exec.h>

#include "xsysinfo.h"
#include "cache.h"
#include "hardware.h"
#include "cpu.h"

/* External references */
extern struct ExecBase *SysBase;
extern HardwareInfo hw_info;

/*
 * Prefer Exec's generic CacheControl() API when it exists.
 *
 * On 68040/060 systems, Exec normalizes cache control to the old
 * CACRF_* abstraction, while Kickstart 1.3 has no CacheControl()
 * and still needs the raw CACR fallback.
 */
static BOOL use_exec_cachecontrol(void)
{
	return (SysBase->LibNode.lib_Version >= 36 &&
		hw_info.cpu_type != CPU_68080);
}

/*
 * Convert 68030-style CACR flags to 68040/060 format.
 *
 * The 68040 uses bit 31 (CACRF_CopyBack) for data cache enable and
 * bit 15 (CACRF_ICACHE040) for instruction cache enable, while the
 * 68030 uses CACRF_EnableD/CACRF_DBE and CACRF_EnableI/CACRF_IBE.
 */
ULONG convert68030to68040(ULONG input)
{
	ULONG output, keeper;

	if (hw_info.cpu_type < CPU_68040 || hw_info.cpu_type > CPU_68080)
		return input;

	output = 0;
	if (input & (CACRF_CopyBack | CACRF_EnableD | CACRF_DBE))
		output = CACRF_CopyBack;
	if (input & (CACRF_EnableI | CACRF_IBE))
		output |= CACRF_ICACHE040;

	/* Preserve all unrelated flags */
	keeper = input & ~(CACRF_CopyBack | CACRF_EnableD | CACRF_DBE |
			   CACRF_EnableI | CACRF_IBE | CACRF_ICACHE040);
	return output | keeper;
}

/*
 * Convert 68040/060 CACR bits back to 68030-style flags.
 *
 * This allows the toggle logic to test and manipulate cache state
 * using the 68030-style flag names uniformly.
 */
ULONG convert68040to68030(ULONG input)
{
	ULONG output, keeper;

	if (hw_info.cpu_type < CPU_68040 || hw_info.cpu_type > CPU_68080)
		return input;

	output = 0;
	if (input & CACRF_CopyBack)
		output = CACRF_CopyBack | CACRF_EnableD | CACRF_DBE;
	if (input & CACRF_ICACHE040)
		output |= CACRF_EnableI | CACRF_IBE;

	keeper = input & ~(CACRF_CopyBack | CACRF_EnableD | CACRF_DBE |
			   CACRF_EnableI | CACRF_IBE | CACRF_ICACHE040);
	return output | keeper;
}

/*
 * Expand a single 68030-style flag to the full group needed on 68040.
 *
 * The 68040 has no separate burst mode -- any data cache flag change
 * must set/clear all D-cache bits together, and likewise for I-cache.
 */
ULONG convertFlagsFor68040(ULONG input)
{
	ULONG output;

	if (hw_info.cpu_type < CPU_68040 || hw_info.cpu_type > CPU_68080)
		return input;

	output = 0;
	if (input & (CACRF_CopyBack | CACRF_EnableD | CACRF_DBE))
		output = CACRF_CopyBack | CACRF_EnableD | CACRF_DBE;
	if (input & (CACRF_EnableI | CACRF_IBE))
		output |= CACRF_EnableI | CACRF_IBE;

	return output;
}

/*
 * Toggle a cache flag, handling 68040/060 CACR bit layout differences.
 *
 * Read current CACR, convert to 68030-style, modify the requested flag
 * (expanding it for 68040 if needed), convert back and write.
 */
static void toggle_cache_flag(ULONG flag)
{
	if (use_exec_cachecontrol()) {
		ULONG current = CacheControl(0, 0);

		if (current & flag)
			CacheControl(0, flag);
		else
			CacheControl(flag, flag);
		return;
	}

	ULONG current = convert68040to68030(GetCacheBits());
	ULONG expanded = convertFlagsFor68040(flag);
	ULONG new_val;

	if (current & flag)
		new_val = current & ~expanded;
	else
		new_val = current | expanded;

	SetCacheBits(convert68030to68040(new_val), 0xFFFFFFFF);
}

/*
 * Toggle instruction cache
 */
void toggle_icache(void)
{
    if (cpu_has_icache()) {
        toggle_cache_flag(CACRF_EnableI);
    }
}

/*
 * Toggle data cache
 */
void toggle_dcache(void)
{
    if (cpu_has_dcache()) {
        toggle_cache_flag(CACRF_EnableD);
    }
}

/*
 * Toggle instruction burst
 */
void toggle_iburst(void)
{
    if (cpu_has_iburst()) {
        toggle_cache_flag(CACRF_IBE);
    }
}

/*
 * Toggle data burst
 */
void toggle_dburst(void)
{
    if (cpu_has_dburst()) {
        toggle_cache_flag(CACRF_DBE);
    }
}

/*
 * Toggle copyback mode
 */
void toggle_copyback(void)
{
    if (cpu_has_copyback()) {
        toggle_cache_flag(CACRF_CopyBack);
    }
}

/*
 * Toggle super-scalar mode
 */
void toggle_super_scalar(void)
{
    if (cpu_has_super_scalar()) {
        hw_info.super_scalar_enabled = set_super_scalar_mode(!(hw_info.super_scalar_enabled));
    }
}

/*
 * Check if CPU has instruction cache
 */
BOOL cpu_has_icache(void)
{
    return hw_info.has_icache;
}

/*
 * Check if CPU has data cache
 */
BOOL cpu_has_dcache(void)
{
    return hw_info.has_dcache;
}

/*
 * Check if CPU has instruction burst mode
 */
BOOL cpu_has_iburst(void)
{
    return hw_info.has_iburst;
}

/*
 * Check if CPU has data burst mode
 */
BOOL cpu_has_dburst(void)
{
    return hw_info.has_dburst;
}

/*
 * Check if CPU has copyback mode
 */
BOOL cpu_has_copyback(void)
{
    return hw_info.has_copyback;
}

/*
 * Check if CPU has super-scalar mode
 */
BOOL cpu_has_super_scalar(void)
{
    return hw_info.has_super_scalar;
}

BOOL cache_setting_enabled(CacheSetting setting)
{
    switch (setting) {
        case CACHE_ICACHE: return hw_info.icache_enabled;
        case CACHE_DCACHE: return hw_info.dcache_enabled;
        case CACHE_IBURST: return hw_info.iburst_enabled;
        case CACHE_DBURST: return hw_info.dburst_enabled;
        case CACHE_CBACK: return hw_info.copyback_enabled;
        case CACHE_SUPER_SCALAR: return hw_info.super_scalar_enabled;
        default: return FALSE;
    }
}

void toggle_cache_setting(CacheSetting setting)
{
    switch (setting) {
        case CACHE_ICACHE: toggle_icache(); break;
        case CACHE_DCACHE: toggle_dcache(); break;
        case CACHE_IBURST: toggle_iburst(); break;
        case CACHE_DBURST: toggle_dburst(); break;
        case CACHE_CBACK: toggle_copyback(); break;
        case CACHE_SUPER_SCALAR: toggle_super_scalar(); break;
        default: break;
    }
}
