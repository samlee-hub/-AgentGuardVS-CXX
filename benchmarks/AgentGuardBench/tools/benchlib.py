from __future__ import annotations

import hashlib
import json
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


BENCH_ROOT = Path(__file__).resolve().parents[1]
CONFIG_PATH = BENCH_ROOT / "configs" / "project_config.json"

TEXT_EXTENSIONS = {
    ".h",
    ".hpp",
    ".cpp",
    ".c",
    ".cc",
    ".cxx",
    ".cs",
    ".py",
    ".json",
    ".txt",
    ".md",
    ".ini",
    ".sln",
    ".slnx",
    ".vcxproj",
    ".filters",
    ".uproject",
}


def now_utc() -> str:
    return datetime.now(timezone.utc).isoformat()


def normalize_rel(path: str | Path) -> str:
    return Path(path).as_posix().lstrip("./")


def is_tracked_text_file(path: Path) -> bool:
    if path.name.endswith(".Build.cs"):
        return True
    return path.suffix in TEXT_EXTENSIONS


def read_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8-sig") as handle:
        return json.load(handle)


def write_json(path: Path, data: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2, ensure_ascii=False), encoding="utf-8")


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="replace")


def write_text(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load_config() -> dict[str, Any]:
    return read_json(CONFIG_PATH)


def source_repo() -> Path:
    return Path(load_config()["source_repo"])


def task_dir(task_id: str) -> Path:
    return BENCH_ROOT / "tasks" / task_id


def load_task(task_id: str) -> dict[str, Any]:
    return read_json(task_dir(task_id) / "task.json")


def resolve_run_dir(run_value: str | Path) -> Path:
    run_path = Path(run_value)
    if not run_path.is_absolute():
        run_path = BENCH_ROOT / run_path
    return run_path.resolve()


def repo_dir(run_dir: Path) -> Path:
    return run_dir / "repo"


def scan_manifest(repo: Path) -> dict[str, dict[str, Any]]:
    files: dict[str, dict[str, Any]] = {}
    if not repo.exists():
        return files
    for path in sorted(repo.rglob("*")):
        if not path.is_file() or not is_tracked_text_file(path):
            continue
        rel = normalize_rel(path.relative_to(repo))
        files[rel] = {
            "sha256": sha256_file(path),
            "size": path.stat().st_size,
        }
    return files


def load_run_meta(run_dir: Path) -> dict[str, Any]:
    return read_json(run_dir / "run_meta.json")


def markdown_list(values: list[str]) -> str:
    if not values:
        return "- None"
    return "\n".join(f"- `{value}`" for value in values)


def text_snippet(text: str, needle: str, radius: int = 120) -> str:
    lower_text = text.lower()
    lower_needle = needle.lower()
    index = lower_text.find(lower_needle)
    if index < 0:
        return ""
    start = max(0, index - radius)
    end = min(len(text), index + len(needle) + radius)
    return text[start:end].replace("\r", "").strip()
