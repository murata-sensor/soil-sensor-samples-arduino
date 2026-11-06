#!/usr/bin/env python3
"""Validate public release metadata, examples, and local documentation links."""

from __future__ import annotations

import json
import os
import re
import subprocess
import sys
from fnmatch import fnmatch
from pathlib import Path
from urllib.parse import unquote

ROOT = Path(__file__).resolve().parent.parent
EXPECTED_REPOSITORY = "https://github.com/murata-sensor/soil-sensor-samples-arduino"
SEMVER = re.compile(
    r"(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)$"
)
MARKDOWN_LINK = re.compile(r"!?\[[^\]]*\]\(([^)]+)\)")
# Assemble exact prohibited wording so this validator is itself publishable.
FORBIDDEN_PUBLIC_TEXT = {
    "dated verification note": re.compile(
        r"(?:checked\s+\d{4}-\d{2}-\d{2}|\d{4}-\d{2}-\d{2}確認)",
        re.IGNORECASE,
    ),
    "ambiguous safety-validation claim": re.compile("安全確認" + "済みの評価回路"),
    "untraceable hardware-validation claim": re.compile(
        r"(?:hardware-" + r"tested|動作確認" + r"済みPython)", re.IGNORECASE
    ),
    "self-assessed quality label": re.compile("Public-" + "quality", re.IGNORECASE),
    "internal publishing process": re.compile(
        "reviewed one-way " + "publisher", re.IGNORECASE
    ),
    "private test-material reference": re.compile(
        "private pre-" + "release", re.IGNORECASE
    ),
}


def tracked_files() -> set[str]:
    completed = subprocess.run(
        ["git", "-C", str(ROOT), "ls-files", "-z", "--cached", "--"],
        check=True,
        stdout=subprocess.PIPE,
    )
    return {
        Path(os.fsdecode(raw)).as_posix()
        for raw in completed.stdout.split(b"\0")
        if raw
    }


def parse_properties(path: Path, errors: list[str]) -> dict[str, str]:
    values: dict[str, str] = {}
    for line_number, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if "=" not in line:
            errors.append(f"{path.name}:{line_number}: expected key=value")
            continue
        key, value = line.split("=", 1)
        if key in values:
            errors.append(f"{path.name}:{line_number}: duplicate key {key!r}")
        values[key] = value
    return values


def validate_metadata(errors: list[str], tracked: set[str]) -> None:
    properties = parse_properties(ROOT / "library.properties", errors)
    required = {
        "name": "Murata Soil Sensor",
        "author": "Murata Manufacturing Co., Ltd.",
        "maintainer": "Murata Manufacturing Co., Ltd.",
        "category": "Sensors",
        "url": EXPECTED_REPOSITORY,
        "architectures": "avr,esp32",
        "includes": "SLT5006.h",
        "license": "BSD-3-Clause",
    }
    for key, expected in required.items():
        if properties.get(key) != expected:
            errors.append(
                f"library.properties: {key} must be {expected!r}, "
                f"got {properties.get(key)!r}"
            )

    expected_dependencies = (
        "M5Unified (>=0.2.17 && <0.3.0), "
        "ArduinoJson (>=7.4.2 && <8.0.0)"
    )
    if properties.get("depends") != expected_dependencies:
        errors.append(
            "library.properties: depends must match the validated dependency ranges"
        )

    version = properties.get("version", "")
    if not SEMVER.fullmatch(version):
        errors.append(f"library.properties: invalid SemVer version {version!r}")

    manifest = json.loads((ROOT / "library.json").read_text(encoding="utf-8"))
    comparisons = {
        "name": required["name"],
        "version": version,
        "homepage": EXPECTED_REPOSITORY,
        "license": "BSD-3-Clause",
        "frameworks": "arduino",
        "headers": "SLT5006.h",
    }
    for key, expected in comparisons.items():
        if manifest.get(key) != expected:
            errors.append(
                f"library.json: {key} must be {expected!r}, got {manifest.get(key)!r}"
            )
    if manifest.get("platforms") != ["atmelavr", "espressif32"]:
        errors.append(
            "library.json: platforms must be ['atmelavr', 'espressif32']"
        )
    expected_git = EXPECTED_REPOSITORY + ".git"
    if manifest.get("repository") != {"type": "git", "url": expected_git}:
        errors.append(f"library.json: repository must point to {expected_git}")

    examples = manifest.get("examples")
    if not isinstance(examples, list) or not examples:
        errors.append("library.json: examples must be a non-empty list")
    else:
        for example in examples:
            if not isinstance(example, dict):
                errors.append("library.json: every example must be an object")
                continue
            base = example.get("base")
            files = example.get("files")
            if not isinstance(base, str) or not isinstance(files, list) or not files:
                errors.append(f"library.json: malformed example entry {example!r}")
                continue
            for filename in files:
                relative = (Path(base) / filename).as_posix()
                candidate = ROOT / relative
                if not candidate.is_file():
                    errors.append(f"library.json: missing example file {relative}")
                elif relative not in tracked:
                    errors.append(
                        f"library.json: example file is not tracked: {relative}"
                    )

    platformio = (ROOT / "platformio.ini").read_text(encoding="utf-8")
    checks = (
        "platform = atmelavr@5.3.0",
        "platform = espressif32@6.12.0",
        "m5stack/M5Unified@^0.2.17",
        "bblanchon/ArduinoJson@^7.4.2",
        "[env:uno_basic]",
        "[env:uno_cooperative_polling]",
        "[env:core2_quick_start]",
        "[env:core2_cloud_logger]",
    )
    for expected in checks:
        if expected not in platformio:
            errors.append(f"platformio.ini: missing validated setting {expected!r}")


def validate_security_choices(errors: list[str]) -> None:
    logger = (ROOT / "examples/M5Core2CloudLogger/M5Core2CloudLogger.ino").read_text(
        encoding="utf-8"
    )
    config = (ROOT / "examples/M5Core2CloudLogger/LoggerConfig.h").read_text(
        encoding="utf-8"
    )
    certificate = (ROOT / "examples/M5Core2CloudLogger/GoogleRootCA.h").read_text(
        encoding="utf-8"
    )
    combined = logger + config
    if "setInsecure" in combined:
        errors.append("cloud logger must not disable TLS certificate validation")
    if "client.setCACert(GOOGLE_ROOT_CA)" not in logger:
        errors.append("cloud logger must install its reviewed TLS trust anchor")
    if 'startsWith("https://")' not in config:
        errors.append("logger config must reject non-HTTPS GAS URLs")
    if "-----BEGIN CERTIFICATE-----" not in certificate:
        errors.append("GoogleRootCA.h does not contain a PEM certificate")
    if "CloudUpload::responseAccepted(status, responseSaysOk(response))" not in logger:
        errors.append("cloud logger must retain queued rows unless upload is accepted")


def iter_markdown_files() -> list[Path]:
    top_level = [
        ROOT / "README.md",
        ROOT / "README.ja.md",
        ROOT / "CONTRIBUTING.md",
        ROOT / "SECURITY.md",
    ]
    return [path for path in [*top_level, *(ROOT / "docs").rglob("*.md")] if path.exists()]


def validate_local_links(errors: list[str]) -> None:
    for document in iter_markdown_files():
        text = document.read_text(encoding="utf-8")
        for match in MARKDOWN_LINK.finditer(text):
            raw_target = match.group(1).strip().strip("<>")
            target = raw_target.split(maxsplit=1)[0]
            if not target or target.startswith(("#", "http://", "https://", "mailto:")):
                continue
            relative = unquote(target.split("#", 1)[0])
            candidate = (document.parent / relative).resolve()
            try:
                candidate.relative_to(ROOT)
            except ValueError:
                errors.append(
                    f"{document.relative_to(ROOT)}: link leaves repository: {raw_target}"
                )
                continue
            if not candidate.exists():
                errors.append(
                    f"{document.relative_to(ROOT)}: broken local link: {raw_target}"
                )


def validate_exported_links(errors: list[str], tracked: set[str]) -> None:
    manifest = json.loads((ROOT / "library.json").read_text(encoding="utf-8"))
    patterns = manifest.get("export", {}).get("include", [])
    if not isinstance(patterns, list) or not patterns:
        errors.append("library.json: export.include must be a non-empty list")
        return

    exported: set[str] = set()
    for relative in tracked:
        if (ROOT / relative).is_file() and any(
            fnmatch(relative, pattern) for pattern in patterns
        ):
            exported.add(relative)
    for relative in sorted(exported):
        if not relative.endswith(".md"):
            continue
        document = ROOT / relative
        for match in MARKDOWN_LINK.finditer(document.read_text(encoding="utf-8")):
            raw_target = match.group(1).strip().strip("<>")
            target = raw_target.split(maxsplit=1)[0]
            if not target or target.startswith(("#", "http://", "https://", "mailto:")):
                continue
            linked = (document.parent / unquote(target.split("#", 1)[0])).resolve()
            try:
                linked_relative = linked.relative_to(ROOT).as_posix()
            except ValueError:
                continue
            if linked_relative not in exported:
                errors.append(
                    f"library.json export: {relative} links to omitted file "
                    f"{linked_relative}"
                )


def validate_public_wording(errors: list[str]) -> None:
    files = [
        *iter_markdown_files(),
        *(ROOT / "examples").rglob("*.ino"),
        *(ROOT / "examples").rglob("*.h"),
    ]
    for document in files:
        text = document.read_text(encoding="utf-8")
        for label, pattern in FORBIDDEN_PUBLIC_TEXT.items():
            if pattern.search(text):
                errors.append(f"{document.relative_to(ROOT)}: contains {label}")


def validate_keywords(errors: list[str]) -> None:
    path = ROOT / "keywords.txt"
    for line_number, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if not raw.strip() or raw.startswith("#"):
            continue
        fields = raw.split("\t")
        if len(fields) != 2 or fields[1] not in {"KEYWORD1", "KEYWORD2", "LITERAL1", "LITERAL2"}:
            errors.append(f"keywords.txt:{line_number}: invalid Arduino keyword entry")


def main() -> int:
    errors: list[str] = []
    try:
        tracked = tracked_files()
        validate_metadata(errors, tracked)
        validate_security_choices(errors)
        validate_local_links(errors)
        validate_exported_links(errors, tracked)
        validate_public_wording(errors)
        validate_keywords(errors)
    except (OSError, UnicodeError, json.JSONDecodeError, subprocess.CalledProcessError) as exc:
        errors.append(str(exc))

    if errors:
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        print(f"Release validation failed with {len(errors)} error(s).", file=sys.stderr)
        return 1

    print("Release metadata, examples, repository links, and package links are valid.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
