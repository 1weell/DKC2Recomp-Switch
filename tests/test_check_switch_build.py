import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("check_switch_build", Path(__file__).resolve().parents[1] / "scripts/check_switch_build.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

class SwitchBuildCheck(unittest.TestCase):
    def test_artifacts_and_truncation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "DKC2RecompSwitch").write_bytes(b"\x7fELF" + bytes(64))
            (root / "DKC2RecompSwitch.nacp").write_bytes(bytes(0x4000))
            nro = bytearray(128)
            nro[16:20] = b"NRO0"
            nro[24:28] = (128).to_bytes(4, "little")
            (root / "DKC2RecompSwitch.nro").write_bytes(nro)
            self.assertEqual(len(module.inspect_artifacts(root)), 3)
            nro[24:28] = (129).to_bytes(4, "little")
            (root / "DKC2RecompSwitch.nro").write_bytes(nro)
            with self.assertRaises(ValueError): module.inspect_artifacts(root)
            (root / "DKC2RecompSwitch.nro").unlink()
            with self.assertRaises(FileNotFoundError): module.inspect_artifacts(root)

if __name__ == "__main__": unittest.main()
