import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class ScanProjectTest(unittest.TestCase):
    def test_scan_project_reports_real_inventory_without_modifying_source(self):
        config = json.loads((ROOT / "configs" / "project_config.json").read_text(encoding="utf-8"))
        source = Path(config["source_repo"])
        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp) / "inventory.md"
            json_output = Path(tmp) / "inventory.json"
            result = subprocess.run(
                [
                    sys.executable,
                    str(ROOT / "tools" / "scan_project.py"),
                    "--source",
                    str(source),
                    "--output",
                    str(output),
                    "--json-output",
                    str(json_output),
                ],
                cwd=ROOT,
                text=True,
                capture_output=True,
            )

            self.assertEqual(result.returncode, 0, result.stderr)
            data = json.loads(json_output.read_text(encoding="utf-8"))
            self.assertEqual(data["source_repo"], str(source))
            self.assertEqual(data["solutions"], [])
            self.assertIn("Server1/Server1/Server/Server.vcxproj", data["vcxproj"])
            self.assertIn("Client1/Client1/Client/Client.vcxproj", data["vcxproj"])
            self.assertTrue(any("UE_MyProject" in item for item in data["ue_files"]))
            self.assertTrue(output.exists())


if __name__ == "__main__":
    unittest.main()
