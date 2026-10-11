"""MemoryFS path normalization at construction and lookup."""

from pathlib import PurePosixPath

import pytest

from slua_bundle import MemoryFS


def test_from_dict_normalizes_keys():
    vfs = MemoryFS.from_dict({"/a/../b/foo.luau": "hello"})
    assert PurePosixPath("/b/foo.luau") in vfs.files
    assert PurePosixPath("/a/../b/foo.luau") not in vfs.files


def test_read_normalizes_lookup():
    vfs = MemoryFS.from_dict({"/b/foo.luau": "hello"})
    assert vfs.read(PurePosixPath("/c/../b/foo.luau")) == "hello"
    assert vfs.read(PurePosixPath("/b/./foo.luau")) == "hello"


def test_is_file_normalizes():
    vfs = MemoryFS.from_dict({"/x/y.luau": "."})
    assert vfs.is_file(PurePosixPath("/x/./y.luau"))
    assert not vfs.is_file(PurePosixPath("/x/missing.luau"))


def test_is_dir_implicit_from_children():
    vfs = MemoryFS.from_dict({"/proj/src/main.luau": "."})
    assert vfs.is_dir(PurePosixPath("/proj"))
    assert vfs.is_dir(PurePosixPath("/proj/src"))
    assert vfs.is_dir(PurePosixPath("/proj/./src"))
    assert not vfs.is_dir(PurePosixPath("/proj/src/main.luau"))
    assert not vfs.is_dir(PurePosixPath("/missing"))


def test_read_missing_raises():
    vfs = MemoryFS.from_dict({"/a.luau": "."})
    with pytest.raises(FileNotFoundError):
        vfs.read(PurePosixPath("/b.luau"))


def test_string_inputs_accepted():
    """All public MemoryFS methods accept either a string or a PurePosixPath."""
    vfs = MemoryFS.from_dict({"/proj/src/main.luau": "hello"})
    assert vfs.is_file("/proj/src/main.luau")
    assert vfs.is_dir("/proj/src")
    assert vfs.read("/proj/src/main.luau") == "hello"
    assert vfs.read("/proj/./src/../src/main.luau") == "hello"
