# RFC 0031 E1 baseline and Linux PSS runbook

The E1 pre-implementation tree is frozen at
`tools/mermaid/rfc0031-baseline.json`. It records:

- the `HEAD` commit (`b8286503`), branch, subject and `git stash create`
  tree hash;
- every modified and untracked file with its SHA-256 (11k+ entries,
  including the pinned Mermaid fixtures);
- the debug ctest registration (`3935` total, `746` Mermaid);
- the PSS status and the exact before/after commands.

PSS is Linux-only: `tools/arch/measure_bmi.py` reads
`/proc/<pid>/smaps_rollup` and exits 3 on macOS. PSS is never replaced by
RSS. The current macOS host has no `/proc` and no saved SSH target, so the
E1 before-PSS cannot be measured here. Do not start E1 code on the macOS
tree until the before-PSS has been captured from this frozen tree, or the
before value is lost.

## 1. Verify the frozen tree on the Linux dev box

Copy/sync the working tree (including the untracked fixtures) and run:

```bash
python3 tools/mermaid/verify_rfc0031_baseline.py
```

It must print `0 mismatch(es)`. Do not measure a tree that fails this check.

## 2. Configure the debug preset

```bash
cmake --preset debug
```

The measurement reads `build/debug/compile_commands.json`; configure first
if it is missing.

## 3. Capture the E1 before-PSS

```bash
tools/arch/measure_bmi.py --runs 3 --json --out tools/mermaid/rfc0031-pss-before.json src/ui/visual/mermaid.cppm
```

Use the same compiler, preset and default Ninja parallelism as the project
build. The tool records producer PSS, reduced-BMI bytes, object bytes and
wall time.

## 4. Capture the E1 after-PSS

After E1's interface declarations land, on the same Linux host:

```bash
tools/arch/measure_bmi.py --runs 3 --json --out tools/mermaid/rfc0031-pss-after.json src/ui/visual/mermaid.cppm
```

## 5. Record the result

Copy both JSON files back into `tools/mermaid/` and add their measured values
to the RFC 0031 Implementation History. E1 does not graduate without both
files and the Linux debug/release `-Werror` + serial ctest evidence.
