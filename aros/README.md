# xSysInfo for AROS AArch64

One AROS application, built from this tree for three AArch64 systems: native
Apple Silicon, Macaros (AROS hosted on macOS) and native Raspberry Pi. It keeps
xSysInfo's window, palettes, panels and report modes. It replaces the Amiga
hardware probes with AROS OS interfaces and optional platform providers.

The upstream m68k build (`Makefile`, `src/`) is unchanged. Nothing in `aros/`
is compiled into it, and nothing from `src/` except the adapted drawing
primitives is compiled into this build.

## Layout

| File | Role |
|---|---|
| `model.h`, `model.c` | The data model. Plain C99 with 64-bit addresses and sizes. Every field carries a state (`ok`, `unavailable`, `unknown`, `not applicable`) and the source it was read from. |
| `report.c` | The BRIEF and FULL text reports, generated from the model. |
| `collect.c` | Common collectors: exec, dos, aros.library, kernel.resource, and processor.resource when present. |
| `provider_apple.c` | Apple native: board, MIDR, exception level and counter frequency recorded by the Apple kernel, and the Apple driver resources that are loaded. |
| `provider_hosted.c` | Hosted AROS: marks the host as the owner of the hardware; host details are not queried. |
| `provider_raspi.c` | Raspberry Pi: the SoC peripheral base. Board model and revision are unavailable. |
| `gui.c` | The window, drawn from the model. The primitives are adapted from `src/gui.c`. |
| `main.c` | Arguments, report or GUI. |
| `build.sh` | Cross-build for one profile. |
| `test/` | Host tests of the model, the report and all three providers. |

## Usage

```
xSysInfo                  window on the default public screen
xSysInfo BRIEF            short report to the Shell, no window
xSysInfo FULL             complete report (sources, lists, regions, volumes)
xSysInfo FULL TO RAM:x    the same, to a file
xSysInfo DARK             dark palette
```

When both are given, FULL wins over BRIEF, as upstream. `WHICH` and `SCSI` read
Amiga hardware; they are refused with return code 5. In the window, *Print*
writes the FULL report to `RAM:xsysinfo.txt`. *Memory* pages through the
MemHeaders and *Drives* lists the running filesystem handlers. `q`, `Esc` and
the close gadget quit.

## What each value means

- **Loaded driver resources.** A resource that is present means its driver was
  initialised. It does not mean the function was tested.
- **Hosted.** Macaros is a guest: the board, temperature and battery fields are
  `not applicable`. Host hardware is never shown as an AROS driver. The host
  family comes from the kernel's architecture string. The host OS version would
  need hostlib and is reported `unavailable`.
- **Memory headers.** Headers are copied under `Forbid()`; their free lists are
  never walked. AROS may manage a header with its own allocator, and SMP
  kernels guard free lists with spinlocks. The largest free block per region is
  therefore `unknown`, while the system-wide largest block comes from
  `AvailMem(MEMF_LARGEST)`.
- **Volumes.** Only devices whose handler is already running are asked, so
  reading the list starts no handler. Requesters are suppressed during the
  query. Sizes are computed in 64 bits.
- **Processor.** processor.resource is optional. Where it is absent, which
  includes the Apple native bundle, the processor fields are `unavailable`.
  On Apple, the provider adds the boot core's identity.
- **Benchmarks.** Upstream's scores are calibrated against m68k machines, so
  this build reports no benchmark at all. Benchmarks are a later slice.

## Build

```sh
aros/build.sh apple      # ~/Build/aros-apple-core/apple      (XSI_SDK_APPLE)
aros/build.sh macaros    # ~/aros-build, darwin-aarch64        (XSI_SDK_MACAROS)
aros/build.sh raspi      # ~/aros-build-850, raspi-aarch64     (XSI_SDK_RASPI)
make -C aros/test        # host tests, ASan + UBSan
```

The toolchain is `~/aros-crosstools` clang with each SDK's `collect-aros`.
Output goes to `build/aros-<profile>/xSysInfo`, with a `build.json` holding the
flags, source revision and SHA-256.

| Profile | SDK target | Program flags | Link |
|---|---|---|---|
| apple | `apple-aarch64`, private core | `-ffixed-x18` (the core's `aarch64-apple` flags) | strict |
| macaros | `darwin-aarch64`, upstream | `-mcmodel=large -ffixed-x18` | `--allow-multiple-definition`, as aros-pkg: the upstream link libraries each define the `__aros_libreq_*` markers |
| raspi | `raspi-aarch64`, upstream | `-mcmodel=large` | as macaros |

The Apple SDK has processor-topology and framebuffer tags that the upstream
SDKs lack. The code guards them with `#ifdef`, so the three builds differ only
by what their headers declare.

## Runtime dependencies

exec, dos, stdc, intuition and graphics are opened automatically. aros.library
is opened by the program itself and is optional; task.resource is used by the
startup code. The program uses kernel.resource and processor.resource when they
are present and hostlib.resource only to test whether it exists. It needs no
identify.library, openpci.library, MMU library or m68k code.

## Platform matrix

Build, launch, GUI and report are recorded separately. A build is never
counted as a runtime result.

| Profile | Build | Launch | Report | GUI | Evidence |
|---|---|---|---|---|---|
| apple | PASS | not run | not run | not run | on-target run pending, operated by Codex |
| macaros | PASS | PASS | PASS: BRIEF, FULL TO file, WHICH refused with rc 5 | PASS: cycles, Memory, Drives, Print, Quit, DARK | `aros-ctl` run on the M5, integration repo `artifacts/runs/20260923-xsysinfo-macaros/` |
| raspi | PASS | not run | not run | not run | — |
| host tests | PASS, 48 checks | — | — | — | `make -C aros/test` |

## Licences

New files are BSD-2-Clause, © 2026 John Knipper, matching the project.
`aros/gui.c` keeps Stefan Reinauer's copyright line for the adapted primitives
and palettes. The build links only the AROS SDK link libraries. None of
upstream's third-party parts is used: identify, flexcat, TinySetPatch,
MMULib, openpci and the Dhrystone sources. The Apple core part names are
facts taken from public documentation; no code was copied.

## AROS findings

- `aros/inquire.h` documents the `AI_Aros*` results as ULONG, but
  `ArosInquireA()` stores an IPTR for each of them. On a 64-bit AROS, a ULONG
  destination is overwritten by eight bytes. This build passes IPTR.
- processor.resource on hosted AArch64 answers the model string `Unknown`
  and no clock speed.

## OS interfaces this build lacks

These values are unavailable because no interface exists for applications, not
because of this program. Each would be a small, separately scoped OS change:

- **Apple.** Temperatures and battery. smc.resource has key reads, but
  application use needs a read-only, allow-listed sensor interface.
  Per-cluster core topology. Whether each driver passed its own self-test,
  as distinct from being loaded.
- **Pi.** Board model, revision and memory split from the firmware mailbox
  (a kernel attribute, like Apple's board model). SoC temperature.
- **Hosted.** Host OS name and version, as a hostlib-backed kernel
  attribute. This avoids having each application call host functions itself.
- **All.** processor.resource on the Apple native bundle, or an equivalent
  core count and model. A per-MemHeader "largest free block" that is safe
  under SMP.
