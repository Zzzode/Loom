# Reviewed Merman terminal-test data import

Pinned Merman v0.9.1, commit `f6202f702dbde0a75049b1a62b3124feae709b3d`.
MIT OR Apache-2.0 and original third-party notices are retained under upstream/.
The parent import and native baseline are documented in
[the terminal test guide](../../../../docs/dev/mermaid-terminal-tests.md).

`cases.json` is the reviewed merge of 100 original inputs and 1,532 additional
runtime-observed exact inputs. No backend implementation is compiled or imported.
The original source tests, conditionals, options, models, references and negative
probes remain evidence; native semantic assertion translation is still pending
outside the separately reviewed first flowchart batch.

`README.md`, `promotion-inventory.json` and `runtime-capture/PROMOTION.md`
are immutable historical candidate artifacts. Independent agent:/root/corpus_oracle
approved their asset promotion after verifying all 4,526 runtime asset hashes,
678 original assertion byte slices and 7,483 parse/render correlation events.
Twenty-one original upstream suites passed 1,376 tests, with zero failures.
This does not certify Loom behavior. Corrected output classifications and all
remaining semantic obligations are specified in `runtime-capture/PROMOTION.md`.

The exact maintenance merge inputs are retained. Reproduce the candidate in a
scratch directory (never overwrite the vendored inventory automatically):

```sh
python3 tests/fixtures/mermaid_terminal/merman/maintenance/merge_runtime_cases.py \
  --base tests/fixtures/mermaid_terminal/merman/legacy-cases.json \
  --runtime tests/fixtures/mermaid_terminal/merman/runtime-capture/runtime-cases.json \
  --capabilities tests/fixtures/mermaid_terminal/merman/runtime-capture/full-public-capabilities.json \
  --out build/merman-import-reproduction
```

The script verifies three literal SHA-256 input anchors. Its produced cases must
have SHA-256 `89ce2ef6ad779481285ce61b771a64fd059b17a824eb2c292934ff13393562db`.
The parent `inventory.lock.json`, separately pinned by native tests, is the
current complete asset authority. Historical handoff and candidate locks retain
their original scopes and are not substituted for that root lock.
