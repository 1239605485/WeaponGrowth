"""Cross-build all desktop modules with Zig 0.14.1; no game SDK install needed."""
from pathlib import Path
import argparse
import subprocess
import shutil
import os

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser()
p.add_argument('--zig', default=shutil.which('zig') or str(ROOT / ('.tools/ziglang/zig.exe' if os.name=='nt' else '.tools/ziglang/zig')))
p.add_argument('--platform', choices=['all', 'windows', 'linux'], default='all')
args = p.parse_args()
sources = [str(p) for d in ['growth', 'catalog', 'storage', 'gacha', 'loader', 'engine', 'runtime', 'commands']
           for p in (ROOT / 'src' / d).glob('*.cpp')]
targets = [('windows', 'x86', 'x86-windows-gnu'), ('windows', 'x64', 'x86_64-windows-gnu'),
           ('linux', 'x86', 'x86-linux-gnu.2.17'), ('linux', 'x64', 'x86_64-linux-gnu.2.17')]
for platform, arch, target in targets:
    if args.platform not in ['all', platform]:
        continue
    output = ROOT / 'build-desktop' / (('' if platform == 'windows' else 'lib') +
             f'WeaponGrowth.{platform}.{arch}' + ('.dll' if platform == 'windows' else '.so'))
    output.parent.mkdir(exist_ok=True)
    command = [args.zig, 'c++', '-target', target, '-std=c++17', '-O2', '-shared', '-DBUILDING_DLL',
               '-Isrc', '-Ivendor', *sources, '-o', str(output)]
    if platform == 'windows':
        command += ['-Wl,--export-all-symbols']
    else:
        command += ['-fPIC', '-Wl,--no-undefined']
    print(f'Building {platform}/{arch}', flush=True)
    subprocess.run(command, cwd=ROOT, check=True)
    print(output, flush=True)
