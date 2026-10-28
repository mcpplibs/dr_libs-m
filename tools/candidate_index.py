#!/usr/bin/env python3
"""The compat namespace from mcpp-index's candidate branch, while the published
index does not yet carry compat.dr_libs.

compat.dr_libs is added to mcpp-index by mcpplibs/mcpp-index#610. Until that
descriptor is on the index's main branch, a build of this repository cannot
resolve its one dependency from the published index. This tool then clones
the PR's branch, prepares the unchanged namespace views of its descriptors
(the index's own tests/prepare_mwrap_indices.py) and prints the checkout.

  --checkout-only   print the candidate checkout (nothing once published),
                    for tools/test_template.py --index
  --ci              also append `[indices] compat = { path = <view> }` to this
                    checkout's manifests (the root and examples/dr_libs), in
                    CI's working copy only

Once the published index has compat.dr_libs, it does nothing.
"""
import argparse
from pathlib import Path
import subprocess
import sys
import urllib.error
import urllib.request

INDEX = 'https://github.com/mcpplibs/mcpp-index'
BRANCH = 'compat-dr_libs'
PUBLISHED = 'https://raw.githubusercontent.com/mcpplibs/mcpp-index/main/pkgs/c/compat.dr_libs.lua'
MANIFESTS = ['mcpp.toml', 'examples/dr_libs/mcpp.toml']


def published() -> bool:
    try:
        with urllib.request.urlopen(PUBLISHED, timeout=60) as response:
            return response.status == 200
    except urllib.error.HTTPError as error:
        if error.code == 404:
            return False
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--checkout-only', action='store_true')
    mode.add_argument('--ci', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    if published():
        print('compat.dr_libs is published: the published index is used', file=sys.stderr)
        return
    checkout = root / 'target/candidate-index'
    if not (checkout / 'index.toml').exists():
        subprocess.run(['git', 'clone', '--quiet', '--depth', '1', '--branch', BRANCH, INDEX + '.git', str(checkout)],
                       check=True, stdout=sys.stderr)
    subprocess.run([sys.executable, str(checkout / 'tests/prepare_mwrap_indices.py')], cwd=checkout, check=True,
                   stdout=sys.stderr)
    view = checkout / 'tests/mwrap/.indices/compat'
    if not (view / 'pkgs/view/compat.dr_libs.lua').exists():
        sys.exit(f'{INDEX} {BRANCH} has no compat.dr_libs')
    if args.ci:
        for manifest in MANIFESTS:
            with (root / manifest).open('a', encoding='utf-8', newline='\n') as out:
                out.write('\n# CI only (tools/candidate_index.py): compat.dr_libs from ' + BRANCH + '\n')
                out.write('[indices]\ncompat = { path = "' + view.as_posix() + '" }\n')
            print('candidate compat index for', manifest, file=sys.stderr)
    print(checkout.as_posix())


if __name__ == '__main__':
    main()
