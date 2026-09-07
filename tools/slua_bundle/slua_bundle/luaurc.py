from __future__ import annotations

import json
import re
from pathlib import PurePath

from .errors import BundleError
from .fs import FSBackend, normalize
from .resolver import ascii_lower

CONFIG_NAME = ".luaurc"


class InvalidLuaurcError(BundleError):
    """A .luaurc file exists but is not valid JSONC."""

# Mirror upstream isValidAlias (Config.cpp:166): ASCII alphanumerics
# plus '-', '_', '.'; never '.', '..', or anything with a path
# separator. Upstream also tolerates a leading '@' but then stores the
# name verbatim, so such an alias never matches a require -- reject it
# outright instead of inheriting the trap.
_ALIAS_NAME_RE = re.compile(r"[A-Za-z0-9._-]+\Z")


# Match Luau's actual .luaurc parser (Config/src/Config.cpp), which
# tokenizes with the Luau lexer: // and Lua-style -- line comments and
# --[[ ]] long comments (any =-level, same closer-matching rule as the
# Lua lexer) are skipped, and trailing commas in objects/arrays are
# allowed. C block comments (`/* */`) are NOT accepted by Luau and stay
# in the stream so json.loads fails on them.
_LONG_OPENER_RE = re.compile(r"--\[(=*)\[")


def _strip_jsonc(s: str, context: str = CONFIG_NAME) -> str:
    """Reduce JSONC-with-Lua-comments to plain JSON for json.loads.

    A single-pass character scanner, mirroring upstream's lexer-based
    consumption. Whitespace and newlines are preserved (long comments
    are replaced by their newlines) so JSONDecodeError line numbers
    still point at the real source. A comma is held back until the next
    significant character: dropped if that closes a container (trailing
    comma), emitted otherwise -- this also handles commas separated from
    their closer by comments.
    """
    out: list[str] = []
    comma_held = False
    i = 0
    n = len(s)
    while i < n:
        c = s[i]

        if c in " \t\r\n":
            out.append(c)
            i += 1
            continue

        if c == "/" and s.startswith("//", i):
            j = s.find("\n", i)
            i = n if j == -1 else j
            continue

        if c == "-" and s.startswith("--", i):
            opener = _LONG_OPENER_RE.match(s, i)
            if opener is None:
                j = s.find("\n", i)
                i = n if j == -1 else j
                continue
            closer = "]" + opener.group(1) + "]"
            end = s.find(closer, opener.end())
            if end == -1:
                raise InvalidLuaurcError(
                    f"{context}: unterminated long comment (--[[ without matching ]])"
                )
            stop = end + len(closer)
            newlines = s.count("\n", i, stop)
            out.append("\n" * newlines if newlines else " ")
            i = stop
            continue

        if c == ",":
            if comma_held:
                # ",," is invalid JSON regardless; pass the first through.
                out.append(",")
            comma_held = True
            i += 1
            continue

        if comma_held:
            comma_held = False
            if c not in "}]":
                out.append(",")

        if c == '"':
            j = i + 1
            while j < n and s[j] != '"':
                j += 2 if s[j] == "\\" else 1
            j = min(j + 1, n)
            out.append(s[i:j])
            i = j
            continue

        out.append(c)
        i += 1

    if comma_held:
        out.append(",")
    return "".join(out)


def load_config(vfs: FSBackend, config_dir: PurePath) -> dict[str, PurePath]:
    config_path = config_dir / CONFIG_NAME
    if not vfs.is_file(config_path):
        return {}
    try:
        raw = json.loads(_strip_jsonc(vfs.read(config_path), str(config_path)))
    except json.JSONDecodeError as e:
        raise InvalidLuaurcError(
            f"{config_path}: invalid JSONC ({e.msg} at line {e.lineno}, column {e.colno})"
        ) from e
    aliases = raw.get("aliases", {})
    out: dict[str, PurePath] = {}
    for name, target in aliases.items():
        if name in (".", "..") or not _ALIAS_NAME_RE.fullmatch(name):
            raise InvalidLuaurcError(
                f"{config_path}: invalid alias name {name!r} (allowed: ASCII "
                "letters, digits, '-', '_', '.'; no path separators; declare "
                "without a leading '@')"
            )
        # Alias names are case-insensitive, matching upstream Luau
        # (Config.cpp folds to lowercase at load).
        folded = ascii_lower(name)
        if folded in out:
            raise InvalidLuaurcError(
                f"{config_path}: alias {name!r} collides with another alias "
                "after case folding (alias names are case-insensitive)"
            )
        target_path = vfs.Path(target)
        if target_path.is_absolute():
            out[folded] = normalize(target_path)
        else:
            out[folded] = normalize(config_dir / target_path)
    return out


