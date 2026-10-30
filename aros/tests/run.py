#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 John Knipper
import argparse,json,subprocess,hashlib,re
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);p.add_argument('--cc',default='cc');a=p.parse_args()
a.out.mkdir(parents=True,exist_ok=True);root=Path(__file__).resolve().parent.parent
sources=[root/'tests/engine.c',root/'benchmark.c',root/'dhry_1.c',root/'dhry_2.c']
references=json.loads((root/'references/m1.json').read_text())
benchmark=(root/'benchmark.c').read_text()
for key,label in [('single_firestorm','M1 Firestorm 1 core'),('smp_eight','M1 8 cores SMP')]:
    measurement=references['measurements'][key]
    assert measurement['dhrystones_per_second']==sorted(measurement['trial_rates'])[1]
    assert '{"'+label+'",'+str(measurement['reference_integer'])+'}' in benchmark
    assert measurement['reference_integer']==round(measurement['dhrystones_per_second'])
for name,digest in references['workload_sha256'].items():
    assert hashlib.sha256((root/name).read_bytes()).hexdigest()==digest, 'workload changed: refresh/version the M1 references'
cmd=[a.cc,'-O2','-fno-builtin','-fno-lto','-fno-inline','-std=gnu11','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-fsanitize=address,undefined','-pthread','-I',str(root),*map(str,sources),'-o',str(a.out/'engine')]
build=subprocess.run(cmd,capture_output=True,text=True)
run=subprocess.run([str((a.out/'engine').resolve())],capture_output=True,text=True,timeout=30) if build.returncode==0 else None
passed=bool(run and run.returncode==0 and 'HOST RESULT PASS' in run.stdout)
(a.out/'output.txt').write_text(build.stdout+build.stderr+(run.stdout+run.stderr if run else ''))
(a.out/'result.json').write_text(json.dumps(dict(result='PASS' if passed else 'FAIL',scope='HOST correctness, simulated CPU IDs',command=cmd,sources={str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in sources+[root/'dhry.h',root/'sysinfo.h']}),indent=2)+'\n')
print('xSysInfo HOST', 'PASS' if passed else 'FAIL');raise SystemExit(not passed)
