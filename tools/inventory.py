#!/usr/bin/env python3
"""The public surface of dr_wav, dr_flac and dr_mp3, from the pinned commit.

Shared by tools/gen_exports.py (which writes the module sources and the
generated macro test) and tools/check_exports.py (which audits them). The
headers come from the upstream commit archive compat.dr_libs builds, downloaded
once into target/upstream and checked against upstream/documents.json.

Each header's public part is the block between `#ifndef dr_<x>_h` and its
`#endif  /* dr_<x>_h */`; what follows is the implementation, compiled only
under DR_<X>_IMPLEMENTATION (by compat.dr_libs' generated TU, never by the
module). The headers are read as compat.dr_libs compiles them: nothing is
defined, so DR_WAV_NO_STDIO, DR_WAV_NO_CONVERSION_API, DR_FLAC_NO_STDIO,
DR_MP3_NO_STDIO … are all absent and their blocks declare what they declare.
Both branches of the compiler and word-size conditionals declare the same
names (drwav_int64 is `__int64` under MSVC and `long long` elsewhere), so a
name is a name whichever branch a compiler takes.

Every top-level declaration is one of: a typedef (plain, of a function
pointer, of a struct or of an enum — every enum is named by its typedef, so
its enumerators have linkage and are exported as they are), a struct
definition, or a *_API function. Anything else stops the inventory.
"""
from dataclasses import dataclass, field
import hashlib
import json
from pathlib import Path
import re
import sys
import tarfile
import urllib.request

HEADERS = {  # header: (short name, its API macro)
    'dr_wav.h': ('wav', 'DRWAV_API'),
    'dr_flac.h': ('flac', 'DRFLAC_API'),
    'dr_mp3.h': ('mp3', 'DRMP3_API'),
}


def fetch(root: Path) -> Path:
    pins = json.loads((root / 'upstream/documents.json').read_text(encoding='utf-8'))
    cache = root / 'target/upstream'
    cache.mkdir(parents=True, exist_ok=True)
    archive = cache / ('dr_libs-' + pins['upstream_commit'] + '.tar.gz')
    if not archive.exists() or hashlib.sha256(archive.read_bytes()).hexdigest() != pins['upstream_sha256']:
        print('+ fetch', pins['upstream'], flush=True)
        with urllib.request.urlopen(pins['upstream'], timeout=120) as response:
            archive.write_bytes(response.read())
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    if digest != pins['upstream_sha256']:
        sys.exit(f'upstream archive checksum {digest} != {pins["upstream_sha256"]}')
    with tarfile.open(archive) as tar:
        top = tar.getmembers()[0].name.split('/')[0]
        if not (cache / top / 'dr_wav.h').exists():
            tar.extractall(cache, filter='data')
    return cache / top


@dataclass
class Macro:
    name: str
    header: str
    params: list[str] | None   # None: object-like
    body: str
    conditional: bool          # defined inside an #if of its own (beyond the include guard)


@dataclass
class Header:
    name: str                  # dr_wav.h
    short: str                 # wav
    types: list[str] = field(default_factory=list)        # typedef names
    tags: list[str] = field(default_factory=list)         # struct tags
    enumerators: dict[str, str] = field(default_factory=dict)  # enumerator: its enum's typedef
    functions: list[str] = field(default_factory=list)
    deprecated: list[str] = field(default_factory=list)
    macros: dict[str, Macro] = field(default_factory=dict)


def strip(text: str) -> str:
    text = re.sub(r'/\*.*?\*/', lambda m: '\n' * m.group(0).count('\n'), text, flags=re.S)
    return re.sub(r'//[^\n]*', '', text)


def public_part(raw: str, short: str) -> str:
    start = re.search(rf'^#ifndef dr_{short}_h\b', raw, flags=re.M)
    end = re.search(rf'^#endif\s*/\*\s*dr_{short}_h\s*\*/', raw, flags=re.M)
    if not start or not end:
        sys.exit(f'dr_{short}.h: no public part')
    return raw[start.start():end.end()]


def macros_of(text: str, header: str) -> dict[str, Macro]:
    out: dict[str, Macro] = {}
    depth = 0  # #if nesting, the include guard at 1
    lines = text.split('\n')
    i = 0
    while i < len(lines):
        line = lines[i]
        while line.endswith('\\') and i + 1 < len(lines):
            i += 1
            line = line[:-1] + ' ' + lines[i]
        i += 1
        m = re.match(r'^\s*#\s*(\w+)(.*)$', line)
        if not m:
            continue
        directive, rest = m.group(1), m.group(2)
        if directive in ('if', 'ifdef', 'ifndef'):
            depth += 1
        elif directive == 'endif':
            depth -= 1
        elif directive == 'define':
            d = re.match(r'\s+(\w+)(\(([^)]*)\))?\s*(.*)$', rest)
            params = [p.strip() for p in d.group(3).split(',')] if d.group(2) else None
            body = ' '.join(d.group(4).split())
            name = d.group(1)
            if name in out and out[name].body != body:
                out[name].conditional = True  # defined differently in different branches
                continue
            out.setdefault(name, Macro(name, header, params, body, depth > 1))
    return out


def split_top(code: str) -> list[str]:
    """Top-level statements, split at `;` outside braces and parentheses."""
    out, depth, cur = [], 0, []
    for ch in code:
        if ch in '{(':
            depth += 1
        elif ch in '})':
            depth -= 1
        if ch == ';' and depth == 0:
            out.append(''.join(cur).strip())
            cur = []
        else:
            cur.append(ch)
    if ''.join(cur).strip():
        sys.exit('trailing text without `;`: ' + ''.join(cur).strip()[:200])
    return [s for s in out if s]


def braces(stmt: str) -> tuple[str, str, str]:
    """(before the outermost `{`, the body inside it, after its `}`)."""
    a = stmt.index('{')
    depth = 0
    for j in range(a, len(stmt)):
        depth += {'{': 1, '}': -1}.get(stmt[j], 0)
        if depth == 0:
            return stmt[:a], stmt[a + 1:j], stmt[j + 1:]
    sys.exit('unbalanced braces: ' + stmt[:200])


def surface_of(upstream: Path, name: str) -> Header:
    short, api = HEADERS[name]
    raw = (upstream / name).read_text(encoding='utf-8')
    text = strip(public_part(raw, short))
    h = Header(name, short)
    h.macros = macros_of(text, name)
    # The extern "C" brackets, then every preprocessor line.
    code = re.sub(r'#ifdef __cplusplus\s*extern "C" \{\s*#endif', '', text)
    code = re.sub(r'#ifdef __cplusplus\s*\}\s*#endif', '', code)
    code = re.sub(r'^[ \t]*#(?:[^\n]*\\\n)*[^\n]*', '', code, flags=re.M)
    for stmt in split_top(code):
        s = ' '.join(stmt.split())
        if s.startswith('typedef enum'):
            head, body, tail = braces(s)
            t = tail.strip()
            if not re.fullmatch(r'\w+', t):
                sys.exit(f'{name}: enum typedef of an unexpected shape: {s[:120]}')
            h.types.append(t)
            for e in body.split(','):
                e = e.split('=')[0].strip()
                if e:
                    h.enumerators[e] = t
        elif re.match(r'typedef (struct|union)\b', s) and '{' in s:
            head, body, tail = braces(s)
            tag = re.fullmatch(r'typedef (?:struct|union)\s*(\w*)\s*', head)
            if not tag or not re.fullmatch(r'\w+', tail.strip()):
                sys.exit(f'{name}: struct typedef of an unexpected shape: {s[:120]}')
            if tag.group(1):
                h.tags.append(tag.group(1))
            h.types.append(tail.strip())
        elif re.match(r'typedef (struct|union) \w+ \w+$', s):
            _, _, tag, t = s.split()
            h.tags.append(tag)
            h.types.append(t)
        elif s.startswith('typedef') and re.search(r'\(\s*\*\s*\w+\s*\)\s*\(', s):
            h.types.append(re.search(r'\(\s*\*\s*(\w+)\s*\)\s*\(', s).group(1))
        elif s.startswith('typedef'):
            m = re.fullmatch(r'typedef [\w\s\*]+?\b(\w+)', s)
            if not m:
                sys.exit(f'{name}: typedef of an unexpected shape: {s[:120]}')
            h.types.append(m.group(1))
        elif re.match(r'(struct|union) \w+ \{', s):
            head, body, tail = braces(s)
            if tail.strip():
                sys.exit(f'{name}: struct definition with declarators: {s[:120]}')
            h.tags.append(head.split()[1])
        elif re.match(rf'(\w+_DEPRECATED )?{api}\b', s):
            f = re.search(r'\b(\w+)\s*\(', s)
            if not f:
                sys.exit(f'{name}: {api} on something that is not a function: {s[:120]}')
            h.functions.append(f.group(1))
            if '_DEPRECATED' in s.split(api)[0]:
                h.deprecated.append(f.group(1))
        else:
            sys.exit(f'{name}: a declaration the inventory does not know: {s[:200]}')
    h.types = sorted(set(h.types))
    h.tags = sorted(set(h.tags))
    h.functions = sorted(set(h.functions))
    h.deprecated = sorted(set(h.deprecated))
    return h


def surface(upstream: Path) -> list[Header]:
    return [surface_of(upstream, name) for name in HEADERS]


if __name__ == '__main__':
    root = Path(__file__).resolve().parents[1]
    for h in surface(fetch(root)):
        print(f'== {h.name}: {len(h.types)} types, {len(h.tags)} struct tags, {len(h.enumerators)} enumerators, '
              f'{len(h.functions)} functions ({len(h.deprecated)} deprecated), {len(h.macros)} macros')
        print('types:', ' '.join(h.types))
        print('tags:', ' '.join(h.tags))
        print('enum types:', ' '.join(sorted(set(h.enumerators.values()))))
        print('deprecated:', ' '.join(h.deprecated))
        print('object-like:', ' '.join(m.name + ('?' if m.conditional else '') for m in h.macros.values() if m.params is None))
        print('function-like:', ' '.join(m.name + ('?' if m.conditional else '') for m in h.macros.values() if m.params is not None))
