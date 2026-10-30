#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (C) 2026 John KNIPPER.
"""Build an application against an explicitly selected AROS SDK, without changing it."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

p = argparse.ArgumentParser()
p.add_argument('--sdk', type=Path, required=True, help='bin/apple-aarch64 (or other AArch64 target)')
p.add_argument('--tools', type=Path, required=True, help='directory containing collect-aros')
p.add_argument('--cc', type=Path, required=True, help='AROS clang')
p.add_argument('--out', type=Path, required=True)
p.add_argument('--hosted', action='store_true', help='reserve x18 for Darwin hosted ABI')
p.add_argument('--smp', action='store_true', help='use an SMP SDK/runtime (changes Exec structure ABI)')
a = p.parse_args()
root = Path(__file__).resolve().parent
sdk = a.sdk.resolve()
dev = sdk/'AROS/Developer'
cc = a.cc.resolve()
a.out.mkdir(parents=True, exist_ok=True)
sources = ['main.c', 'benchmark.c', 'dhry_1.c', 'dhry_2.c', 'platform_aros.c', 'gui.c',
           'inventory_gui.c', 'collect.c', 'model.c', 'report.c',
           'provider_apple.c', 'provider_hosted.c', 'provider_raspi.c']
flags = ['--target=aarch64-unknown-aros', '-O2', '-fno-builtin', '-fno-lto', '-fno-inline',
         '-std=gnu11', '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter', '-Wno-pointer-sign']
if a.hosted:
    flags += ['-ffixed-x18', '-mcmodel=large']
if a.smp:
    flags += ['-D__AROSEXEC_SMP__']
env = dict(os.environ, COMPILER_PATH=f'{a.tools.resolve()}:{cc.parent}')
command = [str(cc), *flags, '-isystem', str(dev/'include'), '-isystem', str(sdk/'gen/include'),
           '-isystem', str(dev/'include/aros/stdc'), '-isystem', str(dev/'include/aros/posixc'),
           '-nostartfiles', '-nodefaultlibs', '-L', str(dev/'lib'), '-L', str(cc.parent.parent/'lib/generic'),
           str(dev/'lib/startup.o'), *[str(root/s) for s in sources], '-o', str(a.out/'xSysInfo'),
           '-Wl,--start-group', '-lstdc', '-lstdcio', '-ldos', '-lexec', '-lintuition', '-ldiskfont', '-lgraphics', '-laros',
           '-lautoinit', '-llibinit', '-lutility', '-lamiga', '-larossupport', '-Wl,--end-group',
           '-lclang_rt.builtins-aarch64']
subprocess.run(command, env=env, check=True)
binary = (a.out/'xSysInfo').read_bytes()
if binary[:4] != b'\x7fELF' or binary[4:6] != b'\x02\x01' or int.from_bytes(binary[18:20], 'little') != 183:
    raise SystemExit('Not a little-endian AArch64 ELF')
sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
(a.out/'build.json').write_text(json.dumps(dict(command=command, sdk=str(sdk),
    compiler=subprocess.check_output([str(cc), '--version'], text=True),
    sources={s:sha(root/s) for s in sources+['sysinfo.h', 'dhry.h']},
    binary_sha256=sha(a.out/'xSysInfo'), startup_sha256=sha(dev/'lib/startup.o')),
    indent=2)+'\n')
print(a.out/'xSysInfo')
