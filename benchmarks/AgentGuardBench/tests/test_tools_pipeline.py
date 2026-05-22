import json
import shutil
import subprocess
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def run_tool(*args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, *args],
        cwd=ROOT,
        text=True,
        capture_output=True,
    )


def read_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


class ToolsPipelineTest(unittest.TestCase):
    def setUp(self) -> None:
        self.run_root = ROOT / "runs" / "T001_state_unit_count"
        if self.run_root.exists():
            shutil.rmtree(self.run_root)

    def test_prepare_run_copies_repo_and_writes_manifest_and_prompt(self):
        result = run_tool("tools/prepare_run.py", "--task", "T001_state_unit_count", "--mode", "codex_guarded")

        self.assertEqual(result.returncode, 0, result.stderr)
        run_dir = self.run_root / "codex_guarded"
        self.assertTrue((run_dir / "repo" / "Server1" / "Server1" / "Server" / "StrategyBattleMode.cpp").exists())
        self.assertTrue((run_dir / "run_meta.json").exists())
        self.assertTrue((run_dir / "baseline_manifest.json").exists())
        self.assertIn("Allowed files:", (run_dir / "prompt.md").read_text(encoding="utf-8"))
        manifest = read_json(run_dir / "baseline_manifest.json")
        self.assertIn("Server1/Server1/Server/StrategyBattleMode.cpp", manifest["files"])
        self.assertFalse((run_dir / "repo" / "AgentGuardBench").exists())
        self.assertFalse((run_dir / "repo" / "Server1" / ".vs").exists())

    def test_check_scope_detects_allowed_and_out_of_scope_changes(self):
        prepare = run_tool("tools/prepare_run.py", "--task", "T001_state_unit_count", "--mode", "codex_raw")
        self.assertEqual(prepare.returncode, 0, prepare.stderr)
        run_dir = self.run_root / "codex_raw"
        allowed_file = run_dir / "repo" / "Client1" / "Client1" / "Client" / "Client.cpp"
        allowed_file.write_text(allowed_file.read_text(encoding="utf-8") + "\n// benchmark test edit\n", encoding="utf-8")
        added_file = run_dir / "repo" / "Client1" / "Client1" / "Client" / "OutOfScope.cpp"
        added_file.write_text("// added out of scope\n", encoding="utf-8")

        result = run_tool("tools/check_scope.py", "--run", str(run_dir))

        self.assertEqual(result.returncode, 0, result.stderr)
        scope = read_json(run_dir / "scope_result.json")
        self.assertFalse(scope["scope_pass"])
        self.assertIn("Client1/Client1/Client/Client.cpp", scope["allowed_modified_files"])
        self.assertIn("Client1/Client1/Client/OutOfScope.cpp", scope["added_files"])
        self.assertGreaterEqual(scope["violation_count"], 1)
        self.assertTrue((run_dir / "scope_report.md").exists())

    def test_semantic_classify_and_verify_outputs_are_machine_readable(self):
        prepare = run_tool("tools/prepare_run.py", "--task", "T002_summon_global_cooldown", "--mode", "codex_guarded")
        self.assertEqual(prepare.returncode, 0, prepare.stderr)
        run_dir = ROOT / "runs" / "T002_summon_global_cooldown" / "codex_guarded"

        semantic = run_tool("tools/semantic_check.py", "--run", str(run_dir))
        self.assertEqual(semantic.returncode, 0, semantic.stderr)
        semantic_result = read_json(run_dir / "semantic_result.json")
        self.assertIn("summon_global_cooldown_exists", semantic_result["checks"])

        verify = run_tool("tools/verify_run.py", "--run", str(run_dir))
        self.assertEqual(verify.returncode, 0, verify.stderr)
        verify_result = read_json(run_dir / "verify_result.json")
        self.assertEqual(verify_result["task_id"], "T002_summon_global_cooldown")
        self.assertIn("failure_type", verify_result)
        self.assertTrue((run_dir / "report.md").exists())


if __name__ == "__main__":
    unittest.main()
