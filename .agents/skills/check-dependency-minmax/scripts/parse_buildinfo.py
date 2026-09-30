#!/usr/bin/env python3
"""Extract `oiiotool --buildinfo` from downloaded CI job logs.

Usage: parse_buildinfo.py <outdir> [dep ...]

Reads <outdir>/logs/*.log (see fetch_logs.sh), writes <outdir>/parsed.json
(job id -> name, compiler, cxx std, deps), and prints a pivot per dependency
(version -> jobs). With dep names given, only those are pivoted.
Jobs with no buildinfo (skip_tests jobs) get an empty deps dict.
"""
import glob, json, os, re, sys

out = sys.argv[1]
want = sys.argv[2:]
res = {}
for f in sorted(glob.glob(os.path.join(out, 'logs', '*.log'))):
    jid = os.path.basename(f)[:-4]
    name, blk, on, cmake = None, [], False, None
    for line in open(f, errors='replace').read().split('\n'):
        p = line.split('\t', 2)
        if len(p) < 3:
            continue
        name = p[0]
        mc = re.search(r'-- (?:Using )?CMake (?:version )?(\d+\.\d+\.\d+)', p[2])
        if mc and not cmake:
            cmake = mc.group(1)
        t = re.sub(r'^\S*Z ', '', p[2])      # strip timestamp
        if re.match(r'OIIO \d', t):
            on = True
        if on:
            if 'Results of oiiotool brief help' in t and not t.startswith('+'):
                break
            if t.startswith('+') or not t.strip():
                continue                      # shell trace interleaved by tee
            blk.append(t)
    txt = ' '.join(x.strip() for x in blk)
    m = re.search(r'Build compiler: (.*?) \| (C\+\+\d+)', txt)
    comp, std = (m.group(1), m.group(2)) if m else ('?', '?')
    m = re.search(r'Dependencies: (.*)$', txt)
    deps = {}
    if m:
        for item in m.group(1).split(', '):
            item = item.strip()
            mm = re.match(r'(\S+) (.*)', item)
            if mm:
                deps.setdefault(mm.group(1), mm.group(2))   # first dup wins
            elif item:
                deps.setdefault(item, 'present')
    if cmake and deps:      # CMake is not in buildinfo; take it from the build log
        deps['CMake'] = cmake
    res[jid] = dict(name=name, compiler=comp, std=std, deps=deps)

json.dump(res, open(os.path.join(out, 'parsed.json'), 'w'), indent=1)

print('== jobs')
for j, v in res.items():
    print(f"{j}  {v['name'][:70]:70}  {v['compiler']} {v['std']}  deps={len(v['deps'])}")

def vkey(s):
    return [int(x) if x.isdigit() else x for x in re.split(r'[.\-_ ]', s)]

# Compact table-A input: all distinct versions per dependency, ascending
# (plus compilers).
with open(os.path.join(out, 'versions.txt'), 'w') as vf:
    allv = {}
    for v in res.values():
        if v['compiler'] != '?':
            allv.setdefault('compiler', set()).add(f"{v['compiler']} {v['std']}")
        for d, ver in v['deps'].items():
            allv.setdefault(d, set()).add(ver)
    for d in sorted(allv, key=str.lower):
        try:
            vs = sorted(allv[d], key=vkey)
        except TypeError:
            vs = sorted(allv[d])
        vf.write(f"{d}: {', '.join(vs)}\n")

alldeps = sorted({d for v in res.values() for d in v['deps']}, key=str.lower)
for d in (want or alldeps):
    print('##', d)
    vals = {}
    for j, v in res.items():
        if not v['deps']:
            continue
        vals.setdefault(v['deps'].get(d, '-'), []).append(v['name'].split(' / ')[0])
    for ver, jobs in sorted(vals.items()):
        print(f'   {ver:12} <- {"; ".join(jobs)}')
