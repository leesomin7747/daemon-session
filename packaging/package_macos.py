"""Build a self-contained local macOS app, ZIP and drag-to-Applications DMG.

Ad-hoc signing is for local verification; this does not notarize a public release.
"""
import argparse
import datetime
import hashlib
import json
import pathlib
import platform
import re
import shutil
import subprocess
import sys
import tempfile
from verify_macos import macho_files,rpaths,verify_dependencies
ROOT=pathlib.Path(__file__).resolve().parents[1]
def run(*args,**kwargs):return subprocess.run([str(x) for x in args],check=True,**kwargs)
def output(*args):return subprocess.check_output([str(x) for x in args],text=True).strip()
def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--build-dir',type=pathlib.Path,default=ROOT/'out/release');parser.add_argument('--output-dir',type=pathlib.Path,default=ROOT/'out/packages');args=parser.parse_args()
 if sys.platform!='darwin':parser.error('macOS packaging must run on macOS')
 args.build_dir=args.build_dir.resolve();args.output_dir=args.output_dir.resolve();args.output_dir.mkdir(parents=True,exist_ok=True)
 run('cmake','-S',ROOT,'-B',args.build_dir,'-G','Ninja','-DCMAKE_BUILD_TYPE=Release','-DBUILD_TESTING=OFF','-DDAEMON_BUILD_DESKTOP=ON')
 run('cmake','--build',args.build_dir,'--target','daemon-session')
 version=re.search(r'project\(DaemonSession VERSION ([0-9.]+)',(ROOT/'CMakeLists.txt').read_text())[1]
 stamp=datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ');name=f'daemon-session-{version}-macos-{platform.machine()}-local-{stamp}'
 destination=args.output_dir/name
 # A fresh directory avoids overwriting any previously produced or user-modified package.
 destination.mkdir()
 # Qt's deployment path calculations can mix NFC/NFD on macOS. Use a
 # canonical ASCII staging path, then verify before relocating the bundle.
 stage=tempfile.TemporaryDirectory(prefix='daemon-session-package-')
 image=pathlib.Path(stage.name).resolve()/'image';image.mkdir();app=image/'daemon-session.app'
 shutil.copytree(args.build_dir/'desktop/daemon-session.app',app,symlinks=True)
 frameworks=app/'Contents/Frameworks';frameworks.mkdir(exist_ok=True)
 keychain=pathlib.Path(output('brew','--prefix','qtkeychain'))/'lib/libqt6keychain.1.dylib'
 bundled_keychain=frameworks/keychain.name;shutil.copy2(keychain,bundled_keychain)
 exe=app/'Contents/MacOS/daemon-session'
 run('install_name_tool','-change',keychain,'@rpath/'+keychain.name,exe)
 run('install_name_tool','-id','@rpath/'+keychain.name,bundled_keychain)
 run('macdeployqt',app,'-qmldir='+str(ROOT/'desktop/qml'),'-executable='+str(bundled_keychain),'-always-overwrite','-no-codesign')
 if '@executable_path/../Frameworks' not in rpaths(exe):run('install_name_tool','-add_rpath','@executable_path/../Frameworks',exe)
 for binary in macho_files(app):
  for search in rpaths(binary):
   expanded=pathlib.Path(search)
   if search.startswith('@loader_path/'):expanded=binary.parent/search.removeprefix('@loader_path/')
   elif search.startswith('@executable_path/'):expanded=exe.parent/search.removeprefix('@executable_path/')
   if expanded.is_absolute() and not str(expanded).startswith(('/System/Library/','/usr/lib/')) and not expanded.resolve().is_relative_to(app.resolve()):run('install_name_tool','-delete_rpath',search,binary)
 resources=app/'Contents/Resources';resources.mkdir(exist_ok=True)
 shutil.copyfile(ROOT/'THIRD_PARTY_NOTICES.md',resources/'THIRD_PARTY_NOTICES.md');shutil.copytree(ROOT/'packaging/licenses',resources/'licenses')
 sbom=resources/'licenses/sbom';sbom.mkdir()
 for prefix in pathlib.Path('/opt/homebrew/opt').glob('qt*'):
  for path in (prefix/'share/qt/sbom').glob('*.spdx'):shutil.copy2(path,sbom/path.name)
 for library in frameworks.glob('lib*.dylib'):
  for original in pathlib.Path('/opt/homebrew/opt').glob('*/lib/'+library.name):
   prefix=original.parent.parent.resolve();notices=resources/'licenses'/prefix.parent.name;notices.mkdir(exist_ok=True)
   for pattern in ['COPYING*','LICENSE*','AUTHORS*','NOTICE*','sbom.spdx.json']:
    for source in prefix.glob(pattern):
     if source.is_file():shutil.copy2(source,notices/source.name)
 errors=verify_dependencies(app)
 if errors:raise RuntimeError('\n'.join(errors))
 if not (app/'Contents/PlugIns/platforms/libqcocoa.dylib').is_file():raise RuntimeError('Cocoa platform plugin missing')
 if not (resources/'qml').is_dir():raise RuntimeError('Deployed QML imports missing')
 # Local ad-hoc signature only; no Developer ID or notarization is implied.
 run('codesign','--force','--deep','--sign','-',app)
 run('codesign','--verify','--deep','--strict',app)
 run(exe,'--preview','--smoke-test',env={**__import__('os').environ,'QML_IMPORT_PATH':str(destination/'empty-imports'),'QT_PLUGIN_PATH':str(destination/'empty-plugins')})
 (image/'Applications').symlink_to('/Applications')
 archive=destination/(name+'.zip');disk=destination/(name+'.dmg')
 run('ditto','-c','-k','--sequesterRsrc','--keepParent',app,archive)
 run('hdiutil','create','-volname','Daemon Session','-srcfolder',image,'-format','UDZO',disk)
 manifest={'version':version,'architecture':platform.machine(),'sourceCommit':output('git','-C',ROOT,'rev-parse','HEAD'),'sourceDirty':bool(output('git','-C',ROOT,'status','--porcelain')),'signing':'ad-hoc local verification; not Developer ID signed or notarized','builtAtUtc':stamp,'minimumOSBuildCommands':output('vtool','-show-build',exe),'qt':output('qtpaths','--qt-version'),'files':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in [archive,disk]}}
 (destination/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
 (destination/'SHA256SUMS').write_text(''.join(f'{digest}  {file}\n' for file,digest in manifest['files'].items()))
 shutil.move(str(image),str(destination/'image'));stage.cleanup()
 print(destination)
if __name__=='__main__':main()
