"""Lexical resolution of require strings to canonical keys."""

import pytest

from slua_bundle import (
    BareIdentifierError,
    InvalidPathComponentError,
    RelativeRequireWithoutAnchorError,
    RequireEscapesAliasError,
    UnknownAliasError,
    resolve,
)

KNOWN = {"myhud", "SomeLib"}


def test_absolute_alias_passthrough():
    assert resolve("@myhud/lib/foo", "@myhud/Main", KNOWN) == "@myhud/lib/foo"


def test_self_from_init_module_resolves_to_alias_root():
    """@self/x from an init.luau-style anchor (no leaf in the key) lands at the alias root.
    This is the realistic / useful case -- @self in an init.luau is sibling-like.
    """
    assert resolve("@self/lib/foo", "@myhud", KNOWN) == "@myhud/lib/foo"


def test_self_from_alias_root_with_no_leaf():
    """Same as above with a different alias -- explicit pin on Luau's behavior."""
    assert resolve("@self/util", "@SomeLib", KNOWN) == "@SomeLib/util"


def test_self_from_leaf_includes_filename_in_path():
    """@self/x from a leaf file resolves *under* a subdir named after the file
    (extension stripped). This matches Luau's RequireNavigator semantics; in
    practice the subdir rarely exists, so @self in a leaf is rarely useful.
    """
    assert resolve("@self/lib/foo", "@myhud/HudController", KNOWN) == "@myhud/HudController/lib/foo"
    assert resolve("@self/sibling", "@myhud/lib/bar", KNOWN) == "@myhud/lib/bar/sibling"


def test_dot_relative_resolves_to_anchor_dir():
    assert resolve("./bar", "@myhud/lib/foo", KNOWN) == "@myhud/lib/bar"


def test_dotdot_relative_pops_one_level():
    assert resolve("../sibling", "@myhud/lib/foo", KNOWN) == "@myhud/sibling"


def test_dot_relative_collapses_dot_segments():
    assert resolve("./a/./b/../c", "@myhud/lib/foo", KNOWN) == "@myhud/lib/a/c"


def test_bare_identifier_rejected():
    with pytest.raises(BareIdentifierError):
        resolve("foo", "@myhud/Main", KNOWN)


def test_unknown_alias_rejected():
    with pytest.raises(UnknownAliasError):
        resolve("@nope/x", "@myhud/Main", KNOWN)


def test_escape_via_dotdot_inside_absolute_alias():
    with pytest.raises(RequireEscapesAliasError):
        resolve("@myhud/../escape", "@myhud/Main", KNOWN)


def test_escape_via_dotdot_in_relative():
    with pytest.raises(RequireEscapesAliasError):
        resolve("../../escape", "@myhud/lib/foo", KNOWN)


def test_escape_via_self_dotdot():
    # Need three `..` to escape past @myhud/lib/foo's three components.
    with pytest.raises(RequireEscapesAliasError):
        resolve("@self/../../../escape", "@myhud/lib/foo", KNOWN)


def test_relative_without_anchor_rejected():
    """MAIN without main= cannot use relative requires."""
    with pytest.raises(RelativeRequireWithoutAnchorError):
        resolve("./x", None, KNOWN)


def test_self_without_anchor_rejected():
    with pytest.raises(RelativeRequireWithoutAnchorError):
        resolve("@self/x", None, KNOWN)


def test_absolute_alias_works_without_anchor():
    """MAIN without main= can still use absolute aliases."""
    assert resolve("@myhud/lib/foo", None, KNOWN) == "@myhud/lib/foo"


def test_path_component_starting_with_dot_rejected():
    """A component like '.bashrc' would resolve to a hidden file -- reject."""
    with pytest.raises(InvalidPathComponentError):
        resolve("@myhud/.bashrc", "@myhud/Main", KNOWN)
    with pytest.raises(InvalidPathComponentError):
        resolve("./.config/foo", "@myhud/lib/x", KNOWN)


def test_path_component_with_nul_rejected():
    with pytest.raises(InvalidPathComponentError):
        resolve("@myhud/lib/foo\x00bar", "@myhud/Main", KNOWN)
