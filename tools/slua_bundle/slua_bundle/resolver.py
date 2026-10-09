from __future__ import annotations

import re
from typing import Iterator

from .errors import BundleError, ReservedAliasError


class BareIdentifierError(BundleError):
    pass


class UnknownAliasError(BundleError):
    pass


class RequireEscapesAliasError(BundleError):
    pass


class RelativeRequireWithoutAnchorError(BundleError):
    pass


class InvalidPathComponentError(BundleError):
    pass


class MalformedKeyError(BundleError):
    """A canonical key does not have the @alias[/...] shape."""


# Matches require with a string-literal argument, parenthesized or via Luau's
# call sugar (`require "./x"`). Each alternative captures the string body in
# its own group; iter_requires() picks whichever matched. Known limitations
# (documented in the RFC's reference-implementation caveats): matches inside
# comments and long strings, and cannot see dynamic requires at all.
REQUIRE_RE = re.compile(
    r"\brequire\s*"
    r"(?:\(\s*(?:\"([^\"]*)\"|'([^']*)')\s*\)"
    r"|\"([^\"]*)\""
    r"|'([^']*)')"
)


def iter_requires(source: str) -> Iterator[str]:
    """Yield the string argument of every require() found in source."""
    for m in REQUIRE_RE.finditer(source):
        for group in m.groups():
            if group is not None:
                yield group
                break


def ascii_lower(s: str) -> str:
    """ASCII-only lowercasing, matching upstream Luau's alias folding
    (Config.cpp lowercases A-Z only; non-ASCII alias names are unspecified).

    bytes.lower() only touches A-Z, and UTF-8 multibyte sequences are all
    >= 0x80, so non-ASCII round-trips untouched.
    """
    return s.encode("utf-8").lower().decode("utf-8")


def _split_canonical(key: str) -> tuple[str, list[str]]:
    if not key.startswith("@"):
        raise MalformedKeyError(f"canonical key must start with @: {key!r}")
    rest = key[1:]
    if "/" in rest:
        alias, tail = rest.split("/", 1)
        return alias, tail.split("/")
    return rest, []


def apply_remap(key: str, remap: dict[str, str]) -> str:
    """Rewrite key through a bundle's ALIAS table ({from: to}).

    Longest component-prefix match wins. A single application suffices:
    every mapping's target is a tiebreak-winner alias, already canonical.
    """
    if not remap:
        return key
    alias, parts = _split_canonical(key)
    for i in range(len(parts), -1, -1):
        target = remap.get(_join_canonical(alias, parts[:i]))
        if target is not None:
            to_alias, to_parts = _split_canonical(target)
            return _join_canonical(to_alias, to_parts + parts[i:])
    return key


def _join_canonical(alias: str, parts: list[str]) -> str:
    if not parts:
        return f"@{alias}"
    return f"@{alias}/" + "/".join(parts)


def _normalize(parts: list[str], context: str) -> list[str]:
    out: list[str] = []
    for p in parts:
        if p == "" or p == ".":
            continue
        if p == "..":
            if not out:
                raise RequireEscapesAliasError(
                    f"{context}: '..' traverses past alias root"
                )
            out.pop()
        else:
            if p.startswith("."):
                raise InvalidPathComponentError(
                    f"{context}: component {p!r} starts with '.' "
                    "(would resolve to a hidden file)"
                )
            if "\x00" in p:
                raise InvalidPathComponentError(
                    f"{context}: component contains NUL"
                )
            out.append(p)
    return out


def resolve(require_str: str, anchor_key: str | None, known_aliases: set[str]) -> str:
    if require_str.startswith("@"):
        rest = require_str[1:]
        if "/" in rest:
            alias, tail = rest.split("/", 1)
        else:
            alias, tail = rest, ""

        # Alias names are case-insensitive, matching upstream Luau
        # (RequireNavigator lowercases before matching). Canonical keys use
        # the lowercased spelling.
        alias = ascii_lower(alias)

        if alias == "sl":
            raise ReservedAliasError(
                f"require '{require_str}': the '@sl' namespace is reserved for future use"
            )

        if alias == "self":
            if anchor_key is None:
                raise RelativeRequireWithoutAnchorError(
                    f"require '{require_str}' uses '@self' but the requiring module has no "
                    "anchor key (MAIN's anchor comes from the MAIN directive)"
                )
            # Match Luau's RequireNavigator: @self resets to the requirer's
            # *module path* and navigates from there. The module path is the
            # file with its extension (and `init` suffix) stripped, which is
            # exactly the canonical key. So @self/x from a requirer with key
            # @p/lib/foo resolves to @p/lib/foo/x -- inside a subdirectory
            # named after the file. That only points somewhere real for
            # init.luau-style modules (whose canonical key already has no
            # leaf); for leaf files @self is mostly useless.
            anchor_alias, anchor_parts = _split_canonical(anchor_key)
            tail_parts = tail.split("/") if tail else []
            new_parts = _normalize(
                anchor_parts + tail_parts,
                f"require '{require_str}' from {anchor_key}",
            )
            return _join_canonical(anchor_alias, new_parts)

        if alias not in known_aliases:
            raise UnknownAliasError(
                f"unknown alias '@{alias}' in require '{require_str}'"
            )
        tail_parts = tail.split("/") if tail else []
        new_parts = _normalize(tail_parts, f"require '{require_str}'")
        return _join_canonical(alias, new_parts)

    if require_str.startswith("./") or require_str.startswith("../") or require_str in (
        ".",
        "..",
    ):
        if anchor_key is None:
            raise RelativeRequireWithoutAnchorError(
                f"relative require '{require_str}' has no anchor "
                "(MAIN's anchor comes from the MAIN directive)"
            )
        anchor_alias, anchor_parts = _split_canonical(anchor_key)
        anchor_dir_parts = anchor_parts[:-1] if anchor_parts else []
        rel_parts = require_str.split("/")
        new_parts = _normalize(
            anchor_dir_parts + rel_parts,
            f"require '{require_str}' from {anchor_key}",
        )
        return _join_canonical(anchor_alias, new_parts)

    raise BareIdentifierError(
        f"require path must start with './', '../', or '@' (got '{require_str}')"
    )
