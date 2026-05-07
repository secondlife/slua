from __future__ import annotations

import re
from collections import deque
from pathlib import PurePath

from .canonicalize import build_alias_map, canonicalize
from .errors import BundleError
from .fs import FSBackend, normalize
from .resolver import _split_canonical, resolve
from .runtime import parse_bundle

BUNDLE_VERSION = 1

MAX_DEPTH = 100
MAX_MODULES = 1000

REQUIRE_RE = re.compile(r'\brequire\s*\(\s*(?:"([^"]*)"|\'([^\']*)\')\s*\)')
_MARKER_RE = re.compile(r"^--\s*!!LUABUNDLE:", re.MULTILINE)


class MarkerInjectionError(BundleError):
    pass


class AmbiguousResolutionError(BundleError):
    """Both <leaf>.luau and <leaf>/init.luau exist for a resolved require."""


class NoResolverError(BundleError):
    """No resolver produced source and no copy was found in the existing bundle."""


class DepthExceededError(BundleError):
    """The require graph exceeds MAX_DEPTH levels from MAIN."""


class ModuleCountExceededError(BundleError):
    """The require graph exceeds MAX_MODULES total modules."""


def _check_no_marker_injection(origin: str, source: str) -> None:
    if _MARKER_RE.search(source):
        raise MarkerInjectionError(
            f"{origin} contains a line that looks like a bundle marker; "
            "source files must not contain '-- !!LUABUNDLE:' at the start of a line"
        )


def bundle(
    vfs: FSBackend,
    project_root: PurePath,
    main_path: PurePath,
    project_name: str | None = None,
    existing_bundle: str | None = None,
) -> str:
    """Bundle MAIN and everything it transitively requires.

    Pure-trace: starts at MAIN, regex-finds `require()` calls, resolves each
    to a physical file, and enqueues unvisited targets. Files unreachable
    from MAIN never enter the bundle. Aliases come from project_root's
    .luaurc only (nested .luaurc files are ignored for reproducibility);
    the built-in @root alias always covers project_root.

    project_name, if provided, is emitted as the bundle's PROJECT directive
    (advisory viewer-linkage metadata) but is not used for canonicalization.

    existing_bundle, if provided, acts as the universal last-resort resolver:
    when an alias has no on-disk source (or no .luaurc entry), the bundler
    falls back to the source already embedded in that bundle. Disk wins
    when both are available.
    """
    project_root = normalize(project_root)
    main_path = normalize(main_path)

    alias_map = build_alias_map(vfs, project_root)
    main_key = canonicalize(vfs, main_path, project_root)
    main_source = vfs.read(main_path)
    _check_no_marker_injection(str(main_path), main_source)

    fallback_modules: dict[str, str] = {}
    if existing_bundle is not None:
        parsed = parse_bundle(existing_bundle)
        fallback_modules.update(parsed.modules)
        fallback_modules[parsed.fields["main"]] = parsed.main_source

    known_aliases = set(alias_map.keys())
    for key in fallback_modules:
        known_aliases.add(key[1:].split("/", 1)[0])

    visited: dict[str, str] = {}
    queue: deque[tuple[str, str, int]] = deque()
    queue.append((main_key, main_source, 0))
    enqueued: set[str] = {main_key}

    while queue:
        key, source, depth = queue.popleft()
        if depth > MAX_DEPTH:
            raise DepthExceededError(
                f"require depth {depth} exceeds maximum {MAX_DEPTH} at module {key}"
            )
        if key in visited:
            continue
        visited[key] = source

        for dq, sq in REQUIRE_RE.findall(source):
            req_str = dq or sq
            target_key, target_source = _resolve_source(
                vfs, alias_map, fallback_modules, project_root,
                key, req_str, known_aliases,
            )
            if target_key in enqueued:
                continue
            _check_no_marker_injection(target_key, target_source)
            if len(enqueued) >= MAX_MODULES:
                raise ModuleCountExceededError(
                    f"bundle exceeds maximum of {MAX_MODULES} modules"
                )
            enqueued.add(target_key)
            queue.append((target_key, target_source, depth + 1))

    main_body = visited.pop(main_key)

    parts: list[str] = [f"-- !!LUABUNDLE:VERSION {BUNDLE_VERSION}\n"]
    if project_name is not None:
        parts.append(f"-- !!LUABUNDLE:PROJECT {project_name}\n")
    parts.append(f"-- !!LUABUNDLE:MAIN {main_key}\n")
    parts.append("-- !!LUABUNDLE:BODY\n")
    parts.append(_terminate(main_body))
    for key, source in visited.items():
        parts.append(f"-- !!LUABUNDLE:MODULE {key}\n")
        parts.append(_terminate(source))
    return "".join(parts)


def _resolve_source(
    vfs: FSBackend,
    alias_map: dict[str, PurePath],
    fallback_modules: dict[str, str],
    project_root: PurePath,
    requirer_anchor_key: str,
    require_str: str,
    known_aliases: set[str],
) -> tuple[str, str]:
    """Resolve a require() string to (canonical_key, source).

    Disk resolver wins; the fallback bundle's embedded copy is the universal
    last resort (RFC: 'Resolver Behaviour'). When the alias isn't even known
    to disk, fall back directly.

    On disk hits, re-canonicalize from the physical path so colliding aliases
    (two .luaurc entries targeting the same directory) de-dup to a single
    canonical key and the collision warning fires from canonicalize().
    """
    target_key = resolve(require_str, requirer_anchor_key, known_aliases)
    alias, rel_parts = _split_canonical(target_key)

    if alias in alias_map:
        target_path = _luau_resolve_to_file(vfs, alias_map[alias], rel_parts)
        if target_path is not None:
            canonical_key = canonicalize(vfs, target_path, project_root)
            return canonical_key, vfs.read(target_path)

    if target_key in fallback_modules:
        return target_key, fallback_modules[target_key]

    raise NoResolverError(
        f"cannot resolve module '{require_str}' to '{target_key}': "
        "no resolver produced source and no copy in existing bundle"
    )


def _luau_resolve_to_file(
    vfs: FSBackend,
    base_dir: PurePath,
    rel_parts: list[str],
) -> PurePath | None:
    """Apply Luau's file resolution: try <base>/.../<leaf>.luau, then <base>/.../<leaf>/init.luau.

    Returns None when no file is found - the caller decides whether to try
    another resolver (existing-bundle fallback) or surface NoResolverError.
    Raises AmbiguousResolutionError when both candidate files exist.
    """
    if not rel_parts:
        # `@SomeAlias` with no tail: the alias root itself is the module.
        # Look for init.luau directly in the alias root.
        candidate = base_dir / "init.luau"
        return candidate if vfs.is_file(candidate) else None

    leaf = rel_parts[-1]
    parent_dir = base_dir
    for p in rel_parts[:-1]:
        parent_dir = parent_dir / p

    leaf_file = parent_dir / f"{leaf}.luau"
    init_file = parent_dir / leaf / "init.luau"

    leaf_exists = vfs.is_file(leaf_file)
    init_exists = vfs.is_file(init_file)

    if leaf_exists and init_exists:
        raise AmbiguousResolutionError(
            f"both {leaf_file} and {init_file} exist; remove one to disambiguate"
        )
    if leaf_exists:
        return leaf_file
    if init_exists:
        return init_file
    return None


def _terminate(content: str) -> str:
    """Force exactly one trailing newline so the next marker lands at column 0.

    Body content is otherwise preserved byte-for-byte.
    """
    return content if content.endswith("\n") else content + "\n"
