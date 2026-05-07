"""DiskFS: real-FS backend smoke tests against pytest's tmp_path."""

import pathlib

from slua_bundle import DiskFS, bundle


def _write(root: pathlib.Path, rel: str, content: str) -> pathlib.Path:
    p = root / rel
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(content)
    return p


def test_bundle_smoke(tmp_path: pathlib.Path):
    """End-to-end against real disk: bundle a small project. Full-text
    equality also enforces the cross-platform contract that bundle keys are
    POSIX-shaped on every host (the expected text uses forward slashes)."""
    _write(tmp_path, "src/Main.luau", 'require("./lib/foo")')
    _write(tmp_path, "src/lib/foo.luau", 'require("./bar")')
    _write(tmp_path, "src/lib/bar.luau", "return 1")

    fs = DiskFS(tmp_path)
    project_root = (tmp_path / "src").resolve()
    text = bundle(fs, project_root, project_root / "Main.luau", "myhud")

    assert text == """\
-- !!LUABUNDLE:VERSION 1
-- !!LUABUNDLE:PROJECT myhud
-- !!LUABUNDLE:MAIN @root/Main
-- !!LUABUNDLE:BODY
require("./lib/foo")
-- !!LUABUNDLE:MODULE @root/lib/foo
require("./bar")
-- !!LUABUNDLE:MODULE @root/lib/bar
return 1
"""


def test_iter_files_skips_dotfiles_except_luaurc(tmp_path: pathlib.Path):
    """Hidden files (other than .luaurc) and hidden directories are pruned."""
    _write(tmp_path, "src/Main.luau", "return 1")
    _write(tmp_path, ".luaurc", "{}")                        # kept
    _write(tmp_path, ".env", "SECRET=1")                     # skipped
    _write(tmp_path, ".cache/index", "stale")                # whole dir pruned
    _write(tmp_path, ".cache/blobs/blob", "binary-junk")
    _write(tmp_path, "node_modules/.hidden/foo.luau", "x")   # outer dir kept, inner pruned
    _write(tmp_path, "node_modules/visible.luau", "y")       # kept (not hidden)

    fs = DiskFS(tmp_path)
    rels = sorted(str(p.relative_to(tmp_path)) for p in fs.iter_files())
    assert rels == [
        ".luaurc",
        "node_modules/visible.luau",
        "src/Main.luau",
    ]
