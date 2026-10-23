#!/usr/bin/env python3
"""Restore the pinned official fixtures from a reviewed local Git checkout.

This maintenance command does not generate expectations, update snapshots or
fetch anything. The native CTest suite owns integrity and capability results.
"""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess


COMMIT = "1fb7869866dd619168d840291c9c54b4f129758b"
TAG = "mermaid@12.2.0"
TAG_OBJECT = "ee1fdbcebe057cae346232efb6df066215565bec"
TREE = "58f6617da68d7f2b8a58161175db5db5a971cd82"
UPSTREAM_ROOT = "e2e/diagrams"
FIXTURE_ROOT = Path(__file__).resolve().parents[2] / "tests/fixtures/mermaid_official"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--upstream", type=Path, required=True,
                        help="clean local Mermaid checkout at the pinned tag")
    args = parser.parse_args()
    upstream = args.upstream.resolve()

    def git(*arguments):
        return subprocess.check_output(["git", "-C", str(upstream), *arguments])

    assert git("rev-parse", "HEAD").decode().strip() == COMMIT, "wrong checkout commit"
    assert git("rev-parse", f"refs/tags/{TAG}").decode().strip() == TAG_OBJECT, "wrong tag object"
    assert git("rev-parse", f"HEAD:{UPSTREAM_ROOT}").decode().strip() == TREE, "wrong fixture tree"
    assert not git("status", "--porcelain", "--untracked-files=no", "--", UPSTREAM_ROOT, "LICENSE"), "dirty upstream fixture checkout"

    manifest = json.loads((FIXTURE_ROOT / "manifest.json").read_text())
    assert manifest["upstream"]["commit"] == COMMIT
    expected = {case["path"]: case for case in manifest["fixtures"]}
    assert len(expected) == len(manifest["fixtures"]) == 2145
    actual = {}
    for record in git("ls-tree", "-r", "-z", "HEAD", "--", UPSTREAM_ROOT).split(b"\0"):
        if not record:
            continue
        metadata, path = record.split(b"\t", 1)
        decoded = path.decode()
        if not decoded.endswith(".mmd"):
            continue
        relative = decoded.removeprefix(UPSTREAM_ROOT + "/")
        mode, kind, blob = metadata.decode().split()
        assert mode == "100644" and kind == "blob", decoded
        actual[relative] = blob
    assert actual.keys() == expected.keys(), "upstream inventory differs from the reviewed lock"

    copies = []
    for relative, case in expected.items():
        path = Path(relative)
        assert not path.is_absolute() and ".." not in path.parts
        assert actual[relative] == case["git_blob"], relative
        source = upstream / UPSTREAM_ROOT / path
        assert not source.is_symlink() and source.is_file(), relative
        data = source.read_bytes()
        assert len(data) == case["bytes"], relative
        assert hashlib.sha256(data).hexdigest() == case["sha256"], relative
        copies.append((FIXTURE_ROOT / "cases" / path, data))

    license_data = (upstream / "LICENSE").read_bytes()
    assert hashlib.sha256(license_data).hexdigest() == manifest["upstream"]["license_sha256"]
    copies.append((FIXTURE_ROOT / "LICENSE", license_data))
    # Validate every input before changing any vendored fixture.
    for destination, data in copies:
        assert not destination.is_symlink(), str(destination)
        destination.parent.mkdir(parents=True, exist_ok=True)
        if not destination.exists() or destination.read_bytes() != data:
            destination.write_bytes(data)
    print(f"Imported {len(expected)} unchanged .mmd files and MIT license from {TAG} ({COMMIT}).")
    print("Expectations and terminal snapshots were not changed; run the native MermaidOfficial tests.")


if __name__ == "__main__":
    main()
