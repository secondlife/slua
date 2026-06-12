"""Bundle production from an FSBackend."""

import warnings
from pathlib import PurePosixPath

import pytest

from slua_bundle import (
    AliasCollisionWarning,
    AmbiguousResolutionError,
    DepthExceededError,
    MarkerInjectionError,
    MemoryFS,
    ModuleCountExceededError,
    NoResolverError,
    ReservedAliasError,
    bundle,
    parse_bundle,
)
from slua_bundle.bundler import MAX_DEPTH, MAX_MODULES

from ._helpers import luaurc


def test_bundle_emits_main_anchor_and_modules():
    vfs = MemoryFS.from_dict({
        "/project/src/HudController.luau": 'require("./lib/foo")',
        "/project/src/lib/foo.luau": 'return "foo"',
    })
    text = bundle(
        vfs,
        PurePosixPath("/project/src"),
        PurePosixPath("/project/src/HudController.luau"),
        "myhud",
    )
    assert text == """\
-- !!LUABUNDLE:VERSION 1
-- !!LUABUNDLE:PROJECT myhud
-- !!LUABUNDLE:MAIN @root/HudController
-- !!LUABUNDLE:BODY
require("./lib/foo")
-- !!LUABUNDLE:MODULE @root/lib/foo
return "foo"
"""


def test_bundle_alias_pointing_outside_project_root_is_fine():
    """An explicit alias to a directory outside project_root is honored.

    The user knows what they're declaring; the bundler maps the file under
    the alias and ships it. This is what enables vendored deps that don't
    live under project_root.
    """
    vfs = MemoryFS.from_dict({
        "/project/.luaurc": luaurc({"shared": "/elsewhere"}),
        "/project/src/Main.luau": 'require("@shared/util")',
        "/elsewhere/util.luau": "return {}",
    })
    text = bundle(
        vfs,
        PurePosixPath("/project"),
        PurePosixPath("/project/src/Main.luau"),
        "myhud",
    )
    assert text == """\
-- !!LUABUNDLE:VERSION 1
-- !!LUABUNDLE:PROJECT myhud
-- !!LUABUNDLE:MAIN @root/src/Main
-- !!LUABUNDLE:BODY
require("@shared/util")
-- !!LUABUNDLE:MODULE @shared/util
return {}
"""


def test_bundle_includes_wally_package():
    # Bundler reads only the project-root .luaurc. Wally writes its alias
    # declarations there, which is sufficient for MAIN to resolve @SomeLib.
    vfs = MemoryFS.from_dict({
        "/project/.luaurc": luaurc({"SomeLib": "Packages/SomeLib"}),
        "/project/src/Main.luau": 'require("@SomeLib/util")',
        "/project/Packages/SomeLib/util.luau": "return {}",
    })
    text = bundle(
        vfs,
        PurePosixPath("/project"),
        PurePosixPath("/project/src/Main.luau"),
        "myhud",
    )
    assert text == """\
-- !!LUABUNDLE:VERSION 1
-- !!LUABUNDLE:PROJECT myhud
-- !!LUABUNDLE:MAIN @root/src/Main
-- !!LUABUNDLE:BODY
require("@SomeLib/util")
-- !!LUABUNDLE:MODULE @SomeLib/util
return {}
"""


def test_bundle_rejects_marker_injection_in_source():
    """Source files must not contain '-- !!LUABUNDLE:' markers; bundler must reject."""
    vfs = MemoryFS.from_dict({
        "/project/src/Main.luau": 'require("./lib/evil")',
        "/project/src/lib/evil.luau": "-- !!LUABUNDLE:MODULE!! @evil/injected\nreturn {}",
    })
    with pytest.raises(MarkerInjectionError):
        bundle(
            vfs,
            PurePosixPath("/project/src"),
            PurePosixPath("/project/src/Main.luau"),
            "myhud",
        )


def test_bundle_rejects_marker_injection_in_main():
    vfs = MemoryFS.from_dict({
        "/project/src/Main.luau": "-- !!LUABUNDLE:BUNDLE!! evil\nreturn {}",
    })
    with pytest.raises(MarkerInjectionError):
        bundle(
            vfs,
            PurePosixPath("/project/src"),
            PurePosixPath("/project/src/Main.luau"),
            "myhud",
        )


def test_bundle_rejects_ambiguous_resolution():
    """When both <leaf>.luau and <leaf>/init.luau exist, the require is ambiguous."""
    vfs = MemoryFS.from_dict({
        "/project/src/Main.luau": 'require("./foo")',
        "/project/src/foo.luau": 'return "leaf"',
        "/project/src/foo/init.luau": 'return "dir"',
    })
    with pytest.raises(AmbiguousResolutionError):
        bundle(
            vfs,
            PurePosixPath("/project/src"),
            PurePosixPath("/project/src/Main.luau"),
            "myhud",
        )


def test_bundle_only_includes_files_reachable_from_main():
    """Files not transitively required by MAIN are not bundled."""
    vfs = MemoryFS.from_dict({
        "/project/src/Main.luau": 'require("./foo")',
        "/project/src/foo.luau": "return 1",
        "/project/src/orphan.luau": 'return "should not appear"',
        "/project/src/lib/sub.luau": 'return "also unused"',
    })
    text = bundle(
        vfs,
        PurePosixPath("/project/src"),
        PurePosixPath("/project/src/Main.luau"),
        "myhud",
    )
    assert text == """\
-- !!LUABUNDLE:VERSION 1
-- !!LUABUNDLE:PROJECT myhud
-- !!LUABUNDLE:MAIN @root/Main
-- !!LUABUNDLE:BODY
require("./foo")
-- !!LUABUNDLE:MODULE @root/foo
return 1
"""


def test_bundle_traces_single_quoted_requires():
    """require('./foo') is valid Lua and must be picked up like the double-quoted form."""
    vfs = MemoryFS.from_dict({
        "/project/src/Main.luau": "require('./foo')",
        "/project/src/foo.luau": "return 1",
    })
    text = bundle(
        vfs,
        PurePosixPath("/project/src"),
        PurePosixPath("/project/src/Main.luau"),
        "myhud",
    )
    assert text == """\
-- !!LUABUNDLE:VERSION 1
-- !!LUABUNDLE:PROJECT myhud
-- !!LUABUNDLE:MAIN @root/Main
-- !!LUABUNDLE:BODY
require('./foo')
-- !!LUABUNDLE:MODULE @root/foo
return 1
"""


def test_bundle_rejects_require_to_missing_file():
    """A require pointing at a nonexistent file with no fallback raises NoResolverError."""
    vfs = MemoryFS.from_dict({
        "/project/src/Main.luau": 'require("./missing")',
    })
    with pytest.raises(NoResolverError, match="no resolver produced source"):
        bundle(
            vfs,
            PurePosixPath("/project/src"),
            PurePosixPath("/project/src/Main.luau"),
            "myhud",
        )


def test_bundle_round_trip_is_stable():
    """bundle -> parse -> compare; module bodies and main come back verbatim."""
    main_src = 'require("./lib/foo")\nreturn 1\n'
    foo_src = 'return "foo"\n'
    vfs = MemoryFS.from_dict({
        "/project/src/Main.luau": main_src,
        "/project/src/lib/foo.luau": foo_src,
    })
    text = bundle(
        vfs,
        PurePosixPath("/project/src"),
        PurePosixPath("/project/src/Main.luau"),
        "myhud",
    )
    parsed = parse_bundle(text)
    assert parsed.main_source == main_src
    assert parsed.modules == {"@root/lib/foo": foo_src}


def test_bundle_round_trip_appends_missing_trailing_newline():
    """A source without a trailing newline gets exactly one appended -- once -- for marker alignment."""
    vfs = MemoryFS.from_dict({
        "/project/src/Main.luau": "return 1",  # no trailing \n
    })
    text = bundle(
        vfs,
        PurePosixPath("/project/src"),
        PurePosixPath("/project/src/Main.luau"),
        "myhud",
    )
    parsed = parse_bundle(text)
    # Body now has the forced \n. Re-parse is then idempotent.
    assert parsed.main_source == "return 1\n"


def test_bundle_without_project_name_omits_project_directive():
    """PROJECT is optional viewer-linkage metadata; bundling without it emits no PROJECT line."""
    vfs = MemoryFS.from_dict({
        "/project/src/Main.luau": 'require("./foo")',
        "/project/src/foo.luau": "return 1",
    })
    text = bundle(
        vfs,
        PurePosixPath("/project/src"),
        PurePosixPath("/project/src/Main.luau"),
    )
    assert text == """\
-- !!LUABUNDLE:VERSION 1
-- !!LUABUNDLE:MAIN @root/Main
-- !!LUABUNDLE:BODY
require("./foo")
-- !!LUABUNDLE:MODULE @root/foo
return 1
"""
    parsed = parse_bundle(text)
    assert "project" not in parsed.fields
    assert parsed.modules == {"@root/foo": "return 1\n"}


def test_bundle_rejects_reserved_root_alias_in_luaurc():
    """@root is built-in; .luaurc cannot redefine it."""
    vfs = MemoryFS.from_dict({
        "/project/.luaurc": luaurc({"root": "/elsewhere"}),
        "/project/src/Main.luau": "return 1",
    })
    with pytest.raises(ReservedAliasError):
        bundle(
            vfs,
            PurePosixPath("/project"),
            PurePosixPath("/project/src/Main.luau"),
            "myhud",
        )


def test_bundle_user_alias_at_project_root_loses_to_root_silently():
    """User-declared alias targeting project_root coexists with @root; @root wins canonicalization with no warning."""
    vfs = MemoryFS.from_dict({
        "/project/.luaurc": luaurc({"myproj": "."}),
        "/project/src/Main.luau": "return 1",
    })
    with warnings.catch_warnings():
        warnings.simplefilter("error", AliasCollisionWarning)
        text = bundle(
            vfs,
            PurePosixPath("/project"),
            PurePosixPath("/project/src/Main.luau"),
            "myhud",
        )
    assert "-- !!LUABUNDLE:MAIN @root/src/Main" in text
    assert "@myproj" not in text


def test_bundle_two_user_aliases_at_identical_target_warns():
    """Two non-root user aliases pointing at the exact same directory: ASCII-first wins, warning emitted."""
    vfs = MemoryFS.from_dict({
        "/project/.luaurc": luaurc({"Zeta": "/elsewhere", "Alpha": "/elsewhere"}),
        "/project/src/Main.luau": 'require("@Alpha/util")',
        "/elsewhere/util.luau": "return {}",
    })
    with pytest.warns(AliasCollisionWarning, match="Alpha"):
        text = bundle(
            vfs,
            PurePosixPath("/project"),
            PurePosixPath("/project/src/Main.luau"),
            "myhud",
        )
    assert "-- !!LUABUNDLE:MODULE @Alpha/util" in text
    assert "-- !!LUABUNDLE:MODULE @Zeta/util" not in text


# ---- Last-resort resolver (existing_bundle fallback) ------------------------


_PRIOR_BUNDLE = """\
-- !!LUABUNDLE:VERSION 1
-- !!LUABUNDLE:MAIN @root/Main
-- !!LUABUNDLE:BODY
require("./lib/foo")
-- !!LUABUNDLE:MODULE @root/lib/foo
return "from prior bundle"
"""


def test_existing_bundle_used_when_disk_source_missing():
    """Disk has MAIN but not its dependency; embedded copy fills the gap."""
    vfs = MemoryFS.from_dict({
        "/project/Main.luau": 'require("./lib/foo")',
    })
    text = bundle(
        vfs,
        PurePosixPath("/project"),
        PurePosixPath("/project/Main.luau"),
        existing_bundle=_PRIOR_BUNDLE,
    )
    assert "-- !!LUABUNDLE:MODULE @root/lib/foo" in text
    assert 'return "from prior bundle"' in text


def test_existing_bundle_used_when_alias_missing_from_luaurc():
    """Require references an alias the new env has no .luaurc entry for."""
    prior = """\
-- !!LUABUNDLE:VERSION 1
-- !!LUABUNDLE:MAIN @root/Main
-- !!LUABUNDLE:BODY
require("@SomeLib/util")
-- !!LUABUNDLE:MODULE @SomeLib/util
return "vendored"
"""
    vfs = MemoryFS.from_dict({
        "/project/Main.luau": 'require("@SomeLib/util")',
    })
    text = bundle(
        vfs,
        PurePosixPath("/project"),
        PurePosixPath("/project/Main.luau"),
        existing_bundle=prior,
    )
    assert "-- !!LUABUNDLE:MODULE @SomeLib/util" in text
    assert 'return "vendored"' in text


def test_disk_preferred_over_existing_bundle_when_both_present():
    """Disk version wins when both resolvers can produce source."""
    vfs = MemoryFS.from_dict({
        "/project/Main.luau": 'require("./lib/foo")',
        "/project/lib/foo.luau": 'return "from disk"',
    })
    text = bundle(
        vfs,
        PurePosixPath("/project"),
        PurePosixPath("/project/Main.luau"),
        existing_bundle=_PRIOR_BUNDLE,
    )
    assert 'return "from disk"' in text
    assert 'return "from prior bundle"' not in text


def test_no_resolver_succeeded_raises():
    """Neither disk nor existing bundle has the required module."""
    vfs = MemoryFS.from_dict({
        "/project/Main.luau": 'require("./lib/missing")',
    })
    with pytest.raises(NoResolverError, match="no resolver produced source"):
        bundle(
            vfs,
            PurePosixPath("/project"),
            PurePosixPath("/project/Main.luau"),
            existing_bundle=_PRIOR_BUNDLE,
        )


# ---- Depth + module-count limits --------------------------------------------


def _chain_vfs(n: int) -> MemoryFS:
    """MAIN -> m1 -> m2 -> ... -> m{n-1}, requires written as ./mK."""
    files: dict[str, str] = {
        "/p/Main.luau": 'require("./m1")' if n > 0 else "return 1",
    }
    for i in range(1, n):
        body = f'require("./m{i+1}")' if i < n - 1 else "return 1"
        files[f"/p/m{i}.luau"] = body
    return MemoryFS.from_dict(files)


def test_depth_at_limit_allowed():
    """A chain at exactly MAX_DEPTH depth bundles cleanly."""
    # MAIN at depth 0, deepest module at depth MAX_DEPTH.
    vfs = _chain_vfs(MAX_DEPTH + 1)
    text = bundle(
        vfs,
        PurePosixPath("/p"),
        PurePosixPath("/p/Main.luau"),
    )
    assert f"-- !!LUABUNDLE:MODULE @root/m{MAX_DEPTH}" in text


def test_depth_exceeded_raises():
    """A chain one step deeper than MAX_DEPTH trips the limit."""
    vfs = _chain_vfs(MAX_DEPTH + 2)
    with pytest.raises(DepthExceededError, match=f"exceeds maximum {MAX_DEPTH}"):
        bundle(
            vfs,
            PurePosixPath("/p"),
            PurePosixPath("/p/Main.luau"),
        )


def test_parenless_require_sugar_is_traced():
    """require "./lib/foo" (valid Luau call sugar) pulls the module in."""
    vfs = MemoryFS.from_dict({
        "/project/Main.luau": 'require "./lib/foo"',
        "/project/lib/foo.luau": "return {}",
    })
    text = bundle(
        vfs,
        PurePosixPath("/project"),
        PurePosixPath("/project/Main.luau"),
    )
    assert "@root/lib/foo" in parse_bundle(text).modules


def test_module_count_exceeded_raises():
    """MAIN with > MAX_MODULES distinct dependencies trips the count limit."""
    requires = "\n".join(f'require("./m{i}")' for i in range(MAX_MODULES + 1))
    files: dict[str, str] = {"/p/Main.luau": requires}
    for i in range(MAX_MODULES + 1):
        files[f"/p/m{i}.luau"] = "return 1"
    vfs = MemoryFS.from_dict(files)
    with pytest.raises(ModuleCountExceededError, match=f"maximum of {MAX_MODULES}"):
        bundle(
            vfs,
            PurePosixPath("/p"),
            PurePosixPath("/p/Main.luau"),
        )
