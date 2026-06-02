from __future__ import annotations

import warnings
from pathlib import PurePath

from .errors import BundleError
from .fs import FSBackend, normalize
from .luaurc import load_config

RESERVED_ALIASES = frozenset({"root", "self"})


class NoCoveringAliasError(BundleError):
    pass


class ReservedAliasError(BundleError):
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
    (currently just 'root') cannot be declared in .luaurc.
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


def canonicalize(
    vfs: FSBackend,
    file_path: PurePath,
    project_root: PurePath,
) -> str:
    file_path = normalize(file_path)
    project_root = normalize(project_root)

    aliases = build_alias_map(vfs, project_root)
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

    rel = file_path.relative_to(target_dir)
    rel_parts = list(rel.with_suffix("").parts)
    if rel_parts and rel_parts[-1] == "init":
        rel_parts.pop()

    if not rel_parts:
        return f"@{alias_name}"
    return f"@{alias_name}/" + "/".join(rel_parts)
