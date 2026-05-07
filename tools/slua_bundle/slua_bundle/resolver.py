from __future__ import annotations

from .errors import BundleError


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


def _split_canonical(key: str) -> tuple[str, list[str]]:
    assert key.startswith("@"), f"canonical key must start with @: {key}"
    rest = key[1:]
    if "/" in rest:
        alias, tail = rest.split("/", 1)
        return alias, tail.split("/")
    return rest, []


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

        if alias == "self":
            if anchor_key is None:
                raise RelativeRequireWithoutAnchorError(
                    f"require '{require_str}' uses '@self' but the requiring module has no "
                    "anchor key (set 'main=' in the BUNDLE header for MAIN)"
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
                "(set 'main=' in the BUNDLE header for MAIN)"
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
