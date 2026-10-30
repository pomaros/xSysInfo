// SPDX-License-Identifier: BSD-2-Clause
// SPDX-FileCopyrightText: 2026 John Knipper

/*
 * Host tests for the xSysInfo AROS data model, report and providers.
 * The providers run against a fake kernel attribute table, so the Apple,
 * hosted and Pi paths are exercised without an AROS system: values and
 * addresses above 4 GiB, unavailable fields, unknown attributes and the
 * provider choice. Build and run: make -C aros/test
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../collect.h"
#include "../model.h"
#include "../report.h"

static int failures, checks;

#define CHECK(cond, ...) do { checks++; if (!(cond)) { failures++; \
    printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* ---- fake kernel and resources --------------------------------------- */

typedef struct { uint32_t id; intptr_t value; } attr_t;
static const attr_t *attrs;
static const char *const *resources;
void *xsi_KernelBase = (void *)1;

intptr_t xsi_kernel_attr(uint32_t id)
{
    const attr_t *a;
    for (a = attrs; a && a->id; a++)
        if (a->id == id)
            return a->value;
    return -1;
}

int xsi_has_resource(const char *name)
{
    const char *const *r;
    for (r = resources; r && *r; r++)
        if (strcmp(*r, name) == 0)
            return 1;
    return 0;
}

/* ---- report capture --------------------------------------------------- */

static char report_buf[64 * 1024];

static void capture(void *ctx, const char *text)
{
    (void)ctx;
    strncat(report_buf, text, sizeof report_buf - strlen(report_buf) - 1);
}

static const char *report(const xsi_model *m, xsi_report_mode mode)
{
    report_buf[0] = '\0';
    xsi_report(m, mode, capture, NULL);
    return report_buf;
}

static const char *text_of(const xsi_model *m, const char *key)
{
    const xsi_field *f = xsi_find(m, key);
    return f ? xsi_field_text(f) : "(absent)";
}

/* ---- tests ------------------------------------------------------------ */

static void test_formatting(void)
{
    char b[40];
    CHECK(!strcmp(xsi_format_address(0x812345678ULL, 64, b, sizeof b), "0x00000008_12345678"), "%s", b);
    CHECK(!strcmp(xsi_format_address(0xffffffffffffffffULL, 64, b, sizeof b), "0xffffffff_ffffffff"), "%s", b);
    CHECK(!strcmp(xsi_format_address(0x812345678ULL, 32, b, sizeof b), "0x12345678"), "%s", b);
    CHECK(!strcmp(xsi_format_size(0, b, sizeof b), "0 B"), "%s", b);
    CHECK(!strcmp(xsi_format_size(1023, b, sizeof b), "1023 B"), "%s", b);
    CHECK(!strcmp(xsi_format_size(1536, b, sizeof b), "1.5 KB"), "%s", b);
    CHECK(!strcmp(xsi_format_size(16ULL << 30, b, sizeof b), "16.0 GB"), "%s", b);
    CHECK(!strcmp(xsi_format_size((5ULL << 32) + (1ULL << 31), b, sizeof b), "22.0 GB"), "%s", b);
    CHECK(!strcmp(xsi_format_size(300ULL << 30, b, sizeof b), "300 GB"), "%s", b);
    CHECK(!strcmp(xsi_format_size(UINT64_MAX, b, sizeof b), "15.9 EB"), "%s", b);
}

static void test_states_and_limits(void)
{
    static xsi_model m;
    int i;
    xsi_model_init(&m);
    xsi_missing(&m, XSI_SEC_CPU, "cpu.speed", "Clock", XSI_UNAVAILABLE, "processor.resource");
    xsi_missing(&m, XSI_SEC_CPU, "cpu.x", "X", XSI_OK, "src");  /* never shown as ok */
    CHECK(!strcmp(text_of(&m, "cpu.speed"), "[unavailable]"), "%s", text_of(&m, "cpu.speed"));
    CHECK(xsi_find(&m, "cpu.x")->state == XSI_UNKNOWN, "missing field cannot be ok");
    CHECK(xsi_find(&m, "cpu.speed")->value[0] == '\0', "no value for a missing field");
    for (i = 0; i < XSI_MAX_FIELDS + 3; i++)
        xsi_set(&m, XSI_SEC_DISPLAY, "k", "L", "s", "%d", i);
    CHECK(m.sections[XSI_SEC_DISPLAY].count == XSI_MAX_FIELDS, "cap");
    CHECK(m.sections[XSI_SEC_DISPLAY].dropped == 3, "dropped %u", m.sections[XSI_SEC_DISPLAY].dropped);
    CHECK(strstr(report(&m, XSI_REPORT_BRIEF), "[3 more fields not shown]") != NULL, "drop reported");
    for (i = 0; i < XSI_MAX_MODULES + 2; i++)
        xsi_add_module(&m, XSI_LIST_LIBRARIES);
    CHECK(m.lists[XSI_LIST_LIBRARIES].dropped == 2, "module drop");
    CHECK(strstr(report(&m, XSI_REPORT_BRIEF), "Libraries              258") != NULL, "count includes dropped");
}

static void test_report_high_memory(void)
{
    static xsi_model m;
    xsi_memregion *r;
    xsi_volume *v;
    xsi_module *x;
    const char *text;
    xsi_model_init(&m);
    m.pointer_bits = 64;
    r = xsi_add_region(&m);
    strcpy(r->name, "high memory");
    r->lower = 0x800000000ULL;
    r->upper = 0xbdf6c4000ULL;
    r->total = r->upper - r->lower;
    r->free = 0x2f0000000ULL;
    v = xsi_add_volume(&m);
    strcpy(v->device, "WORK:");
    strcpy(v->volume, "Work");
    strcpy(v->handler, "fat-handler");
    v->has_info = 1;
    v->block_size = 4096;
    v->total_blocks = 976562ULL;        /* ~3.7 GiB */
    v->used_blocks = 3ULL << 20;        /* 12 GiB used, over 32 bits in bytes */
    v->total_blocks = 5ULL << 20;       /* 20 GiB */
    v->state = XSI_VOL_OK;
    x = xsi_add_module(&m, XSI_LIST_RESOURCES);
    strcpy(x->name, "kernel.resource");
    x->address = 0x816348000ULL;
    text = report(&m, XSI_REPORT_FULL);
    CHECK(strstr(text, "0x00000008_00000000 - 0x0000000b_df6c4000") != NULL, "range:\n%s", text);
    CHECK(strstr(text, "size       15.4 GB, free 11.7 GB, largest [unknown]") != NULL, "sizes:\n%s", text);
    CHECK(strstr(text, "free chunks [unknown]") != NULL && strstr(text, "0 free chunks") == NULL,
          "an unwalked free list reports no chunk count:\n%s", text);
    CHECK(strstr(text, "12.0 GB used of 20.0 GB, 4096-byte blocks") != NULL, "volume:\n%s", text);
    /* A FAT32 volume of ~1.5 TB in 512-byte sectors: 3.0e9 blocks, which
     * InfoData32's LONG field holds as a negative number. */
    CHECK(xsi_block_count((int32_t)3000000000u) == 3000000000ULL, "unsigned block count");
    v->block_size = 512;
    v->total_blocks = xsi_block_count((int32_t)3000000000u);
    v->used_blocks = xsi_block_count((int32_t)2500000000u);
    text = report(&m, XSI_REPORT_FULL);
    CHECK(strstr(text, "1.1 TB used of 1.3 TB, 512-byte blocks") != NULL,
          "a volume past 2^31 blocks:\n%s", text);
    CHECK(strstr(text, "kernel.resource                  0x00000008_16348000  -\n") != NULL,
          "a resource is listed without a version:\n%s", text);
}

static const attr_t apple_attrs[] = {
    { 0x83f10003u, 0x611f0231 },               /* MIDR: Apple, part 0x023, r1p1 */
    { 0x83f10004u, 1 },
    { 0x83f10005u, 24000000 },
    { 0x83f10006u, (intptr_t)"Apple MacBook Air (M1, 2020)" },
    { 0x83f10007u, (intptr_t)"apple,j313" },
    { 0, 0 }
};
static const char *const apple_resources[] = { "pmgr.resource", "rtkit.resource", "smc.resource", NULL };

static void test_provider_apple(void)
{
    static xsi_model m;
    xsi_model_init(&m);
    attrs = apple_attrs;
    resources = apple_resources;
    CHECK(xsi_provider_hosted(&m, "apple-aarch64") == 0, "hosted must not claim Apple");
    CHECK(xsi_provider_raspi(&m, "apple-aarch64") == 0, "raspi must not claim Apple");
    CHECK(xsi_provider_apple(&m, "apple-aarch64") == 1, "apple claims apple-aarch64");
    CHECK(m.platform == XSI_PLATFORM_APPLE_NATIVE, "platform");
    CHECK(!strcmp(text_of(&m, "board.model"), "Apple MacBook Air (M1, 2020)"), "%s", text_of(&m, "board.model"));
    CHECK(!strcmp(text_of(&m, "cpu.midr"), "Apple Firestorm (M1 performance) part 0x023 r1p1"),
          "%s", text_of(&m, "cpu.midr"));
    CHECK(!strcmp(text_of(&m, "cpu.counter"), "24.000 MHz"), "%s", text_of(&m, "cpu.counter"));
    CHECK(!strcmp(text_of(&m, "drivers.loaded"), "pmgr, rtkit, smc"), "%s", text_of(&m, "drivers.loaded"));
    CHECK(!strcmp(text_of(&m, "sensor.temperature"), "[unavailable]"), "temperature not invented");
}

static void test_provider_apple_old_kernel(void)
{
    static xsi_model m;
    static const attr_t none[] = { { 0, 0 } };
    xsi_model_init(&m);
    attrs = none;
    resources = NULL;
    CHECK(xsi_provider_apple(&m, "apple-aarch64") == 1, "still Apple");
    CHECK(!strcmp(text_of(&m, "board.model"), "[unknown]"), "%s", text_of(&m, "board.model"));
    CHECK(!strcmp(text_of(&m, "cpu.midr"), "[unknown]"), "%s", text_of(&m, "cpu.midr"));
    CHECK(!strcmp(text_of(&m, "drivers.loaded"), "none"), "an empty list is an answer: %s",
          text_of(&m, "drivers.loaded"));
}

static void test_provider_hosted(void)
{
    static xsi_model m;
    static const char *const hosted[] = { "hostlib.resource", NULL };
    xsi_model_init(&m);
    attrs = NULL;
    resources = hosted;
    CHECK(xsi_provider_apple(&m, "darwin-aarch64") == 0, "apple must not claim darwin");
    CHECK(xsi_provider_hosted(&m, "darwin-aarch64") == 1, "hosted claims darwin");
    CHECK(!strcmp(text_of(&m, "host.family"), "macOS (Darwin)"), "%s", text_of(&m, "host.family"));
    CHECK(!strcmp(text_of(&m, "board.model"), "[n/a]"), "host hardware is not AROS hardware");
    CHECK(!strcmp(text_of(&m, "host.version"), "[unavailable]"), "not invented");
    /* hostlib alone, unknown architecture string */
    xsi_model_init(&m);
    CHECK(xsi_provider_hosted(&m, NULL) == 1, "hostlib alone means hosted");
    CHECK(!strcmp(text_of(&m, "host.family"), "[unknown]"), "%s", text_of(&m, "host.family"));
}

static void test_provider_raspi(void)
{
    static xsi_model m;
    static const attr_t pi[] = { { 0x83f00001u, (intptr_t)0xfe000000 }, { 0, 0 } };
    xsi_model_init(&m);
    attrs = pi;
    resources = NULL;
    CHECK(xsi_provider_hosted(&m, "raspi-aarch64") == 0, "hosted must not claim a Pi");
    CHECK(xsi_provider_raspi(&m, "raspi-aarch64") == 1, "raspi claims raspi-aarch64");
    CHECK(!strcmp(text_of(&m, "soc.peripherals"), m.pointer_bits > 32 ? "0x00000000_fe000000" : "0xfe000000"),
          "%s", text_of(&m, "soc.peripherals"));
    CHECK(!strcmp(text_of(&m, "board.model"), "[unavailable]"), "board not guessed");
    xsi_model_init(&m);
    CHECK(xsi_provider_raspi(&m, "aarch64-raspi") == 1, "recognise upstream AArch64 kernel order");
    CHECK(m.platform == XSI_PLATFORM_RASPI, "upstream selects Pi provider");
    CHECK(!strcmp(text_of(&m, "board.model"), "[unavailable]"), "upstream board also not guessed");
    xsi_model_init(&m);
    CHECK(xsi_provider_raspi(&m, "arm-raspi") == 1, "recognise upstream ARM kernel order");
    xsi_model_init(&m);
    CHECK(xsi_provider_raspi(&m, NULL) == 0, "missing architecture is not a Pi");
    CHECK(xsi_provider_raspi(&m, "aarch64-raspi-hosted") == 0, "do not infer native board from ambiguous suffix");
    CHECK(m.sections[XSI_SEC_PLATFORM].count == 0, "rejected identifiers do not add fields");
}

static void test_no_provider(void)
{
    static xsi_model m;
    xsi_model_init(&m);
    attrs = NULL;
    resources = NULL;
    CHECK(!xsi_provider_apple(&m, "pc-x86_64") && !xsi_provider_hosted(&m, "pc-x86_64")
          && !xsi_provider_raspi(&m, "pc-x86_64"), "nobody claims an unknown native system");
    CHECK(strstr(report(&m, XSI_REPORT_BRIEF), "provider common") != NULL, "common report still works");
}

int main(void)
{
    test_formatting();
    test_states_and_limits();
    test_report_high_memory();
    test_provider_apple();
    test_provider_apple_old_kernel();
    test_provider_hosted();
    test_provider_raspi();
    test_no_provider();
    printf("xsysinfo-aros-host-tests result=%s checks=%d failed=%d\n",
           failures ? "FAIL" : "PASS", checks, failures);
    return failures ? 1 : 0;
}
