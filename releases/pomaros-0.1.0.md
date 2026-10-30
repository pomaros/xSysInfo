# xSysInfo for PomAROS 0.1.0 — M1 preview

First public PomAROS application prerelease, based on Stefan Reinauer's
[xSysInfo](https://github.com/reinauer/xSysInfo). Thank you to Stefan and the
upstream contributors, and to Reinhold P. Weicker for Dhrystone.

## Download and run

`xSysInfo-0.1.0-pomaros-native-aarch64-smp.zip` contains the native application,
licence notices, build metadata and instructions. Check the archive against
`SHA256SUMS`, extract it, and copy the application directory to your PomAROS
volume. From an AROS Shell in that directory:

```text
protect xSysInfo +e
xSysInfo
xSysInfo CLI
xSysInfo FULL SMP >RAM:xSysInfo.txt
```

The GUI offers SINGLE/SMP, CPU selection, SPEED/CANCEL, inventory views and
report export. PRINT writes `RAM:xSysInfo.txt`. The application does not install
an OS or drivers. Source archives are available with this release; build and
extension instructions are in [the porting guide](https://github.com/pomaros/xSysInfo/blob/pomaros-v0.1.0/aros/PORTING.md).

## Included

- Native C AArch64 system inventory and reentrant Dhrystone 2.1.
- Single-core and SMP measurements with independent worker state, a shared
  timing window, three trials and the median aggregate result.
- Classic bitmap logo, dithered panels, centered buttons and a fixed-size window.
- Six preserved Amiga reference scores on a fixed graph scale. Overflow is
  marked `+`; the M1 single-core/SMP ratios appear separately.
- Measured M1 reference reports and protocol documentation in the source tree.

## Compatibility and qualification

The downloadable binary is **PomAROS native AArch64, SMP ABI**, validated on a
MacBook Air M1 (2020), Exec 51.12, using the matching PomAROS SDK with common-core
revision `e009c0b4c5466ceefe13ef15b94e0e9444166716`. That is the tested build,
not a promise of compatibility with every Exec 51.12 installation. It is not a
universal ARM64 executable or a macOS/Linux application.

| Running system | Status for this binary |
|---|---|
| Matching PomAROS native M1 runtime | Native SINGLE/SMP, inventory, GUI and export exercised |
| Another PomAROS Apple model | Not runtime-qualified |
| AROS upstream `raspi-aarch64-smp` | Not binary-qualified; build against that runtime's SDK and validate |
| AROS upstream `raspi-aarch64` (UP) | Requires a matching UP build; do not use this SMP-ABI binary |
| AROS upstream `raspi-armhf` | ARM32: requires an instruction-set/build port, not this AArch64 binary |
| Raspberry Pi OS / Linux, macOS | Different OS ABI; cannot run this native AROS application directly |

The Raspberry Pi provider reads OS information; it does not select an ABI,
load missing OS services or translate binaries. Source compatibility with an
upstream SDK must also be checked. Optional board information may be unavailable,
but incompatible task layouts or missing required services can cause failure or
a crash. There is no comprehensive runtime ABI guard in this prerelease.

The intended distribution model is one shared application source and separately
qualified builds for each OS/ABI combination, not one code fork per board. A
shared binary across compatible AArch64 boards is possible only after proving
that their SDK/runtime contracts match. See the porting guide for timer,
affinity, worker lifecycle and inventory-locking requirements.

## Validation and limitations

Host inventory and engine checks passed under ASan/UBSan. The application was
built with native SMP and VM UP SDKs; this release ships only the native M1
binary. Native evidence includes eight-worker SMP reports, direct display
captures and exact executable/source binding. The full VM desktop acceptance
gate was not qualified; no VM or Raspberry Pi binary is included.

Frequency and thermal conditions of the saved M1 references were not
independently measured; those values describe observed PomAROS performance,
not the M1's theoretical maximum. Memory bandwidth, floating-point and disk
throughput benchmarks are not implemented. CPU numbering currently assumes
contiguous online IDs and a maximum of 256 CPUs.

The ZIP includes component notices. SDK libraries and runtime helpers retain
their own licences; the application does not relicense them. Build metadata
identifies the common-core source and the shipped executable's SHA-256.
