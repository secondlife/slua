"""Reverse-engineer a bundle into a self-contained project tree."""

import json
from pathlib import PurePosixPath

import pytest

from slua_bundle import (
    MemoryFS,
    bundle,
    parse_bundle,
)
from slua_bundle.extractor import (
    ExtractClobberError,
    ExtractCollisionError,
    ExtractMissingMainError,
    ExtractUnsafeKeyError,
    extract_to_dir,
    physical_path_for_key,
)
from slua_bundle.runtime import ParsedBundle

OUT = PurePosixPath("/out")


@pytest.fixture
def fs() -> MemoryFS:
    return MemoryFS()


def test_physical_path_root_alias_with_path():
    assert physical_path_for_key("@root/Main", OUT) == PurePosixPath("/out/Main.luau")
    assert physical_path_for_key("@root/lib/foo", OUT) == PurePosixPath("/out/lib/foo.luau")


def test_physical_path_root_alias_bare():
    assert physical_path_for_key("@root", OUT) == PurePosixPath("/out/init.luau")


def test_physical_path_other_alias_with_path():
    assert physical_path_for_key("@SomeLib/util", OUT) == PurePosixPath("/out/SomeLib/util.luau")


def test_physical_path_other_alias_bare():
    assert physical_path_for_key("@SomeLib", OUT) == PurePosixPath("/out/SomeLib/init.luau")


def test_extract_writes_luaurc_for_external_aliases(fs: MemoryFS):
    parsed = ParsedBundle(
        fields={"version": "1", "project": "myhud", "main": "@root/Main"},
        main_source='require("@SomeLib/util")\n',
        modules={"@SomeLib/util": "return {}\n"},
    )
    extract_to_dir(parsed, fs, OUT)
    assert set(fs.iter_files()) == {
        PurePosixPath("/out/Main.luau"),
        PurePosixPath("/out/SomeLib/util.luau"),
        PurePosixPath("/out/.luaurc"),
    }
    luaurc = json.loads(fs.read(OUT / ".luaurc"))
    assert luaurc == {"aliases": {"SomeLib": "SomeLib"}}


def test_extract_no_luaurc_when_only_project_alias(fs: MemoryFS):
    parsed = ParsedBundle(
        fields={"version": "1", "project": "myhud", "main": "@root/Main"},
        main_source='require("./lib/foo")\n',
        modules={"@root/lib/foo": "return 1\n"},
    )
    extract_to_dir(parsed, fs, OUT)
    assert set(fs.iter_files()) == {
        PurePosixPath("/out/Main.luau"),
        PurePosixPath("/out/lib/foo.luau"),
    }


def test_extract_places_modules_at_expected_paths(fs: MemoryFS):
    parsed = ParsedBundle(
        fields={"version": "1", "project": "myhud", "main": "@root/Main"},
        main_source="MAIN\n",
        modules={
            "@root/lib/foo": "FOO\n",
            "@SomeLib/util": "UTIL\n",
            "@SomeLib": "SOMELIB_INIT\n",
        },
    )
    extract_to_dir(parsed, fs, OUT)
    assert {p: fs.read(p) for p in fs.iter_files() if p.name != ".luaurc"} == {
        PurePosixPath("/out/Main.luau"): "MAIN\n",
        PurePosixPath("/out/lib/foo.luau"): "FOO\n",
        PurePosixPath("/out/SomeLib/util.luau"): "UTIL\n",
        PurePosixPath("/out/SomeLib/init.luau"): "SOMELIB_INIT\n",
    }


def test_extract_round_trips_in_memory():
    src = MemoryFS.from_dict({
        "/proj/Main.luau": 'require("./lib/foo")\nreturn 1\n',
        "/proj/lib/foo.luau": 'return "foo"\n',
    })
    original = bundle(src, PurePosixPath("/proj"), PurePosixPath("/proj/Main.luau"), "myhud")

    out = MemoryFS()
    extract_to_dir(parse_bundle(original), out, PurePosixPath("/extracted"))

    rebuilt = bundle(out, PurePosixPath("/extracted"), PurePosixPath("/extracted/Main.luau"), "myhud")
    assert rebuilt == original


def test_extract_round_trips_with_external_alias_in_memory():
    src = MemoryFS.from_dict({
        "/proj/.luaurc": json.dumps({"aliases": {"SomeLib": "Packages/SomeLib"}}),
        "/proj/Main.luau": 'require("@SomeLib/util")\n',
        "/proj/Packages/SomeLib/util.luau": 'return "u"\n',
    })
    original = bundle(src, PurePosixPath("/proj"), PurePosixPath("/proj/Main.luau"), "myhud")

    out = MemoryFS()
    extract_to_dir(parse_bundle(original), out, PurePosixPath("/extracted"))
    assert out.is_file(PurePosixPath("/extracted/SomeLib/util.luau"))
    assert out.is_file(PurePosixPath("/extracted/.luaurc"))

    rebuilt = bundle(out, PurePosixPath("/extracted"), PurePosixPath("/extracted/Main.luau"), "myhud")
    assert rebuilt == original


def test_extract_refuses_collision(fs: MemoryFS):
    """Bare alias key and explicit `init` leaf both target init.luau."""
    parsed = ParsedBundle(
        fields={"version": "1", "project": "myhud", "main": "@root/Main"},
        main_source="\n",
        modules={
            "@root": "return {}\n",          # -> out/init.luau
            "@root/init": "return {}\n",    # -> out/init.luau (collision)
        },
    )
    with pytest.raises(ExtractCollisionError):
        extract_to_dir(parsed, fs, OUT)
    assert list(fs.iter_files()) == []


def test_extract_refuses_to_clobber():
    fs = MemoryFS.from_dict({"/out/Main.luau": "DO NOT TOUCH\n"})
    before = set(fs.iter_files())
    parsed = ParsedBundle(
        fields={"version": "1", "project": "myhud", "main": "@root/Main"},
        main_source='return 1\n',
        modules={"@root/lib/foo": "FOO\n"},
    )
    with pytest.raises(ExtractClobberError):
        extract_to_dir(parsed, fs, OUT)
    assert set(fs.iter_files()) == before
    assert fs.read(OUT / "Main.luau") == "DO NOT TOUCH\n"


def test_extract_rejects_traversal_in_module_key(fs: MemoryFS):
    """A hand-crafted bundle with `..` components in a key must not write
    outside <output>. Our bundler never emits these; this defends against
    malicious or corrupted bundles."""
    parsed = ParsedBundle(
        fields={"version": "1", "project": "myhud", "main": "@root/Main"},
        main_source="\n",
        modules={"@root/../etc/passwd": "owned\n"},
    )
    with pytest.raises(ExtractUnsafeKeyError):
        extract_to_dir(parsed, fs, OUT)
    assert list(fs.iter_files()) == []


def test_extract_rejects_traversal_in_main_key(fs: MemoryFS):
    parsed = ParsedBundle(
        fields={"version": "1", "project": "myhud", "main": "@root/../escape"},
        main_source="\n",
        modules={},
    )
    with pytest.raises(ExtractUnsafeKeyError):
        extract_to_dir(parsed, fs, OUT)
    assert list(fs.iter_files()) == []


def test_extract_rejects_empty_component(fs: MemoryFS):
    """`@root//foo` produces an empty middle component."""
    parsed = ParsedBundle(
        fields={"version": "1", "project": "myhud", "main": "@root/Main"},
        main_source="\n",
        modules={"@root//foo": "x\n"},
    )
    with pytest.raises(ExtractUnsafeKeyError):
        extract_to_dir(parsed, fs, OUT)
    assert list(fs.iter_files()) == []


def test_extract_errors_when_main_directive_missing(fs: MemoryFS):
    parsed = ParsedBundle(
        fields={"version": "1", "project": "myhud"},
        main_source="\n",
        modules={},
    )
    with pytest.raises(ExtractMissingMainError):
        extract_to_dir(parsed, fs, OUT)
    assert list(fs.iter_files()) == []
