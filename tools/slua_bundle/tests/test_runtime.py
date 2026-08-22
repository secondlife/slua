"""Bundle parsing and execution-graph traversal."""

from __future__ import annotations

import pytest

from slua_bundle import (
    BareIdentifierError,
    BundleParseError,
    CircularDependencyError,
    UnknownAliasError,
    parse_bundle,
    simulate,
)


def _make(*lines: str) -> str:
    return "\n".join(lines) + "\n"


def _v1_header(project: str | None = "myhud", main: str | None = None) -> tuple[str, ...]:
    parts = ["-- !!LUABUNDLE:VERSION 1"]
    if project is not None:
        parts.append(f"-- !!LUABUNDLE:PROJECT {project}")
    if main is not None:
        parts.append(f"-- !!LUABUNDLE:MAIN {main}")
    parts.append("-- !!LUABUNDLE:BODY")
    return tuple(parts)


def test_parse_bundle_extracts_fields_and_modules():
    text = _make(
        *_v1_header(project="myhud", main="@root/Main"),
        "main body line 1",
        "-- !!LUABUNDLE:MODULE @myhud/lib/foo",
        "foo body",
    )
    parsed = parse_bundle(text)
    assert parsed.fields == {"version": "1", "project": "myhud", "main": "@root/Main"}
    assert parsed.main_source == "main body line 1\n"
    assert parsed.modules == {"@myhud/lib/foo": "foo body\n"}


def test_parse_bundle_rejects_missing_version():
    text = _make(
        "-- !!LUABUNDLE:PROJECT myhud",
        "-- !!LUABUNDLE:BODY",
        "main body",
    )
    with pytest.raises(BundleParseError, match="VERSION"):
        parse_bundle(text)


def test_parse_bundle_rejects_unsupported_version():
    text = _make(
        "-- !!LUABUNDLE:VERSION 99",
        "-- !!LUABUNDLE:PROJECT myhud",
        "-- !!LUABUNDLE:BODY",
    )
    with pytest.raises(BundleParseError, match="unsupported VERSION"):
        parse_bundle(text)


def test_parse_bundle_rejects_duplicate_project():
    text = _make(
        "-- !!LUABUNDLE:VERSION 1",
        "-- !!LUABUNDLE:PROJECT myhud",
        "-- !!LUABUNDLE:PROJECT otherhud",
        "-- !!LUABUNDLE:BODY",
    )
    with pytest.raises(BundleParseError, match="duplicate PROJECT"):
        parse_bundle(text)


def test_parse_bundle_rejects_duplicate_main():
    text = _make(
        "-- !!LUABUNDLE:VERSION 1",
        "-- !!LUABUNDLE:PROJECT myhud",
        "-- !!LUABUNDLE:MAIN @root/A",
        "-- !!LUABUNDLE:MAIN @root/B",
        "-- !!LUABUNDLE:BODY",
    )
    with pytest.raises(BundleParseError, match="duplicate MAIN"):
        parse_bundle(text)


def test_parse_bundle_rejects_unknown_header_directive():
    text = _make(
        "-- !!LUABUNDLE:VERSION 1",
        "-- !!LUABUNDLE:PROJECT myhud",
        "-- !!LUABUNDLE:FUTURE-THING something",
        "-- !!LUABUNDLE:BODY",
        "main body",
    )
    with pytest.raises(BundleParseError, match="unknown header directive"):
        parse_bundle(text)


def test_parse_bundle_rejects_missing_body_marker():
    text = _make(
        "-- !!LUABUNDLE:VERSION 1",
        "-- !!LUABUNDLE:PROJECT myhud",
    )
    with pytest.raises(BundleParseError, match="missing BODY"):
        parse_bundle(text)


def test_parse_bundle_accepts_missing_project():
    """PROJECT is optional (advisory viewer-linkage metadata)."""
    text = _make(
        "-- !!LUABUNDLE:VERSION 1",
        "-- !!LUABUNDLE:MAIN @root/X",
        "-- !!LUABUNDLE:BODY",
        "main body",
    )
    parsed = parse_bundle(text)
    assert "project" not in parsed.fields
    assert parsed.main_source == "main body\n"


def test_parse_bundle_rejects_missing_main():
    """MAIN is required; consumers reject bundles without it."""
    text = _make(
        "-- !!LUABUNDLE:VERSION 1",
        "-- !!LUABUNDLE:PROJECT myhud",
        "-- !!LUABUNDLE:BODY",
        "main body",
    )
    with pytest.raises(BundleParseError, match="missing MAIN"):
        parse_bundle(text)


def test_parse_bundle_rejects_unexpected_marker_after_body():
    text = _make(
        *_v1_header(main="@root/Main"),
        "main body",
        "-- !!LUABUNDLE:FUTURE-THING something",
        "-- !!LUABUNDLE:MODULE @myhud/x",
        "x body",
    )
    with pytest.raises(BundleParseError, match="unexpected marker"):
        parse_bundle(text)


def test_parse_bundle_rejects_body_before_body_marker():
    text = _make(
        "-- !!LUABUNDLE:VERSION 1",
        "-- !!LUABUNDLE:PROJECT myhud",
        "stray body line",
        "-- !!LUABUNDLE:BODY",
    )
    with pytest.raises(BundleParseError, match="body content before BODY"):
        parse_bundle(text)


def test_simulate_runs_each_module_body_once_for_dedup():
    """Dedup via canonical key. MAIN is init.luau-style (canonical @root, no leaf)
    so @self/x is sibling-like; bar uses ./foo to reach the same module."""
    text = _make(
        *_v1_header(project="myhud", main="@root"),
        'require("@self/lib/foo"); require("@self/lib/bar")',
        "-- !!LUABUNDLE:MODULE @root/lib/foo",
        'return "foo"',
        "-- !!LUABUNDLE:MODULE @root/lib/bar",
        'require("./foo"); return "bar"',
    )
    body_runs = simulate(text)
    assert body_runs["@root/lib/foo"] == 1
    assert body_runs["@root/lib/bar"] == 1
    assert body_runs["@root"] == 1


def test_simulate_circular_dependency_detected():
    text = _make(
        *_v1_header(project="p", main="@root/Main"),
        'require("@p/A")',
        "-- !!LUABUNDLE:MODULE @p/A",
        'require("@p/B")',
        "-- !!LUABUNDLE:MODULE @p/B",
        'require("@p/A")',
    )
    with pytest.raises(CircularDependencyError) as excinfo:
        simulate(text)
    msg = str(excinfo.value)
    assert "@p/A" in msg and "@p/B" in msg


def test_simulate_main_as_import_target_is_a_cycle():
    """A module requiring MAIN's canonical key creates a cycle."""
    text = _make(
        *_v1_header(project="p", main="@root/Main"),
        'require("@p/A")',
        "-- !!LUABUNDLE:MODULE @p/A",
        'require("@root/Main")',
    )
    with pytest.raises(CircularDependencyError):
        simulate(text)


def test_simulate_main_with_anchor_accepts_relative_and_absolute():
    """init.luau-style MAIN: @self/x and ./x both land at the alias root and dedup."""
    text = _make(
        *_v1_header(project="p", main="@root"),
        'require("@self/lib/foo"); require("./lib/foo")',
        "-- !!LUABUNDLE:MODULE @root/lib/foo",
        'return "foo"',
    )
    body_runs = simulate(text)
    assert body_runs["@root/lib/foo"] == 1


def test_simulate_traverses_single_quoted_requires():
    """simulate() walks single-quoted requires, same as the bundler."""
    text = _make(
        *_v1_header(project="p", main="@root/Main"),
        "require('./lib/foo')",
        "-- !!LUABUNDLE:MODULE @root/lib/foo",
        'return "foo"',
    )
    body_runs = simulate(text)
    assert body_runs.get("@root/lib/foo") == 1


def test_simulate_bare_identifier_rejected():
    text = _make(
        *_v1_header(project="p", main="@root/Main"),
        'require("foo")',
    )
    with pytest.raises(BareIdentifierError):
        simulate(text)


def test_simulate_unknown_alias_rejected():
    text = _make(
        *_v1_header(project="p", main="@root/Main"),
        'require("@nope/x")',
    )
    with pytest.raises(UnknownAliasError):
        simulate(text)


# ---- MAIN/MODULE key validation ----------------------------------------------


def test_parse_bundle_rejects_main_not_under_root():
    """MAIN is @root-relative by format invariant."""
    text = _make(
        *_v1_header(main="@app/Main"),
        "main body",
    )
    with pytest.raises(BundleParseError, match="must be under @root"):
        parse_bundle(text)


def test_parse_bundle_rejects_module_key_without_at():
    text = _make(
        *_v1_header(main="@root/Main"),
        "main body",
        "-- !!LUABUNDLE:MODULE lib/foo",
        "foo body",
    )
    with pytest.raises(BundleParseError, match="must start with @"):
        parse_bundle(text)


def test_parse_bundle_rejects_module_key_with_uppercase_alias():
    """Canonical keys carry the case-folded alias spelling."""
    text = _make(
        *_v1_header(main="@root/Main"),
        "main body",
        "-- !!LUABUNDLE:MODULE @SomeLib/util",
        "util body",
    )
    with pytest.raises(BundleParseError, match="lowercase canonical"):
        parse_bundle(text)


def test_parse_bundle_rejects_module_key_with_reserved_self():
    text = _make(
        *_v1_header(main="@root/Main"),
        "main body",
        "-- !!LUABUNDLE:MODULE @self/x",
        "x body",
    )
    with pytest.raises(BundleParseError, match="reserved alias @self"):
        parse_bundle(text)


def test_parse_bundle_rejects_module_key_with_reserved_sl():
    """The @sl namespace is reserved for future use; no key may claim it."""
    text = _make(
        *_v1_header(main="@root/Main"),
        "main body",
        "-- !!LUABUNDLE:MODULE @sl/x",
        "x body",
    )
    with pytest.raises(BundleParseError, match="reserved alias @sl"):
        parse_bundle(text)


# ---- ALIAS directive ----------------------------------------------------------


def _alias_header(*alias_lines: str) -> tuple[str, ...]:
    return (
        "-- !!LUABUNDLE:VERSION 1",
        "-- !!LUABUNDLE:MAIN @root/Main",
        *alias_lines,
        "-- !!LUABUNDLE:BODY",
    )


def test_simulate_resolves_through_alias_remap():
    """A require spelled through a remapped prefix lands on the canonical module."""
    text = _make(
        *_alias_header("-- !!LUABUNDLE:ALIAS @myproj @root"),
        'require("@myproj/util")',
        "-- !!LUABUNDLE:MODULE @root/util",
        "return {}",
    )
    body_runs = simulate(text)
    assert body_runs == {"@root/Main": 1, "@root/util": 1}


def test_simulate_alias_remap_longest_prefix_wins():
    text = _make(
        *_alias_header(
            "-- !!LUABUNDLE:ALIAS @a @b",
            "-- !!LUABUNDLE:ALIAS @a/sub @c",
        ),
        'require("@a/x"); require("@a/sub/y")',
        "-- !!LUABUNDLE:MODULE @b/x",
        "return 1",
        "-- !!LUABUNDLE:MODULE @c/y",
        "return 2",
    )
    body_runs = simulate(text)
    assert body_runs["@b/x"] == 1
    assert body_runs["@c/y"] == 1


def test_parse_bundle_rejects_alias_with_wrong_arity():
    text = _make(
        *_alias_header("-- !!LUABUNDLE:ALIAS @myproj"),
        "main body",
    )
    with pytest.raises(BundleParseError, match="exactly two keys"):
        parse_bundle(text)


def test_parse_bundle_rejects_alias_involving_self():
    for line in (
        "-- !!LUABUNDLE:ALIAS @self @root",
        "-- !!LUABUNDLE:ALIAS @self/x @root",
        "-- !!LUABUNDLE:ALIAS @myproj @self",
    ):
        text = _make(*_alias_header(line), "main body")
        with pytest.raises(BundleParseError, match="reserved alias @self"):
            parse_bundle(text)


def test_parse_bundle_rejects_alias_remapping_bare_root():
    text = _make(
        *_alias_header("-- !!LUABUNDLE:ALIAS @root @elsewhere"),
        "main body",
    )
    with pytest.raises(BundleParseError, match="may not remap @root"):
        parse_bundle(text)


def test_parse_bundle_allows_alias_under_root_as_source():
    """@root/sub as a remap source is the relative-crossover case."""
    text = _make(
        *_alias_header("-- !!LUABUNDLE:ALIAS @root/sub @util"),
        'require("./sub/helpers")',
        "-- !!LUABUNDLE:MODULE @util/helpers",
        "return {}",
    )
    body_runs = simulate(text)
    assert body_runs["@util/helpers"] == 1


def test_parse_bundle_rejects_alias_with_tailed_target():
    text = _make(
        *_alias_header("-- !!LUABUNDLE:ALIAS @myproj @root/sub"),
        "main body",
    )
    with pytest.raises(BundleParseError, match="bare @alias"):
        parse_bundle(text)


def test_parse_bundle_rejects_alias_mapping_to_itself():
    text = _make(
        *_alias_header("-- !!LUABUNDLE:ALIAS @a @a"),
        "main body",
    )
    with pytest.raises(BundleParseError, match="to itself"):
        parse_bundle(text)


def test_parse_bundle_rejects_self_nesting_alias():
    """@b/x -> @b claims b's directory contains itself; unsatisfiable."""
    text = _make(
        *_alias_header("-- !!LUABUNDLE:ALIAS @b/x @b"),
        "main body",
    )
    with pytest.raises(BundleParseError, match="nests an alias inside itself"):
        parse_bundle(text)


def test_parse_bundle_rejects_duplicate_alias_source():
    text = _make(
        *_alias_header(
            "-- !!LUABUNDLE:ALIAS @a @b",
            "-- !!LUABUNDLE:ALIAS @a @c",
        ),
        "main body",
    )
    with pytest.raises(BundleParseError, match="duplicate ALIAS"):
        parse_bundle(text)


def test_parse_bundle_rejects_alias_with_uppercase_key():
    text = _make(
        *_alias_header("-- !!LUABUNDLE:ALIAS @MyProj @root"),
        "main body",
    )
    with pytest.raises(BundleParseError, match="lowercase canonical"):
        parse_bundle(text)


def test_parse_bundle_rejects_alias_shadowing_module_key():
    """A remapped prefix cannot contain real modules."""
    text = _make(
        *_alias_header("-- !!LUABUNDLE:ALIAS @lib @other"),
        "main body",
        "-- !!LUABUNDLE:MODULE @lib/x",
        "x body",
    )
    with pytest.raises(BundleParseError, match="shadows module key"):
        parse_bundle(text)


def test_parse_bundle_rejects_alias_after_body():
    text = _make(
        *_v1_header(main="@root/Main"),
        "main body",
        "-- !!LUABUNDLE:ALIAS @a @b",
    )
    with pytest.raises(BundleParseError, match="unexpected marker"):
        parse_bundle(text)
