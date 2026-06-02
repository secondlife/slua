from __future__ import annotations

import re
from dataclasses import dataclass
from typing import Iterator

from .errors import BundleError
from .resolver import resolve

SUPPORTED_VERSIONS = frozenset({1})

MARKER_RE = re.compile(r"^--\s*!!LUABUNDLE:(\S+)(?:\s+(.+?))?\s*$")
REQUIRE_RE = re.compile(r'\brequire\s*\(\s*"([^"]*)"\s*\)')


class BundleParseError(BundleError):
    pass


class CircularDependencyError(BundleError):
    pass


@dataclass
class ParsedBundle:
    fields: dict[str, str]
    main_source: str
    modules: dict[str, str]


@dataclass(frozen=True)
class _Line:
    lineno: int
    raw: str
    marker_kind: str | None
    marker_arg: str

    @property
    def is_marker(self) -> bool:
        return self.marker_kind is not None


def _scan(text: str) -> Iterator[_Line]:
    for lineno, raw in enumerate(text.splitlines(keepends=True), start=1):
        stripped = raw.rstrip("\n").rstrip("\r")
        m = MARKER_RE.match(stripped)
        kind = m.group(1) if m else None
        arg = (m.group(2) or "").strip() if m else ""
        yield _Line(lineno=lineno, raw=raw, marker_kind=kind, marker_arg=arg)


class BundleParser:
    def __init__(self, text: str) -> None:
        self._lines = _scan(text)
        self.fields: dict[str, str] = {}
        self.main_source: str = ""
        self.modules: dict[str, str] = {}

    def parse(self) -> ParsedBundle:
        self._consume_version()
        self._consume_header()
        if "main" not in self.fields:
            raise BundleParseError("missing MAIN directive")
        self.main_source, trailing = self._consume_body()
        self._consume_modules(trailing)
        return ParsedBundle(
            fields=self.fields,
            main_source=self.main_source,
            modules=self.modules,
        )

    def _consume_version(self) -> None:
        try:
            line = next(self._lines)
        except StopIteration:
            raise BundleParseError("empty bundle (missing VERSION)") from None
        if not line.is_marker or line.marker_kind != "VERSION":
            what = f"marker {line.marker_kind}" if line.is_marker else "body content"
            raise BundleParseError(
                f"line {line.lineno}: expected VERSION marker first, got {what}"
            )
        try:
            version = int(line.marker_arg)
        except ValueError as e:
            raise BundleParseError(
                f"line {line.lineno}: VERSION arg must be an integer, got {line.marker_arg!r}"
            ) from e
        if version not in SUPPORTED_VERSIONS:
            raise BundleParseError(
                f"line {line.lineno}: unsupported VERSION {version} "
                f"(supported: {sorted(SUPPORTED_VERSIONS)})"
            )
        self.fields["version"] = str(version)

    def _consume_header(self) -> None:
        for line in self._lines:
            if not line.is_marker:
                raise BundleParseError(
                    f"line {line.lineno}: body content before BODY marker"
                )
            kind = line.marker_kind
            if kind == "PROJECT":
                self._set_unique(line, "project")
            elif kind == "MAIN":
                self._set_unique(line, "main")
            elif kind == "BODY":
                if line.marker_arg:
                    raise BundleParseError(f"line {line.lineno}: BODY takes no argument")
                return
            else:
                raise BundleParseError(
                    f"line {line.lineno}: unknown header directive {kind} "
                    "(at this VERSION, only PROJECT, MAIN, BODY are valid)"
                )
        raise BundleParseError("missing BODY marker")

    def _set_unique(self, line: _Line, field_name: str) -> None:
        if not line.marker_arg:
            raise BundleParseError(
                f"line {line.lineno}: {line.marker_kind} requires an argument"
            )
        if field_name in self.fields:
            raise BundleParseError(
                f"line {line.lineno}: duplicate {line.marker_kind} directive"
            )
        self.fields[field_name] = line.marker_arg

    def _consume_body(self) -> tuple[str, _Line | None]:
        """Consume non-marker lines until the next marker or EOF.

        Returns (body source, the marker that ended the body or None for EOF).
        Body content is preserved verbatim -- no stripping -- so round-trip
        through the bundler is stable.
        """
        body: list[str] = []
        for line in self._lines:
            if line.is_marker:
                return "".join(body), line
            body.append(line.raw)
        return "".join(body), None

    def _consume_modules(self, first_marker: _Line | None) -> None:
        current = first_marker
        while current is not None:
            if current.marker_kind != "MODULE":
                raise BundleParseError(
                    f"line {current.lineno}: unexpected marker {current.marker_kind} after BODY "
                    "(only MODULE markers are valid)"
                )
            key = current.marker_arg
            if not key:
                raise BundleParseError(
                    f"line {current.lineno}: MODULE requires a canonical key"
                )
            if key in self.modules:
                raise BundleParseError(
                    f"line {current.lineno}: duplicate MODULE marker for {key}"
                )
            body, next_marker = self._consume_body()
            self.modules[key] = body
            current = next_marker


def parse_bundle(text: str) -> ParsedBundle:
    return BundleParser(text).parse()


def simulate(text: str) -> dict[str, int]:
    """
    Simulate a Luau VM running the bundle with `require()`-cache semantics.

    Starts at MAIN, recursively walks `require()` calls, and counts how
    many times each module body would execute under correct caching --
    once on first require, zero on cache hits. The return value maps each
    canonical key to its body-execution count; a correctly-bundled,
    correctly-traversed program has every reachable key at exactly 1.

    Counts > 1 indicate a require-cache bug; missing keys indicate
    unreachable (over-bundled) modules. Cycles raise CircularDependencyError.

    No Lua code is actually evaluated -- this is a static walk over the
    require graph, comparing dedup behavior against the contract.
    """
    parsed = parse_bundle(text)
    main_anchor = parsed.fields["main"]  # required by the parser

    all_modules = dict(parsed.modules)
    if main_anchor in all_modules:
        raise BundleParseError(
            f"BUNDLE main={main_anchor} collides with a module key in the same bundle"
        )
    all_modules[main_anchor] = parsed.main_source

    known_aliases: set[str] = {"root"}
    for key in all_modules:
        if not key.startswith("@"):
            raise BundleParseError(f"module key does not start with @: {key}")
        alias = key[1:].split("/", 1)[0]
        known_aliases.add(alias)

    body_runs: dict[str, int] = {}
    cached: set[str] = set()
    in_progress: list[str] = []

    def run(key: str) -> None:
        if key in in_progress:
            chain = " -> ".join([*in_progress, key])
            raise CircularDependencyError(f"circular dependency: {chain}")
        if key in cached:
            return
        if key not in all_modules:
            raise BundleParseError(f"required module {key} not in bundle")
        in_progress.append(key)
        for req in REQUIRE_RE.findall(all_modules[key]):
            target = resolve(req, key, known_aliases)
            run(target)
        in_progress.pop()
        body_runs[key] = body_runs.get(key, 0) + 1
        cached.add(key)

    run(main_anchor)
    return body_runs
