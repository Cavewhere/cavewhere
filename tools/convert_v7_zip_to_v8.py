#!/usr/bin/env python3
import argparse
import json
import os
import sys
import tempfile
import zipfile
from pathlib import Path


def sanitize_filename(value: str) -> str:
    forbidden = r'\\/:*?"<>|'
    for ch in forbidden:
        value = value.replace(ch, "_")
    value = value.strip()
    while value.startswith('.') or value.endswith('.'):
        if value.startswith('.'):
            value = value[1:]
        if value.endswith('.'):
            value = value[:-1]
    return value if value else "untitled"


def is_metadata_path(path: str) -> bool:
    parts = Path(path).parts
    if "__MACOSX" in parts:
        return True
    name = Path(path).name
    return name.startswith("._")


def move_project_contents(project_dir: Path, project_name: str, project_file: Path) -> None:
    data_root_dir = project_dir / project_name
    data_root_dir.mkdir(exist_ok=True)

    for entry in project_dir.iterdir():
        if entry == data_root_dir or entry == project_file:
            continue
        if entry.name.startswith("._"):
            continue
        if entry.name == "__MACOSX":
            continue
        if entry.suffix.lower() == ".cwproj":
            continue
        if entry.is_dir():
            has_cwcave = any(p.suffix.lower() == ".cwcave" for p in entry.iterdir() if p.is_file())
            if not has_cwcave and any(p.suffix.lower() == ".cwcave" for p in entry.rglob("*.cwcave")):
                for child in list(entry.iterdir()):
                    target = data_root_dir / child.name
                    if target.exists():
                        continue
                    child.rename(target)
                try:
                    entry.rmdir()
                except OSError:
                    pass
                continue
        target = data_root_dir / entry.name
        if target.exists():
            continue
        entry.rename(target)

    # Flatten a single wrapper directory inside dataRoot if it only contains caves.
    entries = [e for e in data_root_dir.iterdir() if not e.name.startswith("._") and e.name != "__MACOSX"]
    if len(entries) == 1 and entries[0].is_dir():
        wrapper = entries[0]
        has_cwcave = any(p.suffix.lower() == ".cwcave" for p in wrapper.rglob("*.cwcave"))
        if has_cwcave:
            for child in list(wrapper.iterdir()):
                target = data_root_dir / child.name
                if target.exists():
                    continue
                child.rename(target)
            try:
                wrapper.rmdir()
            except OSError:
                pass


def convert_cw_file(cw_path: Path) -> bool:
    try:
        raw = cw_path.read_text(encoding="utf-8")
        data = json.loads(raw)
    except Exception:
        return False

    if data.get("version") != 7:
        return False

    project_dir = cw_path.parent
    name = sanitize_filename(project_dir.name)
    project = {
        "fileVersion": {
            "version": 8,
            "cavewhereVersion": data.get("cavewhereVersion", "")
        },
        "name": name,
        "metadata": {
            "dataRoot": name,
            "gitMode": "ManagedNew",
            "syncEnabled": True
        }
    }

    output_path = project_dir / f"{name}.cwproj"
    output_path.write_text(json.dumps(project, indent=1) + "\n", encoding="utf-8")
    cw_path.unlink()

    move_project_contents(project_dir, name, output_path)
    return True


def normalize_cwproj_file(cwproj_path: Path) -> bool:
    try:
        raw = cwproj_path.read_text(encoding="utf-8")
        data = json.loads(raw)
    except Exception:
        return False

    file_version = data.get("fileVersion", {}).get("version")
    if file_version != 8:
        return False

    project_dir = cwproj_path.parent
    name = sanitize_filename(project_dir.name)
    data["name"] = name
    metadata = data.setdefault("metadata", {})
    metadata["dataRoot"] = name
    metadata.setdefault("gitMode", "ManagedNew")
    metadata.setdefault("syncEnabled", True)

    target_path = project_dir / f"{name}.cwproj"
    target_path.write_text(json.dumps(data, indent=1) + "\n", encoding="utf-8")
    if cwproj_path != target_path:
        cwproj_path.unlink()
    move_project_contents(project_dir, name, target_path)
    return True


def rezip_directory(source_dir: Path, zip_path: Path) -> None:
    with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        for root, _, files in os.walk(source_dir):
            for filename in files:
                abs_path = Path(root) / filename
                rel_path = abs_path.relative_to(source_dir)
                if is_metadata_path(str(rel_path)):
                    continue
                zf.write(abs_path, rel_path)


def convert_zip(zip_path: Path) -> int:
    if not zip_path.exists():
        raise FileNotFoundError(zip_path)

    converted = 0
    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)
        with zipfile.ZipFile(zip_path) as zf:
            zf.extractall(temp_path)

        for root, _, files in os.walk(temp_path):
            for filename in files:
                file_path = Path(root) / filename
                if is_metadata_path(str(file_path)):
                    continue
                if file_path.suffix.lower() == ".cw":
                    if convert_cw_file(file_path):
                        converted += 1
                elif file_path.suffix.lower() == ".cwproj":
                    if normalize_cwproj_file(file_path):
                        converted += 1

        rezip_directory(temp_path, zip_path)

    return converted


def main() -> int:
    parser = argparse.ArgumentParser(description="Convert v7 .cw project files inside zips to v8 .cwproj.")
    parser.add_argument("zips", nargs="+", help="Zip files to convert in place")
    args = parser.parse_args()

    exit_code = 0
    for zip_name in args.zips:
        zip_path = Path(zip_name)
        try:
            converted = convert_zip(zip_path)
        except Exception as exc:
            print(f"error: {zip_path}: {exc}")
            exit_code = 1
            continue

        if converted == 0:
            print(f"warning: {zip_path}: no v7 .cw files converted")
        else:
            print(f"converted: {zip_path}: {converted} file(s)")

    return exit_code


if __name__ == "__main__":
    sys.exit(main())
