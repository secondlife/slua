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
    ExtractAliasError,
    ExtractAmbiguityError,
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
    assert out.is_file(PurePosixPath("/extracted/somelib/util.luau"))
    assert out.is_file(PurePosixPath("/extracted/.luaurc"))

    rebuilt = bundle(out, PurePosixPath("/extracted"), PurePosixPath("/extracted/Main.luau"), "myhud")
    assert rebuilt == original


def test_extract_root_shadow_alias_round_trips():
    """A bare `ALIAS @myproj @root` extracts as the .luaurc entry
    {"myproj": "."} and re-bundles byte-for-byte."""
    src = MemoryFS.from_dict({
        "/proj/.luaurc": json.dumps({"aliases": {"myproj": "."}}),
        "/proj/Main.luau": 'require("@myproj/util")\n',
        "/proj/util.luau": "return {}\n",
    })
    original = bundle(src, PurePosixPath("/proj"), PurePosixPath("/proj/Main.luau"))
    assert "-- !!LUABUNDLE:ALIAS @myproj @root" in original

    out = MemoryFS()
    extract_to_dir(parse_bundle(original), out, PurePosixPath("/x"))
    luaurc = json.loads(out.read(PurePosixPath("/x/.luaurc")))
    assert luaurc == {"aliases": {"myproj": "."}}

    rebuilt = bundle(out, PurePosixPath("/x"), PurePosixPath("/x/Main.luau"))
    assert rebuilt == original


def test_extract_losing_alias_round_trips():
    """`ALIAS @zeta @alpha` becomes a second .luaurc name for alpha's dir;
    re-bundling re-derives the same mapping (with the same collision
    warning the original bundle produced)."""
    src = MemoryFS.from_dict({
        "/proj/.luaurc": json.dumps({"aliases": {"zeta": "/elsewhere", "alpha": "/elsewhere"}}),
        "/proj/Main.luau": 'require("@zeta/util")\n',
        "/elsewhere/util.luau": "return {}\n",
    })
    with pytest.warns(UserWarning):
        original = bundle(src, PurePosixPath("/proj"), PurePosixPath("/proj/Main.luau"))
    assert "-- !!LUABUNDLE:ALIAS @zeta @alpha" in original

    out = MemoryFS()
    extract_to_dir(parse_bundle(original), out, PurePosixPath("/x"))
    luaurc = json.loads(out.read(PurePosixPath("/x/.luaurc")))
    assert luaurc == {"aliases": {"alpha": "alpha", "zeta": "alpha"}}
    assert out.is_file(PurePosixPath("/x/alpha/util.luau"))

    with pytest.warns(UserWarning):
        rebuilt = bundle(out, PurePosixPath("/x"), PurePosixPath("/x/Main.luau"))
    assert rebuilt == original


def test_extract_nested_alias_pin_round_trips():
    """A tailed `ALIAS @alpha/sub @util` pins util's extract dir inside
    alpha's so most-specific-alias canonicalization re-derives it."""
    src = MemoryFS.from_dict({
        "/proj/.luaurc": json.dumps({"aliases": {"alpha": "/e", "util": "/e/sub"}}),
        "/proj/Main.luau": 'require("@alpha/x")\n',
        "/e/x.luau": 'require("./sub/helpers")\n',
        "/e/sub/helpers.luau": "return {}\n",
    })
    original = bundle(src, PurePosixPath("/proj"), PurePosixPath("/proj/Main.luau"))
    assert "-- !!LUABUNDLE:ALIAS @alpha/sub @util" in original

    out = MemoryFS()
    extract_to_dir(parse_bundle(original), out, PurePosixPath("/x"))
    luaurc = json.loads(out.read(PurePosixPath("/x/.luaurc")))
    assert luaurc == {"aliases": {"alpha": "alpha", "util": "alpha/sub"}}
    assert out.is_file(PurePosixPath("/x/alpha/x.luau"))
    assert out.is_file(PurePosixPath("/x/alpha/sub/helpers.luau"))

    rebuilt = bundle(out, PurePosixPath("/x"), PurePosixPath("/x/Main.luau"))
    assert rebuilt == original


def test_extract_main_under_deeper_alias_round_trips():
    """MAIN stays root-relative even when an alias covers its directory;
    extraction places it at its root-relative path and the ALIAS line
    re-derives on re-bundle."""
    src = MemoryFS.from_dict({
        "/proj/.luaurc": json.dumps({"aliases": {"src": "src"}}),
        "/proj/src/Main.luau": 'require("./lib/foo")\n',
        "/proj/src/lib/foo.luau": "return {}\n",
    })
    original = bundle(src, PurePosixPath("/proj"), PurePosixPath("/proj/src/Main.luau"))
    assert "-- !!LUABUNDLE:MAIN @root/src/Main" in original
    assert "-- !!LUABUNDLE:ALIAS @root/src @src" in original

    out = MemoryFS()
    extract_to_dir(parse_bundle(original), out, PurePosixPath("/x"))
    assert out.is_file(PurePosixPath("/x/src/Main.luau"))
    assert out.is_file(PurePosixPath("/x/src/lib/foo.luau"))

    rebuilt = bundle(out, PurePosixPath("/x"), PurePosixPath("/x/src/Main.luau"))
    assert rebuilt == original


def test_extract_rejects_root_key_inside_alias_dir(fs: MemoryFS):
    """A @root key whose path passes through an alias's extract dir would
    re-key under that alias on re-bundle; relocation made the layout
    inexpressible."""
    parsed = ParsedBundle(
        fields={"version": "1", "main": "@root/Main"},
        main_source="\n",
        modules={
            "@root/pkg/a": "A\n",
            "@pkg/b": "B\n",
        },
    )
    with pytest.raises(ExtractAliasError):
        extract_to_dir(parsed, fs, OUT)
    assert list(fs.iter_files()) == []


def test_extract_rejects_root_key_inside_declared_nonmodule_alias_dir(fs: MemoryFS):
    """A tailed remap source alias gets a .luaurc directory without owning
    any module keys; @root files landing inside it would still re-key on
    re-bundle, so the shadow check must cover declared dirs, not just
    module-key aliases."""
    parsed = ParsedBundle(
        fields={"version": "1", "main": "@root/Main"},
        main_source="\n",
        modules={
            "@util/helpers": "H\n",
            "@root/zeta/x": "X\n",
        },
        remap={"@zeta/sub": "@util"},
    )
    with pytest.raises(ExtractAliasError):
        extract_to_dir(parsed, fs, OUT)
    assert list(fs.iter_files()) == []


def test_extract_rejects_leaf_and_init_ambiguity(fs: MemoryFS):
    """@root/pkg (-> pkg.luau) plus bare @pkg (-> pkg/init.luau): a re-bundle
    require of @root/pkg would hit AmbiguousResolutionError, so the layout
    is not hermetic."""
    parsed = ParsedBundle(
        fields={"version": "1", "main": "@root/Main"},
        main_source="\n",
        modules={
            "@root/pkg": "leaf\n",
            "@pkg": "init\n",
        },
    )
    with pytest.raises(ExtractAmbiguityError):
        extract_to_dir(parsed, fs, OUT)
    assert list(fs.iter_files()) == []


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
