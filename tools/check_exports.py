#!/usr/bin/env python3
"""Audit the module surface against the pinned commit's dr_libs headers.

- src/*.cppm, tests/macros_mix_test.cpp and tests/*_surface_test.cpp are what
  tools/gen_exports.py writes.
- Every type, struct tag, enumerator and *_API function of each header's
  public part is exported by `export using` from that header's submodule
  (mcpplibs.mwrap.dr_libs.wav, .flac, .mp3), and nothing else is; each
  surface test names all of them with only that submodule imported.
- Every macro of the three headers is mapped in src/macros.cppm (a constant
  or a string), kept as a macro in <mwrap/dr_libs/macros.hpp> with its
  reason, or an include guard; and each mapping is checked against the raw
  header by tests/macros_mix_test.cpp.
"""
from pathlib import Path
import re
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from inventory import fetch, surface  # noqa: E402
from gen_exports import SIDE_HEADER, excluded  # noqa: E402


def main():
    root = Path(__file__).resolve().parents[1]
    problems = []
    generated = subprocess.run([sys.executable, str(root / 'tools/gen_exports.py'), '--check'], capture_output=True, text=True)
    if generated.returncode != 0:
        problems.append(generated.stderr.strip() or generated.stdout.strip())
    headers = surface(fetch(root))
    counts = []
    for h in headers:
        module = (root / f'src/{h.short}.cppm').read_text(encoding='utf-8')
        exported = re.findall(r'^export using ::(\w+);', module, flags=re.M)
        declared = set(h.types) | set(h.tags) | set(h.enumerators) | set(h.functions)
        if len(exported) != len(set(exported)):
            problems.append(f'src/{h.short}.cppm exports a name twice')
        if declared - set(exported):
            problems.append(f'src/{h.short}.cppm lacks: ' + ', '.join(sorted(declared - set(exported))))
        if set(exported) - declared:
            problems.append(f'src/{h.short}.cppm exports what {h.name} does not declare: ' + ', '.join(sorted(set(exported) - declared)))
        test = (root / f'tests/{h.short}_surface_test.cpp').read_text(encoding='utf-8')
        unnamed = {n for n in declared - set(h.tags) if f'::{n}' not in test}
        if unnamed:
            problems.append(f'tests/{h.short}_surface_test.cpp does not name: ' + ', '.join(sorted(unnamed)))
        counts.append(f'{h.name}: {len(h.types)} types, {len(h.enumerators)} enumerators, {len(h.functions)} functions')

    macros_text = (root / 'src/macros.cppm').read_text(encoding='utf-8')
    body = macros_text[macros_text.index('export module'):]
    mapped = set(re.findall(r'^inline constexpr auto&? (\w+) = c::value_\1;', body, flags=re.M))
    macros = {}
    for h in headers:
        macros |= h.macros
    guards = {m for m in macros if excluded(m)}
    unaccounted = set(macros) - mapped - SIDE_HEADER.keys() - guards
    if unaccounted:
        problems.append('macros neither mapped, side-header nor include guards: ' + ', '.join(sorted(unaccounted)))
    both = SIDE_HEADER.keys() & mapped
    if both:
        problems.append('listed as side-header yet mapped: ' + ', '.join(sorted(both)))
    stale = SIDE_HEADER.keys() - set(macros)
    if stale:
        problems.append('listed but not in the headers: ' + ', '.join(sorted(stale)))
    test = (root / 'tests/macros_mix_test.cpp').read_text(encoding='utf-8')
    untested = {n for n in mapped if f'm::{n}' not in test}
    if untested:
        problems.append('mapped but not checked against the header: ' + ', '.join(sorted(untested)))
    side = (root / 'include/mwrap/dr_libs/macros.hpp').read_text(encoding='utf-8')
    for name in ('dr_wav.h', 'dr_flac.h', 'dr_mp3.h'):
        if f'#include <{name}>' not in side:
            problems.append(f'<mwrap/dr_libs/macros.hpp> does not include <{name}>')
    if problems:
        sys.exit('\n'.join(problems))
    print('; '.join(counts) + ' — all exported, each submodule complete on its own; '
          f'{len(macros)} macros: {len(mapped)} mapped as constants (each checked against the header), '
          f'{len(SIDE_HEADER)} kept as macros, {len(guards)} include guards')


if __name__ == '__main__':
    main()
