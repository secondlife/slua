"""Reverse a parsed bundle into a self-contained on-disk project tree.

Layout convention: @root/... files land at <output>/, files under other
aliases at <output>/<alias>/. A .luaurc declaring every non-root alias is
written when any are present. The extracted tree is hermetic: re-bundling
it with `--root <output>` reproduces the original bundle byte-for-byte.
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
) -> PurePath:
    """Map @alias/path -> on-disk path under <output>.

    @root files go flat under <output>; other aliases go under a same-named
    subdir. Bare alias keys (stripped init.luau) become init.luau in the
    appropriate dir.
    """
    alias, parts = _split_canonical(canonical_key)
    _validate_key_parts(canonical_key, alias, parts)
    base = output if alias == "root" else output / alias
    if not parts:
        return base / "init.luau"
    return base.joinpath(*parts[:-1], f"{parts[-1]}.luau")


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

    layout: dict[str, PurePath] = {
        key: physical_path_for_key(key, output)
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

    luaurc_path = output / ".luaurc"
    aliases_for_luaurc = sorted({
        _split_canonical(key)[0]
        for key in sources
        if _split_canonical(key)[0] != "root"
    })

    pre_existing: list[PurePath] = [
        path for path in layout.values() if out_fs.is_file(path)
    ]
    if aliases_for_luaurc and out_fs.is_file(luaurc_path):
        pre_existing.append(luaurc_path)
    if pre_existing:
        listing = ", ".join(str(p) for p in pre_existing)
        raise ExtractClobberError(
            f"refusing to overwrite existing files: {listing}"
        )

    for key, path in layout.items():
        out_fs.write(path, sources[key])

    if aliases_for_luaurc:
        body = json.dumps(
            {"aliases": {a: a for a in aliases_for_luaurc}},
            indent=2,
        ) + "\n"
        out_fs.write(luaurc_path, body)
