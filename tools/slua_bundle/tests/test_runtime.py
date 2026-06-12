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
        *_v1_header(project="myhud", main="@myhud/Main"),
        "main body line 1",
        "-- !!LUABUNDLE:MODULE @myhud/lib/foo",
        "foo body",
    )
    parsed = parse_bundle(text)
    assert parsed.fields == {"version": "1", "project": "myhud", "main": "@myhud/Main"}
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
        "-- !!LUABUNDLE:MAIN @myhud/A",
        "-- !!LUABUNDLE:MAIN @myhud/B",
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
        *_v1_header(main="@myhud/Main"),
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
    """Dedup via canonical key. MAIN is init.luau-style (canonical @myhud, no leaf)
    so @self/x is sibling-like; bar uses ./foo to reach the same module."""
    text = _make(
        *_v1_header(project="myhud", main="@myhud"),
        'require("@self/lib/foo"); require("@self/lib/bar")',
        "-- !!LUABUNDLE:MODULE @myhud/lib/foo",
        'return "foo"',
        "-- !!LUABUNDLE:MODULE @myhud/lib/bar",
        'require("./foo"); return "bar"',
    )
    body_runs = simulate(text)
    assert body_runs["@myhud/lib/foo"] == 1
    assert body_runs["@myhud/lib/bar"] == 1
    assert body_runs["@myhud"] == 1


def test_simulate_circular_dependency_detected():
    text = _make(
        *_v1_header(project="p", main="@p/Main"),
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
        *_v1_header(project="p", main="@p/Main"),
        'require("@p/A")',
        "-- !!LUABUNDLE:MODULE @p/A",
        'require("@p/Main")',
    )
    with pytest.raises(CircularDependencyError):
        simulate(text)


def test_simulate_main_with_anchor_accepts_relative_and_absolute():
    """init.luau-style MAIN: @self/x and ./x both land at the alias root and dedup."""
    text = _make(
        *_v1_header(project="p", main="@p"),
        'require("@self/lib/foo"); require("./lib/foo")',
        "-- !!LUABUNDLE:MODULE @p/lib/foo",
        'return "foo"',
    )
    body_runs = simulate(text)
    assert body_runs["@p/lib/foo"] == 1


def test_simulate_traverses_single_quoted_requires():
    """simulate() walks single-quoted requires, same as the bundler."""
    text = _make(
        *_v1_header(project="p", main="@p/Main"),
        "require('./lib/foo')",
        "-- !!LUABUNDLE:MODULE @p/lib/foo",
        'return "foo"',
    )
    body_runs = simulate(text)
    assert body_runs.get("@p/lib/foo") == 1


def test_simulate_bare_identifier_rejected():
    text = _make(
        *_v1_header(project="p", main="@p/Main"),
        'require("foo")',
    )
    with pytest.raises(BareIdentifierError):
        simulate(text)


def test_simulate_unknown_alias_rejected():
    text = _make(
        *_v1_header(project="p", main="@p/Main"),
        'require("@nope/x")',
    )
    with pytest.raises(UnknownAliasError):
        simulate(text)
