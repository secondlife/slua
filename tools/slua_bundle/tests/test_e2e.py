"""End-to-end: FSBackend -> bundle -> simulate."""

from pathlib import PurePosixPath

from slua_bundle import MemoryFS, bundle, parse_bundle, simulate
from slua_bundle.extractor import extract_to_dir

from ._helpers import luaurc


def test_happy_path():
    """MAIN -> ./lib/foo -> ./bar -> return."""
    vfs = MemoryFS.from_dict({
        "/project/src/HudController.luau": 'require("./lib/foo")',
        "/project/src/lib/foo.luau": 'require("./bar")',
        "/project/src/lib/bar.luau": "return 42",
    })
    text = bundle(
        vfs,
        PurePosixPath("/project/src"),
        PurePosixPath("/project/src/HudController.luau"),
        "myhud",
    )
    body_runs = simulate(text)
    assert body_runs == {
        "@root/HudController": 1,
        "@root/lib/foo": 1,
        "@root/lib/bar": 1,
    }


def test_bare_project_no_luaurc():
    """@root covers project_root automatically; no .luaurc needed."""
    vfs = MemoryFS.from_dict({
        "/work/src/Main.luau": 'require("./lib/foo")',
        "/work/src/lib/foo.luau": "return {}",
    })
    text = bundle(
        vfs,
        PurePosixPath("/work/src"),
        PurePosixPath("/work/src/Main.luau"),
        "myhud",
    )
    assert "-- !!LUABUNDLE:MAIN @root/Main" in text
    body_runs = simulate(text)
    assert body_runs["@root/Main"] == 1
    assert body_runs["@root/lib/foo"] == 1


def test_wally_style_dedup():
    """SomeLib's util reached two ways from its init.luau (@self/util and @SomeLib/util) dedups.

    The project-root .luaurc declares @SomeLib; that's the only config the
    bundler reads. Inside SomeLib/init.luau, both @self/util (resolves under
    init.luau's anchor module path) and @SomeLib/util reach the same file.
    """
    vfs = MemoryFS.from_dict({
        "/project/.luaurc": luaurc({"SomeLib": "Packages/SomeLib"}),
        "/project/src/Main.luau": 'require("@SomeLib")',
        "/project/Packages/SomeLib/init.luau": 'require("@self/util"); require("@SomeLib/util")',
        "/project/Packages/SomeLib/util.luau": "return {}",
    })
    text = bundle(
        vfs,
        PurePosixPath("/project"),
        PurePosixPath("/project/src/Main.luau"),
        "myhud",
    )
    body_runs = simulate(text)
    assert body_runs["@SomeLib/util"] == 1
    assert body_runs["@SomeLib"] == 1
    assert body_runs["@root/src/Main"] == 1


def test_rebundle_via_extractor_uses_last_resort_resolver():
    """Extract a bundle, drop a source file, rebundle with the original as
    fallback. Demonstrates the RFC's rebundle-without-source flow: a CLI
    receives a bundle, can't reach the original resolvers, but can still
    rebuild because the prior bundle is the universal last-resort resolver.
    """
    src_vfs = MemoryFS.from_dict({
        "/project/Main.luau": 'require("./lib/foo")',
        "/project/lib/foo.luau": 'return "hi"',
    })
    initial = bundle(
        src_vfs,
        PurePosixPath("/project"),
        PurePosixPath("/project/Main.luau"),
        "myhud",
    )

    out_fs = MemoryFS()
    extract_to_dir(parse_bundle(initial), out_fs, PurePosixPath("/out"))
    del out_fs.files[PurePosixPath("/out/lib/foo.luau")]

    rebuilt = bundle(
        out_fs,
        PurePosixPath("/out"),
        PurePosixPath("/out/Main.luau"),
        "myhud",
        existing_bundle=initial,
    )
    assert rebuilt == initial
