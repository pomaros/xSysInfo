<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 John Knipper -->

# xSysInfo for PomAROS AArch64

xSysInfo is the native C system-information and Dhrystone application in
`aros/`. The dashboard follows the upstream classic layout: its original bitmap
logo, blue/gray/red dithered banner, software panel above the comparison table,
tall hardware panel and six action buttons in a three-column grid. Bitmap text
uses the tall Amiga pixel aspect, with black labels and white values. Button
text is centered using the glyph ink bounds in both axes.

The window has a fixed size and no resize gadget. It selects 1x or 2x at opening
according to the screen size, preserving all geometry. The detailed inventory
views use 2x on sufficiently large screens. The original m68k program in `src/`
is a separate upstream implementation.

## Use

```
xSysInfo                         graphical interface
xSysInfo CLI                     single CPU benchmark
xSysInfo SMP                     all online CPUs, plus a single CPU baseline
xSysInfo SMP CPU 4 WORKERS 4      four consecutive online CPUs starting at 4
xSysInfo FULL SMP                inventory and benchmark report
xSysInfo CLI CPU 4 MS 1000       select CPU and trial duration
```

`BRIEF` is an alias for `CLI`. Shell redirection saves a command-line report.
CPU IDs come from PomAROS; the example above does not define a universal
performance-core numbering convention. `MS` accepts 100 through 10000, defaults
to 1000, and applies to each of three trials. The build handles up to 256 CPUs;
a larger topology is explicitly refused rather than silently truncated.

The main window offers SINGLE/SMP, selectable CPUs, paged CPU information,
SPEED/CANCEL, MEMORY, BOARDS, DRIVES, PRINT and QUIT. SINGLE uses the first
selected CPU. SMP measures a single baseline on that CPU before measuring all
selected CPUs. PRINT writes `RAM:xSysInfo.txt`, including inventory and results.
MEMORY and DRIVES open their detailed inventory windows; BOARDS opens the
library/device/resource and platform browser. Its Print button saves the
inventory alone to `RAM:xSysInfo-inventory.txt`.

Keyboard shortcuts: `s` starts/cancels, `m` switches SINGLE/SMP, `p` exports,
`e` opens memory, `b` opens the inventory browser, `d` opens drives, and `q`/Esc
closes the active window. Closing the benchmark window cancels and reaps its
workers before releasing their state. Ctrl-C also cancels CLI measurements.

## Measurement contract

The kernel is Dhrystone C 2.1 adapted from xSysInfo revision
`0ee81383380251df0fc67f78c0282530a7dcff84`. Each worker has independent records,
arrays and scalar state, passed explicitly to the benchmark functions. This
reentrant variant adds context-argument overhead; historical ratios are
synthetic benchmark comparisons, not claims about whole-machine application
performance. The build uses `-O2 -fno-builtin -fno-lto -fno-inline`; compiler,
source hashes, SDK and flags are recorded in `build.json`.

Each worker warms up and verifies Dhrystone's expected final values. Once all
workers are ready, the parent publishes a common future start and deadline.
Workers execute complete 20000-iteration batches, resetting the accumulating
array element between batches to avoid integer overflow. The final batch can
cross the deadline: **all workers use the same denominator**, from the common
start to the latest completion. Scheduling delay and synchronization overhead
therefore reduce the reported throughput rather than inflating it. Results
require final integrity and first/last CPU-ID agreement with requested affinity.
All children are joined before the next trial or program exit.

Three trials are run; the median aggregate trial supplies both the total and
its corresponding per-worker rates. DMIPS means Dhrystones/s divided by 1757.
SMP reports cumulative throughput, not the speed of one core. Speedup names
its baseline CPU: heterogeneous systems can exceed the worker count when
compared with an efficiency core. No frequency, temperature or throttling
condition is inferred from the score.

Historical Amiga references are immutable data copied from xSysInfo's table:

| Machine | Dhrystones/s |
|---|---:|
| A600 68000 7 MHz | 1001 |
| B2000 68000 7 MHz | 1408 |
| A1200 EC020 14 MHz | 2550 |
| A3000 68030 25 MHz | 8300 |
| A4000 68040 25 MHz | 32809 |
| A4000 68060 50 MHz | 91000 |

Upstream describes these as placeholder values to calibrate. xSysInfo preserves
them, their provenance and the raw measured rate; it does not recalibrate them.

The graphical comparison uses only the six Amiga bars on a fixed 0–40,000
Dhrystones/s scale. Bars above that limit are capped and marked with `+`; the
measured score and numerical ratios remain available. The measured score never
changes the scale. M1 single-core and SMP comparisons appear as ratios in a
separate strip below the graph; they do not contribute bars or affect its scale.
Reports retain all eight reference values.

Two measured M1 references are retained without replacing the Amiga entries:

| Machine / mode | Dhrystones/s (rounded for display) |
|---|---:|
| MacBook Air M1, one Firestorm core | 18,242,659 |
| MacBook Air M1, eight cores SMP | 127,077,293 |

The [measurement record](references/m1.json) contains all three trial rates,
compiler/options, workload hashes and limitations. The corresponding
[single-core report](references/single_firestorm.txt) and
[SMP report](references/smp_eight.txt) are retained. These are observed native
PomAROS results with OS-managed clocks, not theoretical M1 specifications.
The SMP entry is explicitly aggregate throughput and must not be read as a
single-core reference. Ordinary benchmark runs never overwrite the reference data.

## Architecture and platform extensions

| Component | Responsibility |
|---|---|
| `benchmark.c`, `dhry_1.c`, `dhry_2.c` | Reentrant workload, validation, trial coordination and comparison table |
| `platform_aros.c` | EClock time, processor inventory, online CPU count, affinity, process creation and exit joining |
| `model.c`, `collect.c`, `report.c` | OS inventory, explicit unavailable states and reports |
| `provider_apple.c`, `provider_hosted.c`, `provider_raspi.c` | Optional OS-reported platform facts; no direct hardware access |
| `gui.c`, `inventory_gui.c` | Benchmark dashboard and detailed inventory views |
| `tests/engine.c`, `test/` | Engine lifecycle/integrity and inventory/provider host tests |

New platforms implement their processor/topology/timer support in PomAROS and,
when additional platform facts are exposed, add a provider through `collect.h`.
The application does not hardcode M1 core counts or privileged registers. An
unknown platform retains generic CPU information and benchmarks when its OS
provides the required services. Missing affinity refuses parallel execution.
Present processors and online processors are separate fields. SMP collectors
use `execlock.resource` read locks for shared Exec lists, instead of relying
on `Forbid()` on a single CPU. Per-region free memory is an instantaneous
snapshot, not a transactional global sum; allocator free lists are not walked.

The inventory describes memory, filesystems, loaded drivers and software.
Loaded resources do not prove hardware qualification. Memory bandwidth, floating
point and disk throughput benchmarks are not implemented; no fabricated scores
are displayed. The comparison table is specifically the Dhrystone CPU test.

## Build

Select the exact SDK and the matching `collect-aros` tools explicitly:

```sh
python3 aros/build.py \
  --sdk /path/to/build/bin/apple-aarch64 \
  --tools /path/to/build/bin/darwin-aarch64/tools \
  --cc /path/to/aros-crosstools/bin/clang \
  --smp --out build/xSysInfo-native
```

Use `--smp` only for an SMP SDK/runtime: it changes Exec structure layout.
Omit it for a UP guest SDK. `--hosted` reserves x18 for the Darwin hosted ABI.
`aros/build.sh` forwards the same arguments. The script verifies that its output
is a little-endian AArch64 ELF and records build inputs; it does not build an
OS, publish source or change a product pin. Include the notices beside any
redistributed executable. Raspberry Pi and additional Apple models require
their own SDK/runtime qualification; an M1 test does not qualify those machines.

## Verification

```
make -C aros/test
python3 aros/tests/run.py --out build/xSysInfo-tests
```

Host engine tests exercise independent Dhrystone state, SINGLE/SMP coordination,
shared accounting, cancellation, each partial-worker-creation failure,
affinity rejection and input bounds under ASan/UBSan. The host adapter simulates
logical CPU IDs; it is not a hardware-affinity or performance test.

Hardware qualification is specific to the tested executable and platform.
The M1 reference reports above retain the measured scores and trial results.
Other Apple models and Raspberry Pi require their own runtime validation.

## Licences

New fork application/engine files use [MIT](LICENSE.MIT), Copyright (c) 2026 John Knipper.
The existing inventory files retain BSD-2-Clause and their original notices.
The classic logo bitmap retains upstream BSD-2-Clause attribution.
The reentrant Dhrystone files retain Reinhold P. Weicker's attribution and the
xSysInfo BSD-2-Clause grant in `LICENSE.xSysInfo`; upstream code is not relicensed.
The benchmark's provenance and adaptations are described above.
