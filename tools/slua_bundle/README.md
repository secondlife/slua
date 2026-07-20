# slua-bundle

Reference implementation of the SLua static-require-bundle algorithm. Bundle a Luau project rooted at a `MAIN` module into a single text artifact, inspect bundle artifacts, and extract bundles back into a self-contained project tree.

For the format spec and design rationale, see [`rfcs/static-require-bundle.md`](../../rfcs/static-require-bundle.md) at the repo root.

## Install

Requires Python 3.7+.

```
pip install ./tools/slua_bundle
```

Or, for development:

```
pip install -e ./tools/slua_bundle
```

Either form puts a `slua-bundle` command on `$PATH`.

## CLI

### `slua-bundle bundle`

Bundle a project tree into a single text artifact. Project-internal modules canonicalize under the built-in `@root` alias. `--project` is an optional advisory project name emitted as the bundle's `PROJECT` directive (for viewer-side linkage to a disk project); it does not affect canonicalization. Before writing, the produced bundle is self-checked with the simulate walker: every require must resolve through the bundle alone, and cycles are surfaced.

```
slua-bundle bundle --root ./src ./src/Main.luau -o bundle.lua
slua-bundle bundle --root ./src --project myhud ./src/Main.luau -o bundle.lua
```

Omit `-o` to write to stdout.

Pass `--input-bundle PATH` to use an existing bundle as a last-resort resolver: when a require's source is missing on disk (or its alias isn't declared in `.luaurc`), the embedded copy from the prior bundle is used instead. Disk wins when both are available. This is the rebundle-without-source flow - useful when a CLI receives a viewer-built bundle and needs to rebuild it without access to the original resolvers (inventory, etc.).

### `slua-bundle inspect`

Show a bundle's structure: format version, advisory project name (if any), MAIN entry point, ALIAS prefix remappings (if any), and a per-module byte breakdown sorted by canonical key.

```
slua-bundle inspect bundle.lua
```

### `slua-bundle extract`

Reverse a bundle into a project tree. `@root` modules land flat under `<output>/`, non-root aliases under `<output>/<alias>/` (nested-alias `ALIAS` lines pin an alias's directory inside another's). A `.luaurc` declaring every alias -- including extra names from `ALIAS` lines -- is generated when needed. Re-bundling the extracted tree reproduces the original bundle byte-for-byte.

```
slua-bundle extract -o ./extracted bundle.lua
```

## Library

```python
from pathlib import Path
from slua_bundle import DiskFS, bundle, parse_bundle
from slua_bundle.extractor import extract_to_dir

fs = DiskFS(Path("./src"))
text = bundle(fs, project_root=Path("./src").resolve(),
              main_path=Path("./src/Main.luau").resolve())
# Optionally pass project_name="myhud" to emit a PROJECT directive.

parsed = parse_bundle(text)
extract_to_dir(parsed, DiskFS(Path("./extracted")), Path("./extracted").resolve())
```

`MemoryFS` is also available for dict-backed in-memory use, mirroring `DiskFS`.

## Development

```
pip install -e ./tools/slua_bundle
cd tools/slua_bundle
python -m pytest
```
