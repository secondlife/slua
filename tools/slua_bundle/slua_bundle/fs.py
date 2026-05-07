"""
Filesystem backends for the bundler.

Two backends ship: MemoryFS (dict-backed, used by tests) and DiskFS
(real on-disk projects). Each declares a `Path` class attribute -- the
PurePath subclass it works with -- so application code can stay generic
over `PurePath` without mixing POSIX and Windows path types within a
single backend.

Bundle keys are constructed from `PurePath.parts`, which returns plain
string tuples for any subclass, so the wire format is POSIX-shaped on
every host.
"""

from __future__ import annotations

import os
import pathlib
from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from pathlib import PurePath, PurePosixPath
from typing import ClassVar, Iterator


def _is_anchor(s: str) -> bool:
    return s in ("/", "\\") or s.endswith((":\\", ":/"))


def normalize(p: PurePath) -> PurePath:
    """Collapse `.` and `..` segments, preserving the path subclass."""
    cls = type(p)
    parts: list[str] = []
    for part in p.parts:
        if part == "..":
            if parts and parts[-1] != ".." and not _is_anchor(parts[-1]):
                parts.pop()
            else:
                parts.append(part)
        elif part == ".":
            continue
        else:
            parts.append(part)
    if not parts:
        return cls(".")
    return cls(*parts)


class FSBackend(ABC):
    Path: ClassVar[type[PurePath]]

    @abstractmethod
    def is_file(self, path: PurePath | str) -> bool: ...

    @abstractmethod
    def read(self, path: PurePath | str) -> str: ...

    @abstractmethod
    def is_dir(self, path: PurePath | str) -> bool: ...

    @abstractmethod
    def iter_files(self) -> Iterator[PurePath]: ...

    @abstractmethod
    def write(self, path: PurePath | str, content: str) -> None:
        """Write `content` to `path`, creating parent dirs as needed."""

    def to_path(self, p: PurePath | str) -> PurePath:
        if isinstance(p, str):
            p = self.Path(p)
        return normalize(p)


@dataclass
class MemoryFS(FSBackend):
    """
    In-memory dict-backed filesystem. Used by tests for deterministic,
    cross-platform behavior. Keys are normalized at construction and at
    lookup, mimicking how `open()` resolves `..`/`.` during traversal.
    """

    Path: ClassVar[type[PurePath]] = PurePosixPath
    files: dict[PurePosixPath, str] = field(default_factory=dict)

    @classmethod
    def from_dict(cls, d: dict[str, str]) -> "MemoryFS":
        return cls({_as_posix(k): v for k, v in d.items()})

    def is_file(self, path: PurePath | str) -> bool:
        return _as_posix(path) in self.files

    def read(self, path: PurePath | str) -> str:
        key = _as_posix(path)
        if key not in self.files:
            raise FileNotFoundError(f"not a file: {path}")
        return self.files[key]

    def is_dir(self, path: PurePath | str) -> bool:
        key = _as_posix(path)
        return any(key in p.parents for p in self.files)

    def iter_files(self) -> Iterator[PurePosixPath]:
        return iter(self.files.keys())

    def write(self, path: PurePath | str, content: str) -> None:
        self.files[_as_posix(path)] = content


def _as_posix(path: PurePath | str) -> PurePosixPath:
    if isinstance(path, str):
        path = PurePosixPath(path)
    elif not isinstance(path, PurePosixPath):
        path = PurePosixPath(*path.parts)
    result = normalize(path)
    assert isinstance(result, PurePosixPath)
    return result


class DiskFS(FSBackend):
    """
    Real on-disk filesystem rooted at a single directory.

    Path type is `pathlib.Path` (PosixPath on POSIX, WindowsPath on NT --
    both inherit from PurePath). Paths passed in are typically absolute
    paths under the root; paths returned from `iter_files()` are
    absolute.

    Symlinks are followed: a symlink in a developer's project is
    intentional, pathlib detects cycles, and any file landing outside an
    alias root surfaces cleanly via NoCoveringAliasError.
    """

    Path: ClassVar[type[PurePath]] = pathlib.Path

    def __init__(self, root: pathlib.Path) -> None:
        self._root = root.resolve()

    @property
    def root(self) -> pathlib.Path:
        return self._root

    def _coerce(self, path: PurePath | str) -> pathlib.Path:
        if isinstance(path, str):
            return pathlib.Path(path)
        if isinstance(path, pathlib.Path):
            return path
        return pathlib.Path(*path.parts)

    def is_file(self, path: PurePath | str) -> bool:
        return self._coerce(path).is_file()

    def read(self, path: PurePath | str) -> str:
        return self._coerce(path).read_text()

    def is_dir(self, path: PurePath | str) -> bool:
        return self._coerce(path).is_dir()

    def iter_files(self) -> Iterator[pathlib.Path]:
        # Skip hidden directories (.git/, node_modules-style hidden trees,
        # etc.) and hidden files except .luaurc, which carries config we
        # need. Dotfile filtering keeps real-world projects bundleable
        # without spurious MarkerInjectionError hits or perf cliffs.
        for cur, dirs, files in os.walk(self._root, followlinks=True):
            dirs[:] = [d for d in dirs if not d.startswith(".")]
            cur_path = pathlib.Path(cur)
            for name in files:
                if name.startswith(".") and name != ".luaurc":
                    continue
                yield cur_path / name

    def write(self, path: PurePath | str, content: str) -> None:
        target = self._coerce(path)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(content)
