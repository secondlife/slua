from __future__ import annotations

import json
import re
from pathlib import PurePath

from .errors import BundleError
from .fs import FSBackend, normalize

CONFIG_NAME = ".luaurc"


class InvalidLuaurcError(BundleError):
    """A .luaurc file exists but is not valid JSONC."""

# Match Luau's actual .luaurc parser (Config/src/Config.cpp): // line
# comments and trailing commas in objects/arrays. Block comments
# (`/* */`) are NOT accepted by Luau and stay in the stream so
# json.loads fails on them.
#
# Two passes so a trailing comma hidden behind a line comment isn't
# missed -- a single combined regex consumes the comment and never
# revisits the comma underneath. String literals are preserved by the
# alternation in each pattern.
_LINE_COMMENT_RE = re.compile(r'"(?:\\.|[^"\\])*"|//[^\n]*')
_TRAILING_COMMA_RE = re.compile(r'"(?:\\.|[^"\\])*"|,(?=\s*[}\]])')


def _strip_jsonc(s: str) -> str:
    def keep_strings(m: re.Match[str]) -> str:
        text = m.group(0)
        return text if text.startswith('"') else ""
    s = _LINE_COMMENT_RE.sub(keep_strings, s)
    s = _TRAILING_COMMA_RE.sub(keep_strings, s)
    return s


def load_config(vfs: FSBackend, config_dir: PurePath) -> dict[str, PurePath]:
    config_path = config_dir / CONFIG_NAME
    if not vfs.is_file(config_path):
        return {}
    try:
        raw = json.loads(_strip_jsonc(vfs.read(config_path)))
    except json.JSONDecodeError as e:
        raise InvalidLuaurcError(
            f"{config_path}: invalid JSONC ({e.msg} at line {e.lineno}, column {e.colno})"
        ) from e
    aliases = raw.get("aliases", {})
    out: dict[str, PurePath] = {}
    for name, target in aliases.items():
        target_path = vfs.Path(target)
        if target_path.is_absolute():
            out[name] = normalize(target_path)
        else:
            out[name] = normalize(config_dir / target_path)
    return out


