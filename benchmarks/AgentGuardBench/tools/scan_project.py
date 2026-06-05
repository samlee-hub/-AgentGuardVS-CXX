#!/usr/bin/env python3
"""Scan the source project and write a reproducible inventory report."""

from __future__ import annotations

import argparse
import json
import os
from collections import Counter
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


KEY_EXTENSIONS = {
    ".sln",
    ".slnx",
    ".vcxproj",
    ".filters",
    ".h",
    ".hpp",
    ".cpp",
    ".c",
    ".cc",
    ".cxx",
    ".cs",
    ".py",
    ".txt",
    ".json",
    ".ini",
    ".uproject",
    ".md",
}

SKIP_DIRS = {
    ".git",
    ".vs",
    "x64",
    "Debug",
    "Release",
    "Binaries",
    "Intermediate",
    "Saved",
    "DerivedDataCache",
    "__pycache__",
    ".idea",
    ".vscode",
}


def normalize(path: Path) -> str:
    return path.as_posix()


def is_key_file(path: Path) -> bool:
    if path.name.endswith(".Build.cs"):
        return True
    return path.suffix in KEY_EXTENSIONS


def load_config(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8-sig") as handle:
        return json.load(handle)


def relative_to_source(path: Path, source_repo: Path) -> str:
    return normalize(path.relative_to(source_repo))


def scan_project(source_repo: Path) -> dict[str, Any]:
    source_repo = source_repo.resolve()
    inventory: dict[str, Any] = {
        "source_repo": str(source_repo),
        "scanned_at": datetime.now(timezone.utc).isoformat(),
        "solutions": [],
        "slnx": [],
        "vcxproj": [],
        "headers": [],
        "sources": [],
        "python_files": [],
        "config_files": [],
        "ue_files": [],
        "server_files": [],
        "client_files": [],
        "all_key_files": [],
        "extension_counts": {},
        "skipped_directories": [],
        "errors": [],
    }

    if not source_repo.exists() or not source_repo.is_dir():
        inventory["errors"].append(f"source_repo does not exist or is not a directory: {source_repo}")
        return inventory

    try:
        for root, dirs, files in os.walk(source_repo):
            root_path = Path(root)
            skipped = sorted(name for name in dirs if name in SKIP_DIRS)
            for name in skipped:
                inventory["skipped_directories"].append(relative_to_source(root_path / name, source_repo))
            dirs[:] = [name for name in dirs if name not in SKIP_DIRS]

            for file_name in files:
                item = root_path / file_name
                try:
                    if not item.is_file() or not is_key_file(item):
                        continue

                    rel = relative_to_source(item, source_repo)
                    inventory["all_key_files"].append(rel)

                    lower_rel = rel.lower()
                    suffix = item.suffix.lower()
                    if suffix == ".sln":
                        inventory["solutions"].append(rel)
                    elif suffix == ".slnx":
                        inventory["slnx"].append(rel)
                    elif suffix == ".vcxproj":
                        inventory["vcxproj"].append(rel)
                    elif suffix in {".h", ".hpp"}:
                        inventory["headers"].append(rel)
                    elif suffix in {".cpp", ".c", ".cc", ".cxx"}:
                        inventory["sources"].append(rel)
                    elif suffix == ".py":
                        inventory["python_files"].append(rel)
                    elif item.name.endswith(".Build.cs") or suffix in {".cs", ".uproject"}:
                        inventory["ue_files"].append(rel)
                    elif suffix in {".txt", ".json", ".ini", ".md", ".filters"}:
                        inventory["config_files"].append(rel)

                    if "server" in lower_rel:
                        inventory["server_files"].append(rel)
                    if "client" in lower_rel:
                        inventory["client_files"].append(rel)
                    if "ue_" in lower_rel or "myproject" in lower_rel or item.name.endswith(".Build.cs") or suffix == ".uproject":
                        if rel not in inventory["ue_files"]:
                            inventory["ue_files"].append(rel)
                except (OSError, UnicodeError) as error:
                    inventory["errors"].append(f"{item}: {error}")
    except OSError as error:
        inventory["errors"].append(str(error))

    for key, value in list(inventory.items()):
        if isinstance(value, list):
            inventory[key] = sorted(set(value))

    inventory["extension_counts"] = dict(
        sorted(Counter(Path(p).suffix or Path(p).name for p in inventory["all_key_files"]).items())
    )
    return inventory


def write_list(lines: list[str], title: str, values: list[str]) -> None:
    lines.append(f"## {title}")
    lines.append("")
    if not values:
        lines.append("- None detected")
    else:
        for value in values:
            lines.append(f"- `{value}`")
    lines.append("")


def write_markdown_report(inventory: dict[str, Any], output_path: Path) -> None:
    lines: list[str] = [
        "# Project Inventory",
        "",
        f"- Source repo: `{inventory['source_repo']}`",
        f"- Scanned at: `{inventory['scanned_at']}`",
        f"- Key files detected: {len(inventory['all_key_files'])}",
        "",
    ]

    if not inventory["solutions"]:
        lines.append("> No `.sln` files were detected under the scanned source tree.")
        lines.append("")
    if inventory["slnx"]:
        lines.append("> `.slnx` files were detected separately from `.sln`; current MSBuild tooling may need explicit support.")
        lines.append("")

    write_list(lines, "Detected .sln Files", inventory["solutions"])
    write_list(lines, "Detected .slnx Files", inventory["slnx"])
    write_list(lines, "Detected Visual Studio C++ Projects", inventory["vcxproj"])
    write_list(lines, "Server-Related Key Files", inventory["server_files"])
    write_list(lines, "Client-Related Key Files", inventory["client_files"])
    write_list(lines, "UE-Related Key Files", inventory["ue_files"])

    lines.append("## Extension Counts")
    lines.append("")
    if inventory["extension_counts"]:
        lines.append("| Extension | Count |")
        lines.append("| --- | ---: |")
        for extension, count in inventory["extension_counts"].items():
            lines.append(f"| `{extension}` | {count} |")
    else:
        lines.append("- None")
    lines.append("")

    write_list(lines, "Skipped Directories", inventory["skipped_directories"])
    write_list(lines, "Scan Errors", inventory["errors"])

    lines.append("## All Key Files")
    lines.append("")
    if inventory["all_key_files"]:
        for value in inventory["all_key_files"]:
            lines.append(f"- `{value}`")
    else:
        lines.append("- None detected")
    lines.append("")

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text("\n".join(lines), encoding="utf-8")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Scan a Visual Studio C++ game project for benchmark inventory.")
    parser.add_argument("--config", default="configs/project_config.json")
    parser.add_argument("--source", default="")
    parser.add_argument("--output", default="reports/project_inventory.md")
    parser.add_argument("--json-output", default="reports/project_inventory.json")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    config_path = Path(args.config)
    source = Path(args.source) if args.source else Path(load_config(config_path)["source_repo"])
    inventory = scan_project(source)

    output = Path(args.output)
    json_output = Path(args.json_output)
    write_markdown_report(inventory, output)
    json_output.parent.mkdir(parents=True, exist_ok=True)
    json_output.write_text(json.dumps(inventory, indent=2, ensure_ascii=False), encoding="utf-8")

    print(json.dumps({
        "ok": True,
        "source_repo": inventory["source_repo"],
        "solutions": inventory["solutions"],
        "vcxproj_count": len(inventory["vcxproj"]),
        "key_file_count": len(inventory["all_key_files"]),
        "report": str(output),
        "json_report": str(json_output),
        "error_count": len(inventory["errors"]),
    }, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
