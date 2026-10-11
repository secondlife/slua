from __future__ import annotations

import warnings
from pathlib import PurePath

from .errors import BundleError, ReservedAliasError
from .fs import FSBackend, normalize
from .luaurc import load_config

RESERVED_ALIASES = frozenset({"root", "self", "sl"})


class NoCoveringAliasError(BundleError):
    pass


class AliasCollisionWarning(UserWarning):
    pass


def _is_prefix(prefix: PurePath, path: PurePath) -> bool:
    if prefix == path:
        return True
    return prefix in path.parents


def build_alias_map(
    vfs: FSBackend,
    project_root: PurePath,
) -> dict[str, PurePath]:
    """Project-root .luaurc + the built-in @root alias.

    Nested .luaurc files are ignored: alias set must be a function of the
    project tree alone for bundle reproducibility. Reserved alias names
    cannot be declared in .luaurc.
    """
    project_root = normalize(project_root)
    aliases = load_config(vfs, project_root)
    for reserved in RESERVED_ALIASES:
        if reserved in aliases:
            raise ReservedAliasError(
                f"alias '{reserved}' is reserved and cannot be declared in .luaurc"
            )
    aliases["root"] = project_root
    return aliases


def key_from_relpath(alias_name: str, rel: PurePath) -> str:
    """Build a canonical key from an alias name and a path relative to its dir.

    Strips the .luau suffix and an `init` leaf (a directory's init.luau is
    the module named by the directory itself).
    """
    rel_parts = list(rel.with_suffix("").parts)
    if rel_parts and rel_parts[-1] == "init":
        rel_parts.pop()
    if not rel_parts:
        return f"@{alias_name}"
    return f"@{alias_name}/" + "/".join(rel_parts)


def canonicalize(
    vfs: FSBackend,
    file_path: PurePath,
    project_root: PurePath,
    alias_map: dict[str, PurePath] | None = None,
) -> str:
    file_path = normalize(file_path)
    project_root = normalize(project_root)

    aliases = build_alias_map(vfs, project_root) if alias_map is None else alias_map
    covering = [(target, name) for name, target in aliases.items() if _is_prefix(target, file_path)]
    if not covering:
        raise NoCoveringAliasError(
            f"no alias spans {file_path}, add a .luaurc entry covering it"
        )

    max_depth = max(len(t.parts) for t, _ in covering)
    most_specific = [(t, n) for t, n in covering if len(t.parts) == max_depth]
    # Two aliases at identical depth that both cover the same file must share
    # the same target path (only one directory at any given depth can be an
    # ancestor of a given file), so target_dir below is unambiguous.
    target_dir = most_specific[0][0]

    if len(most_specific) > 1:
        names = sorted(n for _, n in most_specific)
        if "root" in names:
            alias_name = "root"
        else:
            alias_name = names[0]
            warnings.warn(
                f"multiple .luaurc aliases target the same directory {target_dir}: "
                f"{names}; using '{alias_name}' (ASCII-first) for canonical keys",
                AliasCollisionWarning,
                stacklevel=2,
            )
    else:
        alias_name = most_specific[0][1]

    return key_from_relpath(alias_name, file_path.relative_to(target_dir))
