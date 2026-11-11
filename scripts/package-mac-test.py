#!/usr/bin/env python3
"""Package the verified Experiment 000 ELF and a fresh card for Mac testing."""
import hashlib
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile
import zipfile

root = Path(__file__).resolve().parents[1]
firmware = root / 'artifacts/delxr-marker.elf'
expected = '7d69a5f067981c36d977bf00cf824e9164e4f205a1c8640d5c464c3755bad9ae'
assert hashlib.sha256(firmware.read_bytes()).hexdigest() == expected, 'Expected the verified marker ELF'
archive = root / 'artifacts/Delxr-Experiment-000-Mac-Test.zip'
with tempfile.TemporaryDirectory(prefix='delxr-package-') as tmp:
    bundle = Path(tmp) / 'Delxr-Experiment-000'
    bundle.mkdir()
    for file in (root / 'packaging/macos').iterdir():
        shutil.copy2(file, bundle / file.name)
    shutil.copy2(firmware, bundle / 'delxr-marker.elf')
    shutil.copy2(root / 'upstream/firmware/LICENSE', bundle / 'FIRMWARE-LICENSE.txt')
    card = Path(tmp) / 'card'
    for name in ['SONGS', 'SYNTHS', 'KITS', 'SAMPLES']:
        (card / name).mkdir(parents=True)
    subprocess.run([str(root / 'upstream/delugemu/scripts/mksd.sh'), str(card), str(bundle / 'delxr-sd.img')], check=True)
    source = bundle / 'source'
    source.mkdir()
    with (source / 'firmware.tar.gz').open('wb') as output:
        subprocess.run(['git', '-C', str(root / 'upstream/firmware'), 'archive', '--format=tar.gz',
                        '--prefix=firmware/', 'c23bc2fecb38ccbdcd3c187870f527277f66e726'], stdout=output, check=True)
    with tarfile.open(source / 'argon.tar.gz', 'w:gz') as output:
        output.add(root / 'upstream/firmware/build/_deps/argon-src', arcname='argon')
    shutil.copy2(root / 'experiments/000-delxr-label.patch', source)
    shutil.copytree(root / 'scripts', source / 'scripts', ignore=shutil.ignore_patterns('__pycache__'))
    shutil.copy2(root / 'README.md', source / 'DELXR-README.md')
    (source / 'README.md').write_text('''# Corresponding source

firmware.tar.gz is the complete tracked baseline, including its build files and
licence notices. Apply 000-delxr-label.patch to reproduce the modified source.
argon.tar.gz contains the exact fetched Argon dependency. No local firmware
commits are needed. The attached scripts and DELXR-README.md describe the
verified Linux build environment; native Mac compilation was not tested.

To use the supplied source archives on Linux x86_64 with DBT toolchain 16:

```bash
tar -xzf firmware.tar.gz
tar -xzf argon.tar.gz
(cd firmware && patch -p1 < ../000-delxr-label.patch)
# Point this at the parent of toolchain/v16/linux-x86_64:
export DBT_TOOLCHAIN_PATH=/path/to/delxr/.cache
export DELUGE_FW_ROOT="$PWD/firmware"
tools="$DBT_TOOLCHAIN_PATH/toolchain/v16/linux-x86_64"
export PATH="$tools/cmake/bin:$tools/ninja-build/bin:$PATH"
cmake -S firmware -B firmware/build -G 'Ninja Multi-Config' \\
  -DCMAKE_DISABLE_FIND_PACKAGE_Doxygen=TRUE \\
  -DFETCHCONTENT_SOURCE_DIR_ARGON="$PWD/argon"
cmake --build firmware/build --config Release --target deluge -j 4
```

Without Git metadata, CMake's Git revision queries can issue diagnostics; the
source version is the pinned release recorded in START-HERE.md. Archive rebuilds
may differ in debug paths/metadata. For the original checkout workflow, follow
DELXR-README.md using the recorded Git revisions. The standard compiler/toolchain
is separately available from SynthstromAudible/dbt-toolchain release v16;
its archive URL and SHA-256 are recorded in scripts/setup.sh.
''')
    files = sorted(p for p in bundle.rglob('*') if p.is_file())
    (bundle / 'SHA256SUMS').write_text(''.join(f'{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.relative_to(bundle)}\n' for p in files))
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as output:
        for p in sorted(bundle.rglob('*')):
            if p.is_file():
                output.write(p, p.relative_to(Path(tmp)))
print(archive)
print(f'{archive.stat().st_size / 1024**2:.1f} MiB')
print('SHA-256:', hashlib.sha256(archive.read_bytes()).hexdigest())
