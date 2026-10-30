<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 John Knipper -->

# Adding a platform to xSysInfo

This guide covers new Apple Silicon models, Raspberry Pi boards and other
**AArch64 machines running PomAROS/AROS**. An application port does not provide
an OS port: the target must already boot AROS and expose the services below.
A machine with the same instruction set is not automatically runtime-qualified.

For build options and benchmark semantics, see the [application README](README.md).

## Choose the smallest change

| Target | Starting point |
|---|---|
| Another Apple M-series model | Try the common application first. The `apple-` provider already reads the OS board model. Extend optional identification only when the OS exposes more facts. |
| Raspberry Pi | Try the existing `raspi-` provider. Its recognition does not imply that the board model, sensors or SMP have been qualified. |
| Another native AArch64 board | Try the generic fallback first; add a provider only for additional board information. |
| AROS hosted on another OS | Check the hosted provider and the hosted ABI; distinguish guest resources from host hardware. |
| A different instruction set or a non-AROS OS | This needs a build/ABI port, and potentially a new OS adapter. A provider alone is insufficient. |

Do not create a separate benchmark or GUI for every machine. The common engine
must run the same workload and measurement protocol on each target.

## Where changes belong

| File | Role |
|---|---|
| [platform_aros.c](platform_aros.c) | Timer, CPU topology, affinity, worker creation and exit joining |
| [sysinfo.h](sysinfo.h) | `AAInfo`, `AACpu`, worker/run structures and platform function contract |
| [collect.c](collect.c) | Common inventory and provider selection |
| [collect.h](collect.h) | Provider declarations and OS query helpers |
| [model.h](model.h), [model.c](model.c) | Inventory fields, provenance and unavailable states |
| [provider_apple.c](provider_apple.c), [provider_raspi.c](provider_raspi.c), [provider_hosted.c](provider_hosted.c) | Optional platform-specific inventory |
| [benchmark.c](benchmark.c), [dhry_1.c](dhry_1.c), [dhry_2.c](dhry_2.c) | Shared measurement engine and reentrant workload |
| [build.py](build.py) | Explicit SDK/compiler selection and source list |
| [test/test_model.c](test/test_model.c) | Inventory/provider fixtures with simulated OS queries |
| [tests/engine.c](tests/engine.c) | Host worker lifecycle and benchmark correctness checks |

Hardware enablement belongs in the OS, not in the application. Providers must
not access MMIO, issue firmware mailbox calls or read privileged registers.
Expose needed information through a documented OS interface, then consume that
interface here. Do not copy private OS implementation sources into this public
application repository.

## Check the OS contract first

The AROS adapter uses these services:

- `processor.resource` / `GetCPUInfo()`: present count and per-CPU model,
  cluster, speed and cache information. Missing optional information is not a
  reason to invent a model, frequency or cache size.
- `kernel.resource`: `KrnGetCPUCount()` and `KrnGetCPUNumber()`; SMP also needs
  CPU mask allocation/selection and working `NP_Affinity` scheduling.
- `timer.device` / `ReadEClock()`: a nonzero frequency and a monotonic clock
  consistent across all participating CPUs. This timer frequency is not the
  CPU's operating frequency.
- Exec/DOS: `CreateNewProcTags()`, `NP_UserData`, `NP_NotifyOnDeath`, extended
  tasks, `ChildWait()` and `ChildFree()`. A completed workload is not proof that
  its process has exited; join before freeing worker state or unloading code.
- SMP inventory locking: `execlock.resource` protects shared Exec lists.
  `Forbid()` on one CPU is not a replacement for these locks.
- GUI: an Intuition public screen, graphics services and a bitmap Topaz font;
  detailed inventory views also use diskfont services.

**CPU numbering is an explicit constraint of the current adapter.** It treats
IDs `0 .. online-1` as online, with at most `AA_MAX_CPUS` (256) CPUs. Present and
online counts are separate. If the new OS has sparse online IDs or CPU hotplug,
first add a real online-ID mapping/snapshot contract across enumeration,
selection and validation. Do not pass physical IDs into an interface that
expects dense indices, or silently count offline CPUs as usable.

No core type is tied to a universal CPU number. The M1 reference used CPU 4 for
a Firestorm on the measured installation; another machine may number its cores
differently. Do not assume all Apple chips have eight cores or a 4+4 split.

Without working affinity, parallel execution must be refused. Checking only
that `kernel.resource` exists is not qualification of its scheduling behavior.
The engine checks requested versus observed CPU IDs at the start and end of
workers, but the OS must enforce affinity throughout each run.

## Add an inventory provider only if needed

Providers return zero when they do not recognize the running system. The first
successful provider wins. Selection in `xsi_collect()` currently tries Apple,
then hosted, then Raspberry Pi; otherwise it retains common inventory.

1. Obtain the actual `KATTR_Architecture` value from the target. Choose a precise
   match that does not steal hosted or existing platforms. Never identify a
   board solely from a peripheral address or the presence of a generic device.
2. Add `provider_<family>.c` with its licence header and declare
   `xsi_provider_<family>()` in `collect.h`.
3. Add a platform enum in `model.h` if a distinct family is needed. Extend the
   dispatch chain in `collect.c`, preserving the generic fallback.
4. Add the source to `sources` in `build.py` and `SRC` in `test/Makefile`.
   Providers are compiled explicitly; there is no automatic plugin discovery.
5. Add fixtures to `test/test_model.c` for recognition, rejection, absent
   attributes and correct provenance. Keep new SDK-only dependencies mockable
   through the existing shim/query pattern.

Minimal recognition pattern (the enum and architecture prefix below are
**placeholders to define for your target**, not existing OS API values):

```c
/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 John Knipper */
#include <string.h>
#include "collect.h"

int xsi_provider_example(xsi_model *m, const char *architecture)
{
    if (!architecture || strncmp(architecture, "example-", 8) != 0)
        return 0;

    m->platform = XSI_PLATFORM_EXAMPLE; /* add to model.h */
    m->platform_name = "example-native";
    xsi_set(m, XSI_SEC_PLATFORM, "platform.kind", "Platform",
            "kernel.resource", "Example family, native");
    xsi_missing(m, XSI_SEC_PLATFORM, "board.model", "Board",
                XSI_UNAVAILABLE, "no application API");
    return 1;
}
```

Read real board facts through documented OS queries. `xsi_kernel_attr()` returns
`-1` for an unanswered query; string-valued attributes also need a null check
before dereferencing. Use stable keys such as `board.model`, and identify the
source of every value. Keys, labels, source strings and `platform_name` must
outlive the collection call; use static strings, not local buffers for those
pointers. Formatted field values are copied into the model.

`xsi_set()` appends a field; it is not an update-by-key operation. Avoid duplicate
keys. Respect the model's fixed field/list capacities and its dropped counts.

| State | Use |
|---|---|
| `XSI_OK` | A value was read from its named source. |
| `XSI_UNKNOWN` | A query exists but did not supply an answer. |
| `XSI_UNAVAILABLE` | No usable application API exists. |
| `XSI_NOT_APPLICABLE` | The concept is not meaningful here, such as guest-owned battery hardware in hosted AROS. |

A loaded driver/resource is inventory, not evidence that its hardware function
works. Likewise, an unknown Apple MIDR does not prevent generic CPU inventory
or benchmarks; extend names only from verified hardware information.

## Build against the target's SDK

Run from the repository root, substituting paths from the target's own build:

```sh
python3 aros/build.py \
  --sdk /path/to/target-build/bin/target-aarch64 \
  --tools /path/to/matching-tools \
  --cc /path/to/aros-crosstools/bin/clang \
  --smp --out build/xSysInfo-target
```

`--sdk` must contain `AROS/Developer` and `gen/include`; `--tools` must contain
`collect-aros`. These are example paths, not downloadable SDK names. Use the
SDK headers, startup object and libraries belonging to the runtime under test.
`--smp` changes the Exec structure ABI: omit it for a UP SDK/runtime. For Darwin
hosted AROS, also use `--hosted` to reserve x18 and select its code model.

The output is `build/xSysInfo-target/xSysInfo` plus `build.json`, which records
the command, compiler, source hashes and executable hash. The script verifies a
little-endian AArch64 ELF. Changing only the SDK path does not enable ARM32,
x86-64 or big-endian targets: those require deliberate target, runtime-library,
ELF-validation and ABI changes. Keep benchmark optimization flags identical
when comparing scores; retain the original implementation in `src/` separately.

## Validate before calling the platform supported

Run the existing host checks after changing a provider or the OS adapter:

```sh
make -C aros/test
python3 aros/tests/run.py --out build/xSysInfo-tests
```

Host tests simulate CPU IDs. They validate logic, not the new hardware's timer,
affinity, scheduler or device behavior. Build success alone is also insufficient.

On the target, start with short runs and then the normal measurement duration:

```text
xSysInfo FULL CLI MS 100 >RAM:xsi-single-smoke.txt
xSysInfo SMP MS 100 >RAM:xsi-smp-smoke.txt
xSysInfo FULL SMP MS 1000 >RAM:xsi-smp.txt
xSysInfo CLI CPU 0 MS 1000 >RAM:xsi-cpu0.txt
```

The SMP commands require an SMP-capable runtime. Repeat the single-CPU command
with an observed CPU ID from each core class. For a subset, use `SMP CPU n
WORKERS count` (a contiguous range in the current CLI) or GUI selection.

Retain evidence for:

- Correct board identification, present/online counts and explicit missing
  fields; no accidental recognition by another provider.
- `RESULT PASS`, successful Dhrystone integrity checks, the expected worker
  count/CPU IDs and three trial rates. Test both SINGLE and SMP where supported.
- A consistent monotonic timer and OS-enforced affinity on the real target.
- Cancellation, closing during a run, repeated runs, and no surviving worker
  processes after exit. Partial launch failures are also covered by host tests.
- The fixed-size GUI on the target display: readable values, centered buttons,
  CPU selection/paging, memory/board/drive views and exported reports. Check
  paging on topologies with more than eight CPUs.

Record the source revision, exact executable hash, SDK/runtime revision,
compiler/options, machine/core configuration and execution environment
(native, hosted, VM or hypervisor). Keep a clear pass/fail result and preserve
failed attempts. A VM run is not native board qualification. State any remaining
unsupported services in the contribution rather than claiming family-wide
support from one board.

## Contribute a performance reference

A new platform does not need a built-in reference to run. First qualify it, then
submit a reference as a separate, reviewable measurement:

1. Keep the Dhrystone workload, batch size, optimization flags and three-trial
   protocol unchanged. Use 1000 ms per trial for comparison with the M1 record.
2. Record separate single-core and aggregate SMP results, naming the selected
   core type/IDs and worker count. Do not describe aggregate throughput as the
   speed of one core, or compare runs from different environments as equivalent.
3. Retain raw reports and a metadata file modeled on
   [references/m1.json](references/m1.json): all trial rates, median, rounded
   display value, workload and binary hashes, compiler/options, machine and
   environment. Record frequency policy, thermal/power conditions when known,
   and explicitly mark them unmeasured otherwise.
4. Do not overwrite the six historical Amiga constants or silently replace an
   existing measured reference. Changed workloads need versioned measurements;
   `tests/run.py` deliberately checks the M1 workload hashes.
5. If adding a built-in entry, update `aa_references` in `benchmark.c`,
   `AA_REFERENCE_COUNT` in `sysinfo.h`, reference checks in `tests/run.py`,
   documentation and GUI presentation together. The GUI currently treats the
   first six entries as Amiga bars and entries 6/7 as the two M1 ratios; adding
   another entry does **not** automatically add a GUI comparator. Refactor that
   explicit selection if more comparators are needed, preserving the fixed
   Amiga graph scale and the distinction between SINGLE and SMP.

A useful contribution includes the provider/adapter changes, host checks,
matching SDK build instructions, target evidence and any optional measurement
records. Preserve upstream notices and include the applicable licences with
redistributed binaries.
