"""Reject runtime dependencies and search paths outside a macOS app bundle."""
import argparse
import pathlib
import plistlib
import re
import subprocess
import sys

MAGIC={b'\xcf\xfa\xed\xfe',b'\xce\xfa\xed\xfe',b'\xfe\xed\xfa\xcf',b'\xfe\xed\xfa\xce',b'\xca\xfe\xba\xbe',b'\xca\xfe\xba\xbf'}
def macho_files(app):
    found=set()
    for path in pathlib.Path(app).rglob('*'):
        if not path.is_file():continue
        real=path.resolve()
        if real in found:continue
        with path.open('rb') as f:magic=f.read(4)
        if magic in MAGIC:
            found.add(real);yield real

def run(*args):
    return subprocess.run(args,check=True,capture_output=True,text=True,timeout=30).stdout

def rpaths(path):
    return re.findall(r'cmd LC_RPATH\s+cmdsize \d+\s+path (.*?) \(offset \d+\)',run('otool','-l',str(path)))

def inside(path,app):
    return path.resolve().is_relative_to(app)

def system(path):
    return path.startswith('/System/Library/') or path.startswith('/usr/lib/')

def verify_dependencies(app):
    app=pathlib.Path(app).resolve();errors=[];binaries=list(macho_files(app))
    if not binaries:return ['no Mach-O executables in bundle']
    plist=app/'Contents/Info.plist'
    executable=None
    if plist.exists():
        with plist.open('rb') as f:executable=app/'Contents/MacOS'/plistlib.load(f)['CFBundleExecutable']
    else:
        candidates=list((app/'Contents/MacOS').glob('*'));executable=candidates[0] if len(candidates)==1 else None
    if not executable or not executable.is_file():return ['missing main executable']
    executable=executable.resolve();main_rpaths=rpaths(executable)
    def expand(value,binary):
        if value.startswith('@executable_path/') :return executable.parent/value.removeprefix('@executable_path/')
        if value.startswith('@loader_path/'):return binary.parent/value.removeprefix('@loader_path/')
        return pathlib.Path(value)
    for binary in binaries:
        paths=rpaths(binary)
        for search in paths:
            expanded=expand(search,binary)
            if expanded.is_absolute() and not inside(expanded,app) and not system(str(expanded)):
                errors.append(f'external rpath: {binary.relative_to(app)}: {search}')
        own_id=run('otool','-D',str(binary)).splitlines()[1:]
        for row in run('otool','-L',str(binary)).splitlines()[1:]:
            dep=row.strip().split(' (compatibility version',1)[0]
            if not dep or dep in own_id or system(dep):continue
            if dep.startswith('/'):
                if not inside(pathlib.Path(dep),app):errors.append(f'external dependency: {binary.relative_to(app)}: {dep}');continue
                candidates=[pathlib.Path(dep)]
            elif dep.startswith('@rpath/'):
                tail=dep.removeprefix('@rpath/')
                candidates=[expand(p,binary)/tail for p in paths]+[expand(p,executable)/tail for p in main_rpaths]
            else:candidates=[expand(dep,binary)]
            if not any(c.is_absolute() and inside(c,app) and c.is_file() for c in candidates):
                errors.append(f'unresolved dependency: {binary.relative_to(app)}: {dep}')
    return errors

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('app',type=pathlib.Path);args=parser.parse_args()
    errors=verify_dependencies(args.app)
    for error in errors:print(error,file=sys.stderr)
    if errors:return 1
    print('All non-system Mach-O dependencies resolve inside the app.');return 0
if __name__=='__main__':sys.exit(main())
