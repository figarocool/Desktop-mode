#!/usr/bin/env python3
"""Write the release metadata consumed by Desktop Mode's updater."""
import hashlib
import json
import re
import sys
from pathlib import Path

if len(sys.argv) != 2 or not re.fullmatch(r"v[0-9A-Za-z][0-9A-Za-z.+_-]*", sys.argv[1]):
    raise SystemExit("usage: create-release-manifest.py vX.Y.Z")

tag = sys.argv[1]
build = Path(__file__).resolve().parent.parent / "native/build"
apps = ["notepad", "counter", "browser", "calculator", "taskmanager", "paint",
        "images", "console", "pdf", "network", "solitaire", "minesweeper", "media"]


def digest(name):
    path = build / name
    if not path.is_file():
        raise SystemExit(f"missing release asset: {path}")
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


manifest = {
    "schema": 1,
    "tag": tag,
    "core": {"asset": "desktop-mode.vpk", "sha256": digest("desktop-mode.vpk")},
    "apps": {app: {"asset": f"{app}.dmapp", "sha256": digest(f"{app}.dmapp")}
             for app in apps},
}
(build / "desktop-mode-update.json").write_text(
    json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
print("desktop-mode-update.json")
