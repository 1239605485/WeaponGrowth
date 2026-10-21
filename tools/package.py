"""Validate all five ABI modules, then package a clean source tree and import ZIP."""
from pathlib import Path
import argparse, hashlib, json, subprocess, zipfile
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser()
p.add_argument('--llvm',default='C:/orbuild/toolchain/android-ndk-r26c/toolchains/llvm/prebuilt/windows-x86_64/bin')
a=p.parse_args()
info=json.loads((ROOT/'Info.json').read_text(encoding='utf-8'));version=info['version']
required=['create_kernel_mod','mod_logger_write','patchlib_install_prepost_hook','patchlib_method_invoke_args']
binaries=[ROOT/'build-android/libWeaponGrowth.android.arm64.so']
binaries += [ROOT/'build-desktop'/name for name in ['WeaponGrowth.windows.x86.dll','WeaponGrowth.windows.x64.dll','libWeaponGrowth.linux.x86.so','libWeaponGrowth.linux.x64.so']]
exe='.exe' if (Path(a.llvm)/'llvm-readobj.exe').exists() else ''
for binary in binaries:
 assert binary.exists(),str(binary)
 assert version.encode()+b'\0' in binary.read_bytes(),'version mismatch: '+str(binary)
 if binary.suffix=='.dll':
  exports=subprocess.check_output([str(Path(a.llvm)/('llvm-readobj'+exe)),'--coff-exports',str(binary)],text=True)
  for s in required:assert 'Name: '+s+'\n' in exports,'missing export '+s
  machine=int.from_bytes(binary.read_bytes()[int.from_bytes(binary.read_bytes()[60:64],'little')+4:][:2],'little')
  assert machine==(0x14c if '.x86.' in binary.name else 0x8664)
 else:
  header=subprocess.check_output([str(Path(a.llvm)/('llvm-readelf'+exe)),'-h',str(binary)],text=True)
  expected='AArch64' if 'android' in binary.name else ('Intel 80386' if '.x86.' in binary.name else 'X86-64')
  assert expected in header,(binary,header)
  symbols=subprocess.check_output([str(Path(a.llvm)/('llvm-readelf'+exe)),'--dyn-syms','--wide',str(binary)],text=True)
  for s in required:assert s in symbols,'missing export '+s
  dynamic=subprocess.check_output([str(Path(a.llvm)/('llvm-readelf'+exe)),'-d',str(binary)],text=True)
  assert 'libc++_shared.so' not in dynamic
 print('ABI/export/version verified:',binary.name)
assert not (ROOT/'src/ui').exists() and not (ROOT/'Resources/ui').exists()
for name in ['Manifest.json','kernelmod.json','Resources/config.json']:json.loads((ROOT/name).read_text(encoding='utf-8'))
dist=ROOT/'dist';dist.mkdir(exist_ok=True)
package=dist/f'WeaponGrowth-KernelLoader-{version}-all-platforms.zip'
with zipfile.ZipFile(package,'w',zipfile.ZIP_DEFLATED) as z:
 for name in ['Info.json','Manifest.json','kernelmod.json','icon.png','LICENSE','README.md','THIRD_PARTY_NOTICES.md']:z.write(ROOT/name,name)
 for folder in ['Resources','LICENSES']:
  for f in sorted((ROOT/folder).rglob('*')):
   if f.is_file():z.write(f,f.relative_to(ROOT))
 for f in binaries:z.write(f,'Resources/lib/'+f.name)
source=dist/f'WeaponGrowth-KernelLoader-{version}-source.zip'
with zipfile.ZipFile(source,'w',zipfile.ZIP_DEFLATED) as z:
 for name in ['CMakeLists.txt','Info.json','Manifest.json','kernelmod.json','icon.png','LICENSE','README.md','THIRD_PARTY_NOTICES.md','.gitignore']:
  z.write(ROOT/name,Path(ROOT.name)/name)
 for folder in ['src','tests','tools','vendor','Resources','LICENSES','docs']:
  for f in sorted((ROOT/folder).rglob('*')):
   if f.is_file() and '__pycache__' not in f.parts:
    assert f.suffix not in ['.dll','.so','.exe','.pdb','.log','.zip'],str(f)
    z.write(f,Path(ROOT.name)/f.relative_to(ROOT))
for f in [package,source]:
 with zipfile.ZipFile(f) as z:assert z.testzip() is None
sums=''.join(hashlib.sha256(f.read_bytes()).hexdigest()+'  '+f.name+'\n' for f in [package,source])
(dist/'SHA256SUMS.txt').write_text(sums,encoding='utf-8');print(sums)
