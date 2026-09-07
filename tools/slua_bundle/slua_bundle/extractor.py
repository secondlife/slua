"""Reverse a parsed bundle into a self-contained on-disk project tree.

Layout convention: @root/... files land at <output>/, files under other
aliases at <output>/<alias>/ -- except aliases pinned inside another by a
tailed ALIAS mapping. A .luaurc declaring every alias (including extra
names from bare ALIAS mappings) is written when any are needed. The
extracted tree is hermetic: re-bundling it with `--root <output>`
reproduces the original bundle byte-for-byte.
"""

from __future__ import annotations

import json
from pathlib import PurePath

from .errors import BundleError
from .fs import FSBackend
from .resolver import _split_canonical
from .runtime import ParsedBundle


class ExtractError(BundleError):
    pass


class ExtractCollisionError(ExtractError):
    """Two canonical keys map to the same physical path."""


class ExtractClobberError(ExtractError):
    """Output path already contains files that extract would overwrite."""


class ExtractMissingMainError(ExtractError):
    """Bundle has no MAIN directive; cannot place MAIN's body."""


class ExtractUnsafeKeyError(ExtractError):
    """Canonical key has a component that would escape the output dir or
    otherwise produce an unsafe path."""


class ExtractAliasError(ExtractError):
    """The bundle's ALIAS table cannot be expressed in the extract layout."""


class ExtractAmbiguityError(ExtractError):
    """Extracting would write <leaf>.luau alongside <leaf>/init.luau, making
    requires of that module ambiguous on re-bundle."""


_UNSAFE_COMPONENTS = frozenset({"", ".", ".."})


def _validate_key_parts(canonical_key: str, alias: str, parts: list[str]) -> None:
    """Reject keys whose components could traverse outside <output> or
    produce malformed paths. Our own bundler never emits these; this is
    the trust boundary for bundles from unknown sources.
    """
    for component in (alias, *parts):
        if component in _UNSAFE_COMPONENTS:
            raise ExtractUnsafeKeyError(
                f"canonical key {canonical_key!r} contains unsafe component {component!r}"
            )
        if any(c in component for c in ("/", "\\", "\x00")):
            raise ExtractUnsafeKeyError(
                f"canonical key {canonical_key!r} contains illegal char in component {component!r}"
            )


def physical_path_for_key(
    canonical_key: str,
    output: PurePath,
    alias_dirs: dict[str, tuple[str, ...]] | None = None,
) -> PurePath:
    """Map @alias/path -> on-disk path under <output>.

    @root files go flat under <output>; other aliases go under a same-named
    subdir unless alias_dirs pins them elsewhere (nested-alias ALIAS lines
    do that). Bare alias keys (stripped init.luau) become init.luau in the
    appropriate dir.
    """
    alias, parts = _split_canonical(canonical_key)
    _validate_key_parts(canonical_key, alias, parts)
    if alias == "root":
        base = output
    elif alias_dirs is not None and alias in alias_dirs:
        base = output.joinpath(*alias_dirs[alias])
    else:
        base = output / alias
    if not parts:
        return base / "init.luau"
    return base.joinpath(*parts[:-1], f"{parts[-1]}.luau")


def _plan_aliases(
    parsed: ParsedBundle,
) -> tuple[dict[str, tuple[str, ...]], dict[str, str]]:
    """Place every alias directory and derive the .luaurc to write, such
    that re-bundling the extracted tree re-derives the bundle's ALIAS table.

    Returns (alias_dirs, declared): alias_dirs maps each module-key alias
    (not root) to its directory parts under <output>; declared maps each
    .luaurc alias name to its target string.

    A bare mapping `@x -> @y` declares x as another name for y's directory.
    A tailed mapping `@a/p -> @b` pins b's directory INSIDE a's at offset p,
    so most-specific-alias canonicalization re-keys files under a/p as @b
    on re-bundle. Placements that contradict each other (an alias pinned in
    two places, or a placement cycle) are not expressible -> ExtractAliasError.
    """
    real_aliases = {_split_canonical(key)[0] for key in parsed.modules} - {"root"}

    bare: dict[str, str] = {}
    pin_by_target: dict[str, tuple[str, list[str]]] = {}
    for frm, to in parsed.remap.items():
        frm_alias, frm_parts = _split_canonical(frm)
        to_alias, _ = _split_canonical(to)
        _validate_key_parts(frm, frm_alias, frm_parts)
        if to_alias != "root" and to_alias not in real_aliases:
            raise ExtractAliasError(
                f"ALIAS {frm} -> {to}: target alias has no modules in this bundle"
            )
        if not frm_parts:
            bare[frm_alias] = to_alias
            continue
        if to_alias == "root":
            raise ExtractAliasError(
                f"ALIAS {frm} -> {to}: @root cannot live inside another alias"
            )
        if to_alias in pin_by_target:
            prior = _join(pin_by_target[to_alias])
            raise ExtractAliasError(
                f"alias @{to_alias} is pinned both inside {prior} and inside "
                f"{frm}; layout is not expressible"
            )
        pin_by_target[to_alias] = (frm_alias, frm_parts)

    def dir_of(alias: str, seen: tuple[str, ...] = ()) -> tuple[str, ...]:
        if alias == "root":
            return ()
        if alias in seen:
            raise ExtractAliasError(
                f"circular ALIAS placement involving @{alias}"
            )
        if alias in bare:
            return dir_of(bare[alias], (*seen, alias))
        pin = pin_by_target.get(alias)
        if pin is None:
            return (alias,)
        frm_alias, frm_parts = pin
        return dir_of(frm_alias, (*seen, alias)) + tuple(frm_parts)

    alias_dirs = {a: dir_of(a) for a in real_aliases}

    # .luaurc must declare every alias a require string can spell: module-key
    # aliases, plus every remap source's alias (bare or tailed).
    names = real_aliases | set(bare)
    for frm_alias, _parts in pin_by_target.values():
        if frm_alias != "root":
            names.add(frm_alias)
    declared: dict[str, str] = {}
    for name in sorted(names):
        d = dir_of(name)
        declared[name] = "/".join(d) if d else "."
    return alias_dirs, declared


def _join(pin: tuple[str, list[str]]) -> str:
    frm_alias, frm_parts = pin
    return "@" + "/".join([frm_alias, *frm_parts])


def extract_to_dir(
    parsed: ParsedBundle,
    out_fs: FSBackend,
    output: PurePath,
) -> None:
    main_key = parsed.fields.get("main")
    if not main_key:
        raise ExtractMissingMainError(
            "bundle has no MAIN directive; cannot place MAIN's body. "
            "Re-bundle with current code -- older bundles produced without "
            "MAIN are not extractable."
        )

    sources: dict[str, str] = {main_key: parsed.main_source}
    sources.update(parsed.modules)

    alias_dirs, declared = _plan_aliases(parsed)

    # A @root file landing inside an alias's directory would re-key under
    # that alias on re-bundle (most-specific alias wins), breaking the
    # byte-for-byte round-trip. Such bundles arise only from relocation:
    # the original alias dir was elsewhere; the extract layout creates the
    # overlap. Not expressible -> error. The check runs over every alias
    # the generated .luaurc declares -- a tailed remap source gets a
    # directory without owning any module keys, so alias_dirs alone is
    # not enough. MAIN is exempt: the bundler keys it root-relative by
    # fiat, so it never re-keys under an alias.
    shadow_dirs = {
        name: tuple(target.split("/"))
        for name, target in declared.items()
        if target != "."
    }
    for key in sources:
        if key == main_key:
            continue
        alias, parts = _split_canonical(key)
        if alias != "root":
            continue
        for owner, d in shadow_dirs.items():
            if len(parts) > len(d) and tuple(parts[: len(d)]) == d:
                raise ExtractAliasError(
                    f"@root key {key} extracts inside alias @{owner}'s "
                    f"directory {'/'.join(d)}; re-bundling would re-key it"
                )

    layout: dict[str, PurePath] = {
        key: physical_path_for_key(key, output, alias_dirs)
        for key in sources
    }

    seen: dict[PurePath, str] = {}
    for key, path in layout.items():
        prior = seen.get(path)
        if prior is not None:
            raise ExtractCollisionError(
                f"keys {prior!r} and {key!r} both map to {path}; "
                "rename one alias or restructure the bundle"
            )
        seen[path] = key

    # <leaf>.luau next to <leaf>/init.luau makes a re-bundle's require of
    # that leaf hit AmbiguousResolutionError; the layout is not hermetic.
    paths = set(layout.values())
    for path in paths:
        init_twin = path.with_suffix("") / "init.luau"
        if init_twin in paths:
            raise ExtractAmbiguityError(
                f"extract would write both {path} and {init_twin}; "
                "requires of that module become ambiguous on re-bundle"
            )

    luaurc_path = output / ".luaurc"

    pre_existing: list[PurePath] = [
        path for path in layout.values() if out_fs.is_file(path)
    ]
    if declared and out_fs.is_file(luaurc_path):
        pre_existing.append(luaurc_path)
    if pre_existing:
        listing = ", ".join(str(p) for p in pre_existing)
        raise ExtractClobberError(
            f"refusing to overwrite existing files: {listing}"
        )

    for key, path in layout.items():
        out_fs.write(path, sources[key])

    if declared:
        body = json.dumps({"aliases": declared}, indent=2) + "\n"
        out_fs.write(luaurc_path, body)
