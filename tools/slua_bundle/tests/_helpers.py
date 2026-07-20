"""Shared test helpers."""

from __future__ import annotations

import json


def luaurc(aliases: dict[str, str]) -> str:
    """Render a minimal .luaurc body. Use raw strings for JSONC-shape tests."""
    return json.dumps({"aliases": aliases})
