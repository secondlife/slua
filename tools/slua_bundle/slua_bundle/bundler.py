from __future__ import annotations

import re
from collections import deque
from pathlib import PurePath

from .canonicalize import build_alias_map, canonicalize, key_from_relpath
from .errors import BundleError
from .fs import FSBackend, normalize
from .resolver import _join_canonical, _split_canonical, apply_remap, iter_requires, resolve
from .runtime import parse_bundle

BUNDLE_VERSION = 1

MAX_DEPTH = 100
MAX_MODULES = 1000

_MARKER_RE = re.compile(r"^--\s*!!LUABUNDLE:", re.MULTILINE)


class MarkerInjectionError(BundleError):
    pass


class MainOutsideRootError(BundleError):
    """main_path is not under project_root; MAIN keys are always @root-relative."""


class MainFileError(BundleError):
    """main_path is missing, not a regular file, or not a .luau file."""


class RemapConflictError(BundleError):
    """A syntactic prefix would remap to two different targets, or a remapped
    prefix collides with a real module key. Only possible when a fallback
    bundle's alias universe disagrees with the on-disk .luaurc."""


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

    MAIN must live under project_root and is keyed by its root-relative
    path, ignoring aliases -- "MAIN is under @root" is a format invariant.

    Modules are keyed canonically (most-specific covering alias of the
    physical file). Whenever a require string's syntactic key -- what the
    server computes from the string and the requirer's anchor, with no
    .luaurc -- diverges from the canonical key, the divergence factors into
    a prefix mapping emitted as a header ALIAS directive; the server
    rewrites absolutized require keys through that table at compile time.

    project_name, if provided, is emitted as the bundle's PROJECT directive
    (advisory viewer-linkage metadata) but is not used for canonicalization.

    existing_bundle, if provided, acts as the universal last-resort resolver:
    when an alias has no on-disk source (or no .luaurc entry), the bundler
    falls back to the source already embedded in that bundle, applying that
    bundle's own ALIAS table first. Disk wins when both are available.
    """
    project_root = normalize(project_root)
    main_path = normalize(main_path)

    if not vfs.is_file(main_path):
        raise MainFileError(f"MAIN {main_path} does not exist or is not a file")
    if main_path.suffix != ".luau":
        raise MainFileError(f"MAIN {main_path} must be a .luau file")

    alias_map = build_alias_map(vfs, project_root)
    try:
        main_rel = main_path.relative_to(project_root)
    except ValueError:
        raise MainOutsideRootError(
            f"MAIN {main_path} is not under project root {project_root}; "
            "choose a project root that contains the entry script"
        ) from None
    main_key = key_from_relpath("root", main_rel)
    main_source = vfs.read(main_path)
    _check_no_marker_injection(str(main_path), main_source)

    fallback_modules: dict[str, str] = {}
    fallback_remap: dict[str, str] = {}
    if existing_bundle is not None:
        parsed = parse_bundle(existing_bundle)
        fallback_modules.update(parsed.modules)
        fallback_modules[parsed.fields["main"]] = parsed.main_source
        fallback_remap = parsed.remap

    known_aliases = set(alias_map.keys())
    for key in fallback_modules:
        known_aliases.add(key[1:].split("/", 1)[0])
    for frm in fallback_remap:
        known_aliases.add(frm[1:].split("/", 1)[0])

    visited: dict[str, str] = {}
    remap: dict[str, str] = {}
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

        for req_str in iter_requires(source):
            target_key, target_source, syntactic_key = _resolve_source(
                vfs, alias_map, fallback_modules, fallback_remap, project_root,
                key, req_str, known_aliases,
            )
            if syntactic_key != target_key:
                frm, to = _derive_remap(syntactic_key, target_key)
                prior = remap.setdefault(frm, to)
                if prior != to:
                    raise RemapConflictError(
                        f"prefix {frm} would remap to both {prior} and {to}"
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

    # Module keys are canonical and therefore never live under a remapped
    # prefix within one alias universe; a violation means the fallback
    # bundle's universe disagrees with the on-disk .luaurc. MAIN is exempt:
    # it is root-relative by fiat and never a lookup target.
    for frm in remap:
        prefix = frm + "/"
        for key in visited:
            if key != main_key and (key == frm or key.startswith(prefix)):
                raise RemapConflictError(
                    f"remapped prefix {frm} collides with module key {key}"
                )

    main_body = visited.pop(main_key)

    parts: list[str] = [f"-- !!LUABUNDLE:VERSION {BUNDLE_VERSION}\n"]
    if project_name is not None:
        parts.append(f"-- !!LUABUNDLE:PROJECT {project_name}\n")
    parts.append(f"-- !!LUABUNDLE:MAIN {main_key}\n")
    for frm in sorted(remap):
        parts.append(f"-- !!LUABUNDLE:ALIAS {frm} {remap[frm]}\n")
    parts.append("-- !!LUABUNDLE:BODY\n")
    parts.append(_terminate(main_body))
    for key, source in visited.items():
        parts.append(f"-- !!LUABUNDLE:MODULE {key}\n")
        parts.append(_terminate(source))
    return "".join(parts)


def _derive_remap(syntactic_key: str, canonical_key: str) -> tuple[str, str]:
    """Factor a syntactic-vs-canonical divergence into a prefix mapping.

    canonical = @B/tail and syntactic = @A/p1/../pk/tail share their tail
    (both spell the same file relative to B's directory), so the mapping is
    (syntactic minus the tail) -> @B. Strip by count: suffix matching would
    over-strip when an alias is named after its own directory.
    """
    syn_alias, syn_parts = _split_canonical(syntactic_key)
    canon_alias, canon_parts = _split_canonical(canonical_key)
    cut = len(syn_parts) - len(canon_parts)
    if cut < 0 or syn_parts[cut:] != canon_parts:
        raise RemapConflictError(
            f"cannot remap {syntactic_key} to {canonical_key}: keys do not "
            "share a tail (conflicting alias universes?)"
        )
    return _join_canonical(syn_alias, syn_parts[:cut]), f"@{canon_alias}"


def _resolve_source(
    vfs: FSBackend,
    alias_map: dict[str, PurePath],
    fallback_modules: dict[str, str],
    fallback_remap: dict[str, str],
    project_root: PurePath,
    requirer_anchor_key: str,
    require_str: str,
    known_aliases: set[str],
) -> tuple[str, str, str]:
    """Resolve a require() string to (canonical_key, source, syntactic_key).

    The syntactic key is what resolve() derives from the require string and
    the requirer's anchor alone -- exactly what the server-side compiler
    computes, with no .luaurc in sight. The canonical key re-derives from
    the physical path so colliding aliases de-dup to a single key (and the
    collision warning fires from canonicalize()). The caller records a
    prefix mapping whenever the two differ.

    Disk resolver wins; the fallback bundle's embedded copy is the universal
    last resort (RFC: 'Resolver Behaviour'). Fallback lookup goes through
    the old bundle's own ALIAS table, so any spelling under a remapped
    prefix resolves -- not just spellings observed when it was built.
    """
    target_key = resolve(require_str, requirer_anchor_key, known_aliases)
    alias, rel_parts = _split_canonical(target_key)

    if alias in alias_map:
        target_path = _luau_resolve_to_file(vfs, alias_map[alias], rel_parts)
        if target_path is not None:
            canonical_key = canonicalize(vfs, target_path, project_root, alias_map)
            return canonical_key, vfs.read(target_path), target_key

    fb_key = apply_remap(target_key, fallback_remap)
    if fb_key in fallback_modules:
        return fb_key, fallback_modules[fb_key], target_key

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
    """Ensure a trailing newline so the next marker lands at column 0.

    Body content is otherwise preserved byte-for-byte; existing trailing
    newlines are kept as-is.
    """
    return content if content.endswith("\n") else content + "\n"
