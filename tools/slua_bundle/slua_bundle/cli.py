"""Command-line interface to the bundler.

Subcommands:
  bundle  -- produce a bundle from a project on disk
  inspect -- pretty-print a bundle's structure
  extract -- reverse a bundle into a self-contained project tree

Errors raised by the library are caught at the top of `main()` and
printed to stderr with exit code 1; unexpected exceptions propagate so
stack traces stay visible during prototype development.
"""

from __future__ import annotations

import argparse
import pathlib
import sys

from .bundler import bundle
from .errors import BundleError
from .extractor import extract_to_dir
from .fs import DiskFS, SourceDecodeError
from .runtime import parse_bundle, simulate


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="python -m slua_bundle")
    sub = parser.add_subparsers(dest="cmd", required=True)

    p_bundle = sub.add_parser("bundle", help="Bundle a project tree")
    p_bundle.add_argument("--project", default=None, help="Optional advisory project name (emitted as PROJECT directive for viewer linkage)")
    p_bundle.add_argument("--root", required=True, type=pathlib.Path, help="Project root directory (covered by built-in @root alias)")
    p_bundle.add_argument("--input-bundle", dest="input_bundle", type=pathlib.Path, default=None, help="Existing bundle whose embedded modules act as a last-resort resolver when disk sources are missing")
    p_bundle.add_argument("-o", "--output", type=pathlib.Path, default=None, help="Write bundle to this file (default: stdout)")
    p_bundle.add_argument("main", type=pathlib.Path, help="Path to MAIN .luau file")

    p_inspect = sub.add_parser("inspect", help="Show a bundle's structure")
    p_inspect.add_argument("bundle_file", type=pathlib.Path, nargs="?", default=None, help="Bundle file to inspect (default: stdin)")

    p_extract = sub.add_parser("extract", help="Reverse a bundle into a project tree")
    p_extract.add_argument("-o", "--output", required=True, type=pathlib.Path, help="Output directory")
    p_extract.add_argument("bundle_file", type=pathlib.Path, help="Bundle file to extract")

    return parser


def _read_text_utf8(p: pathlib.Path) -> str:
    try:
        return p.read_text(encoding="utf-8")
    except UnicodeDecodeError as e:
        raise SourceDecodeError(f"{p}: not valid UTF-8 ({e})") from e


def _cmd_bundle(args: argparse.Namespace) -> int:
    disk = DiskFS(args.root)
    existing = _read_text_utf8(args.input_bundle) if args.input_bundle else None
    text = bundle(
        disk,
        project_root=args.root.resolve(),
        main_path=args.main.resolve(),
        project_name=args.project,
        existing_bundle=existing,
    )
    # Self-check before the artifact ships: every require in the emitted
    # bundle must resolve through the bundle alone (the server has nothing
    # else), and the walk also surfaces cycles the BFS absorbs silently.
    simulate(text)
    if args.output:
        args.output.write_text(text, encoding="utf-8")
    else:
        sys.stdout.write(text)
    return 0


def _read_bundle_text(arg: pathlib.Path | None) -> str:
    if arg is None:
        return sys.stdin.read()
    return _read_text_utf8(arg)


def _cmd_inspect(args: argparse.Namespace) -> int:
    text = _read_bundle_text(args.bundle_file)
    parsed = parse_bundle(text)
    print(f"VERSION {parsed.fields.get('version', '?')}")
    project = parsed.fields.get('project')
    print(f"PROJECT {project}" if project else "PROJECT <none>")
    main_key = parsed.fields.get("main")
    main_size = len(parsed.main_source.encode("utf-8"))
    if main_key:
        print(f"MAIN {main_key} ({main_size} bytes)")
    else:
        print(f"MAIN <none> ({main_size} bytes)")
    for frm, to in sorted(parsed.remap.items()):
        print(f"ALIAS {frm} -> {to}")
    modules = sorted(parsed.modules.items())
    total = sum(len(s.encode("utf-8")) for _, s in modules)
    print(f"MODULES ({len(modules)}, total {total} bytes):")
    for key, source in modules:
        print(f"  {key} ({len(source.encode('utf-8'))} bytes)")
    return 0


def _cmd_extract(args: argparse.Namespace) -> int:
    text = _read_text_utf8(args.bundle_file)
    parsed = parse_bundle(text)
    extract_to_dir(parsed, DiskFS(args.output), args.output.resolve())
    n = 1 + len(parsed.modules)
    print(f"extracted {n} modules to {args.output}")
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = _build_parser()
    args = parser.parse_args(argv)
    handlers = {
        "bundle": _cmd_bundle,
        "inspect": _cmd_inspect,
        "extract": _cmd_extract,
    }
    try:
        return handlers[args.cmd](args)
    except BundleError as e:
        print(f"error: {e}", file=sys.stderr)
        return 1
