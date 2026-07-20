"""Canonicalize physical paths to alias-prefixed canonical keys."""

import json
import warnings
from pathlib import PurePosixPath

import pytest

from slua_bundle import (
    AliasCollisionWarning,
    InvalidLuaurcError,
    MemoryFS,
    NoCoveringAliasError,
    ReservedAliasError,
    canonicalize,
)

from ._helpers import luaurc


def test_implicit_root_alias_no_luaurc():
    """No .luaurc -- @root implicitly covers project_root."""
    vfs = MemoryFS.from_dict({
        "/project/src/HudController.luau": "return {}",
        "/project/src/lib/foo.luau": "return {}",
    })
    project_root = PurePosixPath("/project/src")
    assert canonicalize(vfs, PurePosixPath("/project/src/HudController.luau"), project_root) == "@root/HudController"
    assert canonicalize(vfs, PurePosixPath("/project/src/lib/foo.luau"), project_root) == "@root/lib/foo"


def test_init_luau_strips_to_directory():
    vfs = MemoryFS.from_dict({
        "/project/src/init.luau": "",
        "/project/src/lib/init.luau": "",
        "/project/src/lib/foo.luau": "",
    })
    project_root = PurePosixPath("/project/src")
    assert canonicalize(vfs, PurePosixPath("/project/src/init.luau"), project_root) == "@root"
    assert canonicalize(vfs, PurePosixPath("/project/src/lib/init.luau"), project_root) == "@root/lib"


def test_wally_alias_at_project_root_beats_root():
    """Top-level .luaurc declares @SomeLib; files inside it canonicalize under @somelib.

    Most-specific covering alias wins (more path parts), so files under
    /project/Packages/SomeLib/ get @somelib/... rather than @root/Packages/SomeLib/...
    Alias names fold to lowercase in canonical keys.
    """
    vfs = MemoryFS.from_dict({
        "/project/.luaurc": luaurc({"SomeLib": "Packages/SomeLib"}),
        "/project/Packages/SomeLib/init.luau": "",
        "/project/Packages/SomeLib/util.luau": "",
        "/project/src/HudController.luau": "",
    })
    project_root = PurePosixPath("/project")
    assert canonicalize(vfs, PurePosixPath("/project/Packages/SomeLib/util.luau"), project_root) == "@somelib/util"
    assert canonicalize(vfs, PurePosixPath("/project/Packages/SomeLib/init.luau"), project_root) == "@somelib"
    assert canonicalize(vfs, PurePosixPath("/project/src/HudController.luau"), project_root) == "@root/src/HudController"


def test_nested_luaurc_is_ignored():
    """Aliases declared in a nested .luaurc are not honored.

    Bundle reproducibility requires the alias set to be fully determined by
    project_root's .luaurc; a child .luaurc that isn't mirrored at the project
    root has no effect, and a file it would have named is canonicalized under
    @root instead.
    """
    vfs = MemoryFS.from_dict({
        "/project/.luaurc": luaurc({}),
        "/project/Packages/SomeLib/.luaurc": luaurc({"SomeLib": "."}),
        "/project/Packages/SomeLib/util.luau": "",
    })
    project_root = PurePosixPath("/project")
    assert canonicalize(vfs, PurePosixPath("/project/Packages/SomeLib/util.luau"), project_root) == "@root/Packages/SomeLib/util"


def test_no_covering_alias_for_outside_path():
    """A file outside the project root with no covering .luaurc raises."""
    vfs = MemoryFS.from_dict({
        "/project/src/HudController.luau": "",
        "/elsewhere/orphan.luau": "",
    })
    project_root = PurePosixPath("/project/src")
    with pytest.raises(NoCoveringAliasError):
        canonicalize(vfs, PurePosixPath("/elsewhere/orphan.luau"), project_root)


def test_external_alias_with_explicit_target():
    vfs = MemoryFS.from_dict({
        "/project/.luaurc": luaurc({"shared": "shared-modules"}),
        "/project/shared-modules/util.luau": "",
        "/project/src/main.luau": "",
    })
    project_root = PurePosixPath("/project")
    assert canonicalize(vfs, PurePosixPath("/project/shared-modules/util.luau"), project_root) == "@shared/util"


def test_jsonc_line_comments_and_trailing_commas_supported():
    """Match Luau's actual .luaurc parser: // line comments and trailing commas."""
    config = """
    {
        // Wally-style comment
        "aliases": {
            "shared": "shared-modules", // trailing comment after value
        },
    }
    """
    vfs = MemoryFS.from_dict({
        "/project/.luaurc": config,
        "/project/shared-modules/util.luau": "",
    })
    project_root = PurePosixPath("/project")
    assert canonicalize(vfs, PurePosixPath("/project/shared-modules/util.luau"), project_root) == "@shared/util"


def test_jsonc_lua_line_comments_supported():
    """Upstream tokenizes .luaurc with the Luau lexer, which skips Lua-style
    -- line comments; mirror that."""
    config = """
    {
        -- Lua-style comment
        "aliases": {
            "shared": "shared-modules", -- trailing comment after value
        },
    }
    """
    vfs = MemoryFS.from_dict({
        "/project/.luaurc": config,
        "/project/shared-modules/util.luau": "",
    })
    project_root = PurePosixPath("/project")
    assert canonicalize(vfs, PurePosixPath("/project/shared-modules/util.luau"), project_root) == "@shared/util"


def test_jsonc_lua_long_comments_supported():
    """The upstream lexer skips --[[ ]] long comments; mirror it, including
    the =-level rule (`]]` does not close a `--[==[` comment) and comments
    spanning lines or containing quotes."""
    config = """
    {--[[ multi
    line " quote // inside ]]
        "aliases": {--[==[ level ]] still open ]==] "shared": "shared-modules",--[[x]]
        },
    }
    """
    vfs = MemoryFS.from_dict({
        "/project/.luaurc": config,
        "/project/shared-modules/util.luau": "",
    })
    project_root = PurePosixPath("/project")
    assert canonicalize(vfs, PurePosixPath("/project/shared-modules/util.luau"), project_root) == "@shared/util"


def test_jsonc_unterminated_long_comment_rejected():
    """An unterminated --[[ is a lexer error, not a silent strip-to-EOL."""
    config = '{"aliases": {}} --[[ oops'
    vfs = MemoryFS.from_dict({
        "/project/.luaurc": config,
        "/project/Main.luau": "",
    })
    project_root = PurePosixPath("/project")
    with pytest.raises(InvalidLuaurcError, match="unterminated long comment"):
        canonicalize(vfs, PurePosixPath("/project/Main.luau"), project_root)


def test_jsonc_block_comments_rejected():
    """Luau's parser does not accept /* */; the prototype mirrors that."""
    config = '{"aliases": {/* not allowed */ "shared": "shared-modules"}}'
    vfs = MemoryFS.from_dict({
        "/project/.luaurc": config,
        "/project/shared-modules/util.luau": "",
    })
    project_root = PurePosixPath("/project")
    with pytest.raises(InvalidLuaurcError, match="invalid JSONC"):
        canonicalize(vfs, PurePosixPath("/project/shared-modules/util.luau"), project_root)


def test_reserved_root_alias_in_luaurc_rejected():
    """.luaurc may not declare an alias named `root`; it's the built-in project-root alias."""
    vfs = MemoryFS.from_dict({
        "/project/.luaurc": luaurc({"root": "shared-modules"}),
        "/project/Main.luau": "",
    })
    project_root = PurePosixPath("/project")
    with pytest.raises(ReservedAliasError):
        canonicalize(vfs, PurePosixPath("/project/Main.luau"), project_root)


def test_reserved_self_alias_in_luaurc_rejected():
    """.luaurc may not declare `self`; it's a Luau-ecosystem reserved name (current module's dir)."""
    vfs = MemoryFS.from_dict({
        "/project/.luaurc": luaurc({"self": "shared-modules"}),
        "/project/Main.luau": "",
    })
    project_root = PurePosixPath("/project")
    with pytest.raises(ReservedAliasError):
        canonicalize(vfs, PurePosixPath("/project/Main.luau"), project_root)


def test_reserved_sl_alias_in_luaurc_rejected():
    """.luaurc may not declare `sl`; the @sl namespace is reserved for future use."""
    vfs = MemoryFS.from_dict({
        "/project/.luaurc": luaurc({"sl": "shared-modules"}),
        "/project/Main.luau": "",
    })
    project_root = PurePosixPath("/project")
    with pytest.raises(ReservedAliasError):
        canonicalize(vfs, PurePosixPath("/project/Main.luau"), project_root)


def test_user_alias_at_project_root_loses_to_root_silently():
    """User declares an alias targeting project_root; @root wins canonicalization, no warning."""
    vfs = MemoryFS.from_dict({
        "/project/.luaurc": luaurc({"myhud": "."}),
        "/project/Main.luau": "",
    })
    project_root = PurePosixPath("/project")
    with warnings.catch_warnings():
        warnings.simplefilter("error", AliasCollisionWarning)
        assert canonicalize(vfs, PurePosixPath("/project/Main.luau"), project_root) == "@root/Main"


def test_two_user_aliases_at_identical_target_warns_and_picks_ascii_first():
    """Two non-root aliases at the same target dir: ASCII-first wins (over
    folded names), warning emitted."""
    vfs = MemoryFS.from_dict({
        "/project/.luaurc": luaurc({"Zeta": "/elsewhere", "Alpha": "/elsewhere"}),
        "/elsewhere/util.luau": "",
    })
    project_root = PurePosixPath("/project")
    with pytest.warns(AliasCollisionWarning, match="alpha"):
        assert canonicalize(vfs, PurePosixPath("/elsewhere/util.luau"), project_root) == "@alpha/util"


def test_reserved_alias_check_is_case_insensitive():
    """'Root', 'SELF', and 'SL' fold to the reserved names and are rejected."""
    for name in ("Root", "SELF", "SL"):
        vfs = MemoryFS.from_dict({
            "/project/.luaurc": luaurc({name: "shared-modules"}),
            "/project/Main.luau": "",
        })
        with pytest.raises(ReservedAliasError):
            canonicalize(vfs, PurePosixPath("/project/Main.luau"), PurePosixPath("/project"))


def test_aliases_colliding_after_case_fold_rejected():
    """Two .luaurc spellings of one (case-insensitive) alias name is a config error."""
    vfs = MemoryFS.from_dict({
        "/project/.luaurc": luaurc({"Pkg": "a", "pkg": "b"}),
        "/project/Main.luau": "",
    })
    with pytest.raises(InvalidLuaurcError, match="case folding"):
        canonicalize(vfs, PurePosixPath("/project/Main.luau"), PurePosixPath("/project"))


def test_alias_name_charset_matches_upstream():
    """Mirror upstream isValidAlias (Config.cpp): ASCII alphanumerics plus
    '-', '_', '.'; no path separators, no '.'/'..'; the leading '@' that
    upstream tolerates-but-breaks-on is rejected outright. Also covers
    json.loads' lone-surrogate escapes, which fail the charset before the
    case fold could choke on them."""
    bad_names = ["my proj", "wei/rd", "back\\slash", ".", "..", "@Pkg", "", "café", "bad\ud800name"]
    for name in bad_names:
        vfs = MemoryFS.from_dict({
            "/project/.luaurc": '{"aliases": {' + json.dumps(name) + ': "shared-modules"}}',
            "/project/Main.luau": "",
        })
        with pytest.raises(InvalidLuaurcError, match="invalid alias name"):
            canonicalize(vfs, PurePosixPath("/project/Main.luau"), PurePosixPath("/project"))


def test_alias_name_allows_upstream_charset():
    """Dots, dashes, underscores, digits are all legal alias-name chars."""
    vfs = MemoryFS.from_dict({
        "/project/.luaurc": luaurc({"pkg-v2.1_beta": "shared-modules"}),
        "/project/shared-modules/util.luau": "",
    })
    project_root = PurePosixPath("/project")
    assert canonicalize(vfs, PurePosixPath("/project/shared-modules/util.luau"), project_root) == "@pkg-v2.1_beta/util"


def test_unnormalized_project_root_normalizes():
    """A caller-supplied project_root with `.` segments must normalize before lookups."""
    vfs = MemoryFS.from_dict({
        "/project/src/.luaurc": luaurc({}),
        "/project/src/Main.luau": "",
    })
    project_root = PurePosixPath("/project/./src")
    assert canonicalize(vfs, PurePosixPath("/project/src/Main.luau"), project_root) == "@root/Main"


def test_jsonc_does_not_strip_inside_strings():
    """Comment-like sequences inside string literals must be preserved."""
    config = '{"aliases": {"path-with-slashes": "weird//name"}}'
    vfs = MemoryFS.from_dict({
        "/project/.luaurc": config,
        "/project/weird/name/foo.luau": "",
    })
    project_root = PurePosixPath("/project")
    assert canonicalize(vfs, PurePosixPath("/project/weird/name/foo.luau"), project_root) == "@path-with-slashes/foo"
