import importlib.util
import pathlib
import shutil
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('verify_macos', ROOT/'packaging/verify_macos.py')
verify = importlib.util.module_from_spec(spec)
spec.loader.exec_module(verify)

class BundledDependencies(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = pathlib.Path(self.temp.name)
        self.app = self.root/'Fixture.app'
        self.exe = self.app/'Contents/MacOS/fixture'
        self.exe.parent.mkdir(parents=True)
        self.lib = self.root/'libfixture.dylib'
        (self.root/'lib.c').write_text('int fixture(void) {return 0;}\n')
        (self.root/'main.c').write_text('int fixture(void); int main(void) {return fixture();}\n')
        subprocess.run(['clang','-dynamiclib',str(self.root/'lib.c'),'-o',str(self.lib)],check=True,capture_output=True)
        subprocess.run(['clang',str(self.root/'main.c'),str(self.lib),'-o',str(self.exe)],check=True,capture_output=True)
    def make_local(self):
        target=self.app/'Contents/Frameworks'/self.lib.name
        target.parent.mkdir();shutil.copy2(self.lib,target)
        subprocess.run(['install_name_tool','-change',str(self.lib),'@executable_path/../Frameworks/'+self.lib.name,str(self.exe)],check=True,capture_output=True)
        subprocess.run(['install_name_tool','-id','@rpath/'+self.lib.name,str(target)],check=True,capture_output=True)
    def test_external_runtime_library_is_rejected(self):
        errors=verify.verify_dependencies(self.app)
        self.assertTrue(any('external dependency' in e for e in errors),errors)
    def test_relative_bundled_library_is_accepted(self):
        self.make_local()
        self.assertEqual(verify.verify_dependencies(self.app),[])
    def test_missing_bundled_library_is_rejected(self):
        self.make_local();(self.app/'Contents/Frameworks'/self.lib.name).unlink()
        errors=verify.verify_dependencies(self.app)
        self.assertTrue(any('unresolved dependency' in e for e in errors),errors)
    def test_external_rpath_is_rejected(self):
        self.make_local()
        subprocess.run(['install_name_tool','-add_rpath','/opt/homebrew/lib',str(self.exe)],check=True,capture_output=True)
        errors=verify.verify_dependencies(self.app)
        self.assertTrue(any('external rpath' in e for e in errors),errors)

if __name__=='__main__':unittest.main()
