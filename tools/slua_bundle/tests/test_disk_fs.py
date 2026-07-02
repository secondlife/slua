"""DiskFS: real-FS backend smoke tests against pytest's tmp_path."""

import pathlib

import pytest

from slua_bundle import DiskFS, NoResolverError, SourceDecodeError, bundle


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


def test_read_write_utf8_round_trip(tmp_path: pathlib.Path):
    """DiskFS pins utf-8 explicitly; the locale default (cp1252 on Windows)
    must not influence what lands on disk."""
    fs = DiskFS(tmp_path)
    content = 'return "café ☃"\n'
    fs.write(tmp_path / "x.luau", content)
    assert (tmp_path / "x.luau").read_bytes() == content.encode("utf-8")
    assert fs.read(tmp_path / "x.luau") == content


def test_read_invalid_utf8_raises_bundle_error(tmp_path: pathlib.Path):
    """A non-utf-8 source surfaces as a BundleError, not a raw traceback."""
    bad = tmp_path / "bad.luau"
    bad.write_bytes(b'return "\xff\xfe"')
    fs = DiskFS(tmp_path)
    with pytest.raises(SourceDecodeError, match="not valid UTF-8"):
        fs.read(bad)


def test_is_file_requires_exact_case(tmp_path: pathlib.Path):
    """Scripts run under Linux; a wrong-case path must behave as not-found
    even when the host filesystem (macOS, Windows) would open it."""
    _write(tmp_path, "lib/foo.luau", "return 1")
    fs = DiskFS(tmp_path)
    assert fs.is_file(tmp_path / "lib" / "foo.luau")
    assert not fs.is_file(tmp_path / "lib" / "Foo.luau")
    assert not fs.is_file(tmp_path / "Lib" / "foo.luau")


def test_bundle_wrong_case_require_is_not_found(tmp_path: pathlib.Path):
    """require("./lib/Foo") with foo.luau on disk fails resolution on every
    host, matching the Linux runtime."""
    _write(tmp_path, "src/Main.luau", 'require("./lib/Foo")')
    _write(tmp_path, "src/lib/foo.luau", "return 1")
    fs = DiskFS(tmp_path)
    project_root = (tmp_path / "src").resolve()
    with pytest.raises(NoResolverError):
        bundle(fs, project_root, project_root / "Main.luau")


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
