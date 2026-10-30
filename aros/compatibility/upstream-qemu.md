<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 John Knipper -->

# Upstream Raspberry Pi QEMU compatibility experiment

This is a scoped compatibility observation, not qualification of physical
Raspberry Pi hardware or a performance reference.

## Inputs

- Application: the **unchanged** executable from the `pomaros-v0.1.0` release,
  SHA-256 `f4ceae8b710e3ae97ce2438705bea1822698a90aa04c8723c700f3905e8d2100`.
- Official upstream distribution:
  [AROS-20261030-raspi-aarch64-smp-system.tar.bz2](https://sourceforge.net/projects/aros/files/nightly2/20261030/Binaries/AROS-20261030-raspi-aarch64-smp-system.tar.bz2/download),
  SHA-256 `9a46217bb7aa678bd1fbcfb74680db2209a8551a8074d694d020efa1f516ade0`.
  The published MD5 also matched. The application reports Exec 51.11.
- QEMU 11.0.1, `raspi3b`, TCG, four Cortex-A53 CPUs, 1 GiB RAM.
- Upstream source inspected separately at
  [`83cb94f`](https://github.com/aros-development-team/AROS/tree/83cb94fabac3f8f46b915b9fefe8aa3708c43630).
  This does not establish the source commit used by the nightly builder.

No upstream or PomAROS OS code was modified. The test used independent disk
images and a separate source checkout, with no physical target access or use
of the native product's test tooling. An application and Shell test script were
added to a copy of the distribution; kernel and system binaries were unchanged.

## Observed result

The official system reached Wanderer before the application was introduced.
The released xSysInfo binary then completed:

| Scenario | Observation |
|---|---|
| `FULL CLI MS 100` | Three trials, one worker, `RESULT PASS`, return code 0 |
| `SMP MS 100` | Single baseline and three four-worker trials, CPU IDs 0–3, `RESULT PASS`, return code 0 |
| GUI open | Four Cortex-A53 CPUs shown; classic dashboard rendered at 1x |
| GUI close via emulated USB keyboard | Return code 0; Shell continued to the end marker |

The engine's successful result includes its Dhrystone integrity and worker
first/last CPU-ID checks. It does not independently prove scheduler behavior at
every point inside each trial. These short functional runs do not qualify
real Pi performance, all inventory views or every cancellation path.

## Small application-side identification fix

The release displays the generic provider because it recognizes `raspi-...`,
whereas upstream `KrnGetSystemAttr(KATTR_Architecture)` returns `aarch64-raspi`
(and `arm-raspi` on ARM32). The corrected provider accepts those exact upstream identifiers. It also reads
the board model from `/model` using the public `openfirmware.resource` API.
The original binary observations above predate these corrections. ARM32
identification support is not an ARM32 executable port.

## Reproduction outline

1. Verify and unpack the official distribution. Create a disposable 2 GiB SD
   image with an MBR FAT32 partition at sector 2048; copy distribution files
   without changing their contents. Keep an untouched baseline image.
2. Preserve `Devs/DOSDrivers/AUX` exactly. DOS-oriented FAT tools may reject that
   reserved filename: do not silently omit it. In this experiment it was copied
   using a temporary short name, its FAT directory name restored to `AUX`, and
   the resulting file read back and compared byte for byte.
3. Boot QEMU with the distribution's `aros-aarch64-raspi.img` as `-kernel`,
   `aros-aarch64-bsp.rom` as `-initrd`, and `bcm2710-rpi-3-b.dtb` as `-dtb`.
   Use `-machine raspi3b -accel tcg -smp 4 -m 1G` and the SD image via
   `-drive file=...,format=raw,if=sd`. Capture both serial ports; the useful boot
   and debug output on this configuration is on the second, mini-UART port.
4. After observing the baseline desktop, use another SD image copy. Add the
   release executable and an `S:User-Startup` test script; preserve any existing
   user startup when adapting this to another image. Set a 1 MiB Shell stack,
   execute the two CLI scenarios above, then open the GUI.
5. Redirect reports and explicit return-code markers to `DEBUG:`. In this
   experiment, attempted `SYS:` report files were not recovered, so that first
   run did not establish CLI results. The subsequent `DEBUG:` reports did.
6. Add `-device usb-kbd -device usb-mouse` for the UI run. Use an isolated QMP
   Unix socket to capture the display, inject `q`, observe the GUI exit marker,
   and stop QEMU. Retain serial logs, command line, image identities and verdict.

The task listing emitted through `DEBUG:` contained encoding corruption; it
was not used to assert absence of leftover worker processes. A complete gate
should add an unambiguous worker-exit observation, normal-duration/repeated
runs, cancellation and inventory/export checks. Emulated Dhrystone scores must
not be added as Raspberry Pi hardware reference values.

## Physical hardware reports

The user reported that the original release executable works on both a real
Raspberry Pi 500+ and a Pi 3B+, with `common` identification as the only reported
issue. The supplied Pi screenshot shows Exec 51.10, four Cortex-A76 CPUs,
a completed SMP result of about 30.3 million Dhrystones/s and balanced worker
contributions. Exact distribution/build identifiers and raw reports were not
supplied. This is user-reported compatibility evidence for the original
executable, rather than an automated hardware qualification of a refreshed build.

## Refreshed executable

The refreshed release executable has SHA-256
`7694a2487bc9c7d3568da361ff84935ad4b7787f989bb17d720f6a7efbc7a5d6`.
It uses an isolated snapshot of the original release SDK/runtime and the same
compiler and benchmark flags. No OS sources or installed SDKs were modified.

On the same official upstream distribution and QEMU configuration, it completed
`FULL CLI MS 100` and `SMP MS 100`, each with `RESULT PASS` and return code 0.
The full report selected provider `raspi` and read `Raspberry Pi 3 Model B`
from `openfirmware.resource` (`/model`). The GUI displayed `Pi 3 Model B`,
opened successfully, and returned 0 after the emulated `q` key; the Shell reached
its end marker. The executable read back from the SD image matched the build
hash. Host provider tests passed 66 checks under ASan/UBSan, including missing,
empty, oversized and unterminated Device Tree properties; the host engine suite
also passed. The refreshed executable has not been retested on physical M1 or
Pi hardware. Normal-duration measurements, cancellation and all inventory/export
paths are outside this scoped QEMU run.
