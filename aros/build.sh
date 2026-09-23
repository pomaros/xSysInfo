#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# SPDX-FileCopyrightText: 2026 John Knipper
#
# Cross-builds xSysInfo for one AROS AArch64 profile:
#
#   aros/build.sh apple     native Apple Silicon (private core SDK)
#   aros/build.sh macaros   hosted on macOS (darwin-aarch64 SDK)
#   aros/build.sh raspi     native Raspberry Pi (raspi-aarch64 SDK)
#
# Output: build/aros-<profile>/xSysInfo and build.json (flags, SDK, hashes).
# SDK roots can be overridden with XSI_SDK_<PROFILE>=<AROS build dir>.
# Each profile uses the program flags its own configure gives: the Apple
# core reserves x18 with the default code model; the upstream hosted and
# Pi builds use -mcmodel=large, and hosted Darwin reserves x18 as well.
# A successful build is not a runtime test.

set -eu
profile=${1:-}
# Upstream SDKs define each __aros_libreq_* marker in several link
# libraries (aros-pkg's build-aros.sh links the same way); the Apple core's
# do not, and there duplicates stay errors.
link_extra=""
here=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
crosstools=${AROS_CROSSTOOLS:-"$HOME/aros-crosstools"}
case "$profile" in
apple)
    build=${XSI_SDK_APPLE:-"$HOME/Build/aros-apple-core/apple"}
    sdk="$build/bin/apple-aarch64"; flags="-ffixed-x18" ;;
macaros)
    build=${XSI_SDK_MACAROS:-"$HOME/aros-build"}
    sdk="$build/bin/darwin-aarch64"; flags="-mcmodel=large -ffixed-x18"
    link_extra="-Wl,--allow-multiple-definition" ;;
raspi)
    build=${XSI_SDK_RASPI:-"$HOME/aros-build-850"}
    sdk="$build/bin/raspi-aarch64"; flags="-mcmodel=large"
    link_extra="-Wl,--allow-multiple-definition" ;;
*)
    echo "usage: aros/build.sh apple|macaros|raspi" >&2; exit 64 ;;
esac
tools="$build/bin/darwin-aarch64/tools"
developer="$sdk/AROS/Developer"
clang="$crosstools/bin/clang"
out="$here/build/aros-$profile"
for need in "$clang" "$developer/lib/startup.o" "$tools/collect-aros"; do
    [ -e "$need" ] || { echo "build.sh: $profile: missing $need" >&2; exit 69; }
done
mkdir -p "$out"

version=$(git -C "$here" describe --tags --always --dirty 2>/dev/null | sed -E 's/^release_//; s/^v//')
day=$(date -u +%-d.%-m.%Y)
# shellcheck disable=SC2086 -- $flags holds separate options.
COMPILER_PATH="$tools:$crosstools/bin" "$clang" --target=aarch64-unknown-aros $flags \
    -O2 -std=gnu11 -Wall -Wextra -Werror -Wno-pointer-sign \
    -DXSYSINFO_VERSION="\"$version\"" -DXSYSINFO_DATE="\"$day\"" \
    -isystem "$developer/include" -isystem "$sdk/gen/include" \
    -isystem "$developer/include/aros/stdc" \
    -nostartfiles -nodefaultlibs -L "$developer/lib" -L "$crosstools/lib/generic" \
    "$developer/lib/startup.o" \
    "$here/aros/main.c" "$here/aros/model.c" "$here/aros/report.c" "$here/aros/collect.c" \
    "$here/aros/provider_apple.c" "$here/aros/provider_hosted.c" "$here/aros/provider_raspi.c" \
    "$here/aros/gui.c" \
    -o "$out/xSysInfo" $link_extra \
    -Wl,--start-group -lstdc -ldos -lexec -lintuition -lgraphics -laros -lautoinit \
    -llibinit -lutility -lamiga -larossupport -Wl,--end-group -lclang_rt.builtins-aarch64
chmod 755 "$out/xSysInfo"
sha=$(shasum -a 256 "$out/xSysInfo" | cut -d' ' -f1)
cat >"$out/build.json" <<EOF
{
  "schema": 1,
  "profile": "$profile",
  "sdk": "$sdk",
  "flags": "$flags",
  "source": "$(git -C "$here" rev-parse HEAD)",
  "dirty": $(git -C "$here" status --porcelain | wc -l | tr -d ' '),
  "bytes": $(wc -c <"$out/xSysInfo" | tr -d ' '),
  "sha256": "$sha"
}
EOF
echo "build.sh: $out/xSysInfo"
