#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""The shader pre-build catalog: which shader permutations and pipelines the game draws, merged
from players' pipeline caches (user/cache/CUSA03173), for every install to translate before
first sight (vk_shader_prebuild.h; run.py passes it as BB_SHADER_CATALOG).

Only the records are kept - a permutation's specialization and resource layout (.meta) and a
pipeline's state (.key) - in the current format. No SPIR-V and no game code: each install
translates the shaders from its own game files.

  shader_catalog.py <catalog folder> <cache folder> [<cache folder> ...]
"""
from pathlib import Path
import shutil
import struct
import sys

# The versions the game writes (gpu/.../vk_pipeline_serialization.cpp): older records are left.
SHADER_META_VERSION = 8
PIPELINE_KEY_VERSION = 5


def current(path):
    want = SHADER_META_VERSION if path.suffix == '.meta' else PIPELINE_KEY_VERSION
    with path.open('rb') as file:
        head = file.read(4)
    return len(head) == 4 and struct.unpack('<I', head)[0] == want


def merge(catalog, caches):
    catalog = Path(catalog)
    catalog.mkdir(parents=True, exist_ok=True)
    added = 0
    for cache in caches:
        for record in sorted(Path(cache).iterdir()):
            if record.suffix not in ('.meta', '.key') or not current(record):
                continue
            target = catalog / record.name
            if not target.exists():
                shutil.copyfile(record, target)
                added += 1
    total = sum(1 for p in catalog.iterdir() if p.suffix in ('.meta', '.key'))
    return added, total


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    added, total = merge(sys.argv[1], sys.argv[2:])
    print(f'shader catalog: {added} records added, {total} in total')


if __name__ == '__main__':
    main()
