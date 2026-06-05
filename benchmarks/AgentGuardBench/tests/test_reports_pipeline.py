import csv
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def run_tool(root: Path, *args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, *args],
        cwd=root,
        text=True,
        capture_output=True,
    )


class IsolatedBenchMixin:
    def setUp(self) -> None:
        self.tmpdir = tempfile.TemporaryDirectory()
        self.work_root = Path(self.tmpdir.name) / "AgentGuardBench"
        shutil.copytree(
            ROOT,
            self.work_root,
            ignore=shutil.ignore_patterns("runs", "__pycache__", "*.pyc", "*.log"),
        )

    def tearDown(self) -> None:
        self.tmpdir.cleanup()


class ReportsPipelineTest(IsolatedBenchMixin, unittest.TestCase):
    def test_verify_all_writes_metrics_csv(self):
        prepare = run_tool(self.work_root, "tools/prepare_run.py", "--task", "T001_state_unit_count", "--mode", "codex_raw")
        self.assertEqual(prepare.returncode, 0, prepare.stderr)

        result = run_tool(self.work_root, "tools/verify_all.py")

        self.assertEqual(result.returncode, 0, result.stderr)
        metrics_path = self.work_root / "reports" / "metrics.csv"
        self.assertTrue(metrics_path.exists())
        with metrics_path.open("r", encoding="utf-8") as handle:
            rows = list(csv.DictReader(handle))
        self.assertGreaterEqual(len(rows), 1)
        self.assertIn("task_id", rows[0])
        self.assertIn("failure_type", rows[0])

    def test_generate_report_writes_summary_md_and_html(self):
        run_tool(self.work_root, "tools/prepare_run.py", "--task", "T001_state_unit_count", "--mode", "codex_raw")
        run_tool(self.work_root, "tools/verify_all.py")

        result = run_tool(self.work_root, "tools/generate_report.py")

        self.assertEqual(result.returncode, 0, result.stderr)
        summary_md = (self.work_root / "reports" / "summary.md").read_text(encoding="utf-8")
        summary_html = (self.work_root / "reports" / "summary.html").read_text(encoding="utf-8")
        self.assertIn("Raw vs Guarded Summary", summary_md)
        self.assertIn("failure_type Distribution", summary_md)
        self.assertIn("<html", summary_html.lower())

    def test_readme_uses_summary_data_or_states_missing(self):
        run_tool(self.work_root, "tools/prepare_run.py", "--task", "T001_state_unit_count", "--mode", "codex_raw")
        run_tool(self.work_root, "tools/verify_all.py")
        run_tool(self.work_root, "tools/generate_report.py")

        result = run_tool(self.work_root, "tools/generate_readme.py")

        self.assertEqual(result.returncode, 0, result.stderr)
        readme = (self.work_root / "README.md").read_text(encoding="utf-8")
        self.assertIn("AgentGuard Bench: Reliability Evaluation for AI Coding Agents on a Real C++ Game Architecture", readme)
        self.assertIn("AI coding agents can modify unrelated files", readme)
        self.assertIn("TaskSpec", readme)


if __name__ == "__main__":
    unittest.main()
