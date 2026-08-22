# RFC: Static require() for ServerLua

* Status: **DRAFT**
* Reference implementation: [`tools/slua_bundle/`](../tools/slua_bundle/)

## Summary

Compile-time static `require()` resolution that bundles multiple modules into a single deployable unit. Client-side bundling
avoids the kind of security issues (see MySQL's `LOAD LOCAL`) where the server has to tell the client what local resources are needed
to continue bundling, and the client can't reason about what resources are needed on its own. Preserves original filenames and line numbers
for debugging via per-Proto source info in bytecode.

SLua intentionally reuses upstream Luau ecosystem conventions where they exist - `.luaurc` for aliases, `init.luau` for module-as-directory entries,
Wally-style project layout - so existing Roblox/Luau tooling works without modification. SLua adds the bundle format and per-Proto source bytecode
on top; configuration is not a SLua extension surface.

## Motivation

**Problem:** No code reuse beyond copy-paste in-viewer. Large scripts are unwieldy. LSL's `#include` loses dependency info after processing,
 if you don't have the local dependencies you have no option but to edit the post-processed source code soup.

**Why static `require()`:** SL's creation and permission models don't play nice with dynamic runtime dependencies. Bundling together dependencies when
 building makes it easy for us to support multiple development styles (external editor with custom build toolchain, in-viewer editor have different opinions about
 how to include files and from where)

**Why it's tricky:** Server compiles, but dependencies live on client. Can't reasonably have server request files (i.e. `LOAD LOCAL` from MySQL).
Need debug info across modules. Want future tree shaking/inlining for optimization purposes.

## Path Resolution

**Aliases are pure identity, not location.** A `require("@alias/path")` call is a logical name for a module. The path
never encodes where the module is fetched from or what kind of resolver produced it. Two scripts that share an alias
path are referring to the same logical module regardless of how each environment resolved it.

| Pattern | Meaning                                                                                       |
|---------|-----------------------------------------------------------------------------------------------|
| `require("@root/utils")` | The built-in `@root/` alias always points to the project root. See [Built-in aliases](#built-in-aliases). |
| `require("@myalias/utils")` | User-declared alias from `.luaurc`. Resolved by the bundler; the bundle key is the alias path (case-folded, and possibly redirected by an `ALIAS` mapping; see [Canonicalization](#canonicalization)). |
| `require("./foo")`, `require("../foo")`, `require("@self/foo")` | Location-dependent. Absolutized against the requiring module's key at compile time; see [Canonicalization](#canonicalization). |

Bare identifiers (`require("foo")`) are not valid - upstream Luau already rejects anything without a `./`, `../`, or
`@` prefix. The `@sl/` namespace is reserved for future use, and the reservation is enforced: requires through it,
`.luaurc` declarations of it, and bundle keys under it are all rejected (any casing).

**Alias names are case-insensitive**, matching upstream Luau (`Config.cpp` folds to lowercase at `.luaurc` load;
`RequireNavigator.cpp` folds at require time). Canonical keys always carry the lowercase spelling; `.luaurc` may not
declare two names that collide after folding. The fold is ASCII A-Z only; non-ASCII alias names are unspecified,
also matching upstream.

**Bundlers resolve aliases at bundle time; the server never sees `.luaurc`.** The bundle stores each module under
its canonical absolute alias key. At compile time the server makes every require string absolute - location-dependent
forms are absolutized against the enclosing module's key, then the result is rewritten through the bundle's
[`ALIAS` table](#alias-non-canonical-prefixes) (longest component-prefix match). Runtime `require()` is then a simple
string lookup against the bundle's module table - no alias resolution at runtime, and every module has exactly one key.

### Built-in aliases

- `@root/` — always points to the project root. Lets scripts reference project-internal modules without depending on
  the project's PROJECT name. Reserved: `.luaurc` may not declare an alias named `root` (any casing).
- `@self/` — upstream Luau convention: "current module's directory" (relative to the requiring file). Treated by the
  resolver, not by `.luaurc`. Reserved — `.luaurc` may not declare an alias named `self` (any casing). `@self` is
  expanded away during compile-time absolutization, before the `ALIAS` table is consulted, so no mapping can ever
  touch it; `ALIAS` directives naming it are rejected as hygiene.
- `@sl/` — reserved for future use; requires, `.luaurc` declarations, and bundle keys using it are rejected.

Project-internal canonical keys always have the form `@root/...`, regardless of the bundle's PROJECT name. PROJECT is
viewer-linkage metadata only and never appears in canonical keys.

**MAIN is always under `@root`.** The entry script must live under `project_root` and is keyed by its root-relative
path, ignoring aliases; parsers reject a `MAIN` directive whose key is not `@root` or `@root/...`. MAIN's key is
never a lookup target - in a pure-trace bundle everything is reachable from MAIN, so requiring it is by definition a
cycle - it exists only to anchor the BODY's location-dependent requires.

### Canonicalization

Every module is keyed in the bundle under exactly one absolute alias path: the **canonical key**, derived from the
module's physical location (most-specific covering alias). Source text is preserved byte-for-byte - nothing rewrites
require strings in module bodies. Instead, two kinds of keys are reconciled at compile time:

- The **syntactic key** of a require: what the server computes from the require string and the enclosing MODULE/MAIN
  key alone, with no `.luaurc` - alias names case-folded, `./`/`../`/`@self` absolutized against the anchor.
- The **canonical key** the module is stored under.

These coincide almost always. When they diverge (the require spells the module through a different alias relationship
than the canonicalizer picked), the bundler factors the divergence into a prefix mapping and emits it as a header
[`ALIAS` directive](#alias-non-canonical-prefixes); the server applies the table after absolutization. Keying through
the physical file means two aliases to one file are still **one** module with one cache instance, matching upstream
Luau's cache-by-resolved-path semantics.

```mermaid
flowchart TD
    Start[require S from module R]
    Start --> Kind{S starts with}
    Kind -->|"@alias/..."| Fold[Case-fold alias -> syntactic key K]
    Kind -->|"./, ../, @self/"| Anchor[Absolutize against R's key -> syntactic key K]
    Fold --> Resolve[Resolve K to physical file L]
    Anchor --> Resolve
    Resolve --> FindAlias{Most-specific covering alias for L}
    FindAlias -->|Found| Build["Canonical key C = @alias/{L relative to target}"]
    FindAlias -->|None| NoAliasErr["Error: no alias spans L (shouldn't happen: @root always covers project_root)"]
    Build --> Same{K == C?}
    Same -->|Yes| Done[Stored in bundle module table under C]
    Same -->|No| Remap[Emit header ALIAS prefix mapping K-minus-tail -> C-alias]
    Remap --> Done
```

The covering-alias set always includes `@root` for `project_root`, so files under `project_root` always canonicalize
cleanly. Files outside `project_root` need an explicit `.luaurc` alias spanning them.

MAIN is the exception to most-specific-alias keying: its key is its root-relative path by fiat (see
[Built-in aliases](#built-in-aliases)). A deeper alias covering MAIN's directory simply makes the body's relative
requires diverge syntactic-vs-canonical, which the uniform `ALIAS` rule already handles.

**Tiebreaker for identical-target aliases.** Specificity is by number of path components in the alias target (deeper
wins). When two aliases point at the exactly-same directory at the deepest covering tier, the tiebreak picks the
**primary** (canonical) key only - requires spelled through the losing alias keep working via the emitted `ALIAS`
mapping:
- If `@root` is one of them, `@root` wins silently. Common case: a user declared a Wally-style alias for
  `project_root` in `.luaurc` not realizing `@root` already covers it.
- Otherwise, the alphabetically-first folded alias name wins (ASCII byte order), and the bundler emits a warning -
  a config smell, usually a copy-paste mistake in `.luaurc`, but not a broken bundle.

**Worked example.** Inside `Packages/SomeLib/init.luau` with `.luaurc` defining `"SomeLib": "Packages/SomeLib"`:

```lua
local Module = require("./src/Module")
```

resolves physically to `Packages/SomeLib/src/Module.luau`. The most-specific covering alias is `@somelib` (folded),
so the bundle key becomes `@somelib/src/Module` - identical to what `require("@SomeLib/src/Module")` produces after
case folding. Wally packages canonicalize this way without any SLua-specific handling.

Same source tree + same `.luaurc` produces identical canonical keys regardless of which bundler ran; `.luaurc` ships
alongside the source. Resolvers returning virtual source (e.g. inventory) must declare an alias prefix for what they
return, or reject location-dependent requires inside it - a resolver concern, not the format's.

**Known deliberate divergence:** a `../` that climbs above the canonical alias root is an error, even when the
physical file exists under a shallower alias (upstream, resolving on disk, would find it). Key space has no headroom
above the alias root; the filesystem does. Restructure the require to spell the target through the shallower alias.

### ALIAS: non-canonical prefixes

Every syntactic-vs-canonical divergence has one shape: syntactic `@A/p1/../pk/tail` vs canonical `@B/tail`, where
alias B's directory equals A's directory plus `p1/../pk`. The shared tail means each alias *relationship* is a single
prefix mapping - "in this bundle, this prefix means that alias":

```lua
-- !!LUABUNDLE:ALIAS @zeta @alpha        (two aliases, same directory; @alpha won the tiebreak)
-- !!LUABUNDLE:ALIAS @myproj @root       (user alias targeting project_root; @root wins)
-- !!LUABUNDLE:ALIAS @alpha/sub @util    (nested alias, or a relative require crossing into @util's territory)
```

At compile time, after absolutization, the server rewrites any require key whose component-prefix matches a mapping
source (longest match wins) to the target plus the remainder. One application suffices: targets are always
tiebreak-winner aliases, already canonical. The runtime module table stays strictly one key per module.

Validation (parse-time):
- `ALIAS` is a header directive (between `VERSION` and `BODY`); two `@`-prefixed keys in canonical (folded) form.
- The target must be a bare `@alias`. The source may carry a path tail (`@alpha/sub`), which is how nested aliases
  and relative-require crossovers are expressed.
- Neither key may involve `@self`; the source may not be bare `@root` (nothing can ever legitimately remap the root
  namespace away - the target of every derived mapping is a tiebreak winner, and `@root` always wins at its own
  directory). `@root/sub` as a source is legal and necessary.
- Duplicate sources, a source mapping to itself, and self-nesting (`@b/x -> @b`, which claims an alias's directory
  contains itself) are errors.
- No `MODULE` key may sit under a mapping's source prefix: module keys are canonical, so a remapped prefix only
  redirects spellings that would otherwise dangle. (MAIN is exempt - root-relative by fiat, never a lookup target.)

## Bundle Format

Text-based, valid Luau syntax, lightly inspired by MIME multipart RFC. The format is **producer-agnostic**: the viewer
and any external bundler emit the same artifact, and the server cannot tell which built it.

```lua
-- !!LUABUNDLE:VERSION 1
-- !!LUABUNDLE:PROJECT myhud
-- !!LUABUNDLE:MAIN @root/HudController
-- !!LUABUNDLE:BODY
local foo = require("@root/lib/foo")
foo.bar()
-- !!LUABUNDLE:MODULE @root/lib/foo
local helpers = require("./helpers")
return { bar = function() return helpers.helper() end }
-- !!LUABUNDLE:MODULE @root/lib/helpers
return { helper = function() return "hello" end }
```

**Directives** (each on its own line, line-comment form):

| Directive | Required | Purpose |
|-----------|----------|---------|
| `-- !!LUABUNDLE:VERSION <n>` | yes, first line | Bundle format version. Consumers MUST reject unknown versions. Future version bumps are non-back-compatible unless explicitly stated. |
| `-- !!LUABUNDLE:PROJECT <name>` | no | Advisory viewer-linkage metadata; associates the bundle with a disk project. Not used for canonicalization. See [Project Linkage](#project-linkage). |
| `-- !!LUABUNDLE:MAIN <canonical-key>` | yes | Key for the MAIN section; always `@root`-relative (computed from MAIN's path under `project_root`, ignoring aliases). Anchors `./`, `../`, and `@self/` requires inside MAIN. |
| `-- !!LUABUNDLE:ALIAS <from> <to>` | per mapping | Prefix remapping applied to require keys at compile time, after absolutization; longest component-prefix match wins. See [ALIAS](#alias-non-canonical-prefixes). |
| `-- !!LUABUNDLE:BODY` | yes | Separates the header from MAIN's source body. Everything between BODY and the first MODULE marker (or EOF) is MAIN. |
| `-- !!LUABUNDLE:MODULE <canonical-key>` | per module | Marks each dependency. The canonical key identifies the module for runtime `require()` lookup. |

**Other rules:**
- Lines matching `^--\s*!!LUABUNDLE:` in user source are rejected at bundle time. Users may not author lines that look like bundle directives.
- Generally the user only sees and directly edits the `MAIN` body of the bundle.
- Users may define their own aliases (via `.luaurc`) referring to libs on disk; see [Configuration & Project Layout](#configuration--project-layout).

The bundle conceptually provides a virtual filesystem for the runtime `require()` implementation. Because the format
includes source code as-is - before any optimization or tree-shaking - filenames and line mappings for errors are
automatically correct without `--!@line` directives.

## Configuration & Project Layout

SLua reuses Luau ecosystem conventions for project configuration. **No SLua-specific config file is introduced.**

### Alias config: `.luaurc`

`.luaurc` is the standard Luau alias config (JSON, Wally-compatible). SLua bundlers read it the same way `lune`, the
`luau` CLI, and Roblox Studio do:

```json
{
  "languageMode": "strict",
  "aliases": {
    "Pkg": "Packages"
  }
}
```

Project-internal modules don't need a user alias — the built-in `@root/` covers `project_root`. `.luaurc` is for
declaring external aliases (Wally packages, vendored libs, virtual-source resolvers, etc.). The alias names `root`,
`self`, and `sl` are reserved (any casing); alias names are case-insensitive (see [Path Resolution](#path-resolution))
and restricted to upstream's charset (`isValidAlias` in Config.cpp): ASCII letters, digits, `-`, `_`, `.`; never `.`,
`..`, or anything containing a path separator. Declare names without a leading `@` - upstream tolerates one but then
stores the name verbatim so it can never match a require; SLua rejects it instead of inheriting the trap.
Like upstream's lexer-based parser, `//` and Lua-style `--` line comments, `--[[ ... ]]` long comments (including
`=`-leveled openers), and trailing commas are all accepted; an unterminated long comment is an error, and C block
comments (`/* */`) are rejected, exactly as upstream's lexer behaves.

**SLua bundlers read only the `.luaurc` at `project_root`.** Nested `.luaurc` files deeper in the tree are ignored;
files above `project_root` are never consulted. This deviates from upstream Luau's walking behavior on purpose:
bundle reproducibility requires the alias set to be a function of the project tree alone. Walking up past
`project_root` would let aliases bleed in from whatever directory the developer happened to bundle from, breaking
identical-source-tree-gives-identical-canonical-keys.

### Recommended directory layout

Mirror Rojo/Wally idiom:

```
myhud/                   # project_root (any name; not load-bearing)
  .luaurc                # aliases for external deps (Wally-friendly, Roblox-compatible)
  wally.toml             # optional, Wally manifest
  Packages/              # Wally-vendored deps (e.g. @SomeLib)
  src/
    HudController.luau   # top of src/ -> SL script "HudController" (canonical: @root/src/HudController)
    DataLink.luau        # top of src/ -> SL script "DataLink"      (canonical: @root/src/DataLink)
    lib/
      utils.luau         # nested -> helper, @root/src/lib/utils
      shared.luau        # nested -> helper, @root/src/lib/shared
```

**Convention:** files at the **immediate root of `src/`** become SL scripts (one each, named after the disk filename).
Files in **any subdirectory** are project-local helpers, never deployed as standalone scripts. A CLI deploy tool
(`slua deploy <object-id>` or similar) reads the top of `src/`, bundles each top-level file with its required modules,
and uploads one SL script per file. No manifest is needed; the directory shape is the manifest.

This dissolves the question of what "the main file" is named at the script level: the SL script name *is* the disk
filename. There is no `main.luau` magic.

### Module-as-directory entry: `init.luau`

When an alias resolves to a directory (e.g., `require("@coollib")` where `coollib` is a directory), the entry file is
`init.luau`. **This is upstream Luau's convention** (see `Require/include/Luau/Require.h:33-34`), adopted unchanged so
SLua require behaves identically to Lute, Roblox, and the `luau` CLI for directory aliases. `init.luau` has no special
meaning at the SL-script level - only for module-as-directory.

### Wally compatibility

SLua projects are normal Luau projects. `wally install` populates `Packages/`, `.luaurc` aliases (`@Pkg/...`) point at
it, the bundler reads disk normally. No SLua-side configuration is needed beyond what any other Luau tool already
supports.

## Resolver Behaviour

A bundler maps an alias path to source code at bundle time. The RFC commits only to **minimum** behaviour and stays
silent on the rest:

- Bundlers **SHOULD** support `.luaurc`-based disk alias resolution. This is the Wally/Roblox compatibility floor.
- Disk resolvers **MUST** treat path casing as significant even on case-insensitive host filesystems (macOS, Windows):
  scripts always run under Linux, so a require that resolves only via case insensitivity would produce a bundle whose
  keys depend on which host built it. The reference `DiskFS` verifies exact on-disk casing and treats mismatches as
  not-found, matching the Linux runtime.
- Bundlers **MAY** support additional resolvers (inventory, marketplace, http registries, etc.). These are entirely an
  implementation choice; the RFC neither enumerates nor specifies them.
- The bundle's embedded source is the **universal last-resort resolver**. Any bundler can rebundle a bundle it received -
  even if it cannot reach the original sources - by reusing the bundled copies of each module.

The last-resort resolver is what keeps a viewer-authored bundle re-buildable from an external CLI without inventory
access: when an alias has no local resolver, the bundler falls back to the source already embedded in the previous
bundle. The script never breaks just because the new environment lacks the original resolver.

## Project Linkage

The bundle header's optional `PROJECT <name>` directive lets a viewer associate a script with a local disk project
for editing. The RFC specifies the **contract** only:

- A viewer MAY maintain bindings from project names (and optionally per-object UUID overrides) to local disk directories.
- Resolvers see the path resulting from the binding; how it is stored is viewer business.
- Bindings are per-user-per-machine, never synced to SL servers, and not assumed identical between collaborators.
- Header is metadata only; an unset project name just means "no disk linkage known here." A bundler that ignores it
  works correctly.

### Non-normative appendix: registry sketch

A minimal viewer-internal registry could look like:

```json
{
  "projects": {
    "myhud":         "/Users/me/slua/myhud",
    "vendor-system": "/Users/me/slua/vending"
  },
  "objects": {
    "01234567-89ab-cdef-0123-456789abcdef": "/Users/me/slua/oldhud"
  }
}
```

Resolution order: object UUID override -> project header name -> unbound.

This appendix is illustrative. TPVs may store bindings however suits their codebase; only the contract above is
load-bearing.

Viewer-to-disk **sync-back** (writing MAIN edits back to a bound disk project) is deferred to a follow-up RFC.

## Editing Workflow

```mermaid
flowchart TD
    subgraph Start
        direction LR
        OpenExisting[Open existing script]
        CreateNew[Create new script]
    end

    DownloadArchive[Download source archive]
    IsValidBundle{Valid bundle?}
    MainView[MAIN-only view]
    RawView[Raw view]
    UserEdits[User edits]

    CreateNew --> MainView
    OpenExisting --> DownloadArchive
    DownloadArchive --> IsValidBundle
    IsValidBundle -->|Yes| MainView
    IsValidBundle -->|No| RawView
    MainView --> UserEdits
    RawView --> UserEdits

    UserEdits -->|Toggle view| CheckUnsaved
    UserEdits -->|Save| StartSave

    subgraph Toggle[Toggle View]
        CheckUnsaved{Unsaved changes?}
        TogglePrompt{Discard or cancel?}
        CheckToggleDir{Raw to MAIN?}
        ValidBundle{Valid bundle?}
        DoToggle[Toggle view]
        ShowParseError[Show parse error]

        CheckUnsaved -->|No| CheckToggleDir
        CheckUnsaved -->|Yes| TogglePrompt
        TogglePrompt -->|Discard| CheckToggleDir
        CheckToggleDir -->|No| DoToggle
        CheckToggleDir -->|Yes| ValidBundle
        ValidBundle -->|Yes| DoToggle
        ValidBundle -->|No| ShowParseError
    end

    subgraph Save
        StartSave[User saves]
        IsRawView{Viewing raw?}
        SendRaw[Send as-is]
        ResolverSucceeds{Resolver produces source?}
        CheckOwnership{Same user last saved?}
        BundleResolved[Bundle from resolver]
        CheckMatch{Resolver matches bundle?}
        ConfirmUntrusted{Confirm: pull resolved source\ninto untrusted script?}
        DepInBundle{Dep in existing bundle?}
        BundleFromBundle[Bundle from existing]
        ResolveError[Error: can't resolve]
        SendBundle[Send bundle]
        AbortSave[Abort]

        StartSave --> IsRawView
        IsRawView -->|Yes| SendRaw
        IsRawView -->|No| ResolverSucceeds
        ResolverSucceeds -->|Yes| CheckOwnership
        CheckOwnership -->|Yes| BundleResolved
        CheckOwnership -->|No| CheckMatch
        CheckMatch -->|Yes| BundleResolved
        CheckMatch -->|No| ConfirmUntrusted
        ConfirmUntrusted -->|Yes| BundleResolved
        ConfirmUntrusted -->|No| AbortSave
        ResolverSucceeds -->|No| DepInBundle
        DepInBundle -->|Yes| BundleFromBundle
        DepInBundle -->|No| ResolveError
        ResolveError --> AbortSave
        BundleResolved --> SendBundle
        BundleFromBundle --> SendBundle
    end

    TogglePrompt -->|Cancel| UserEdits
    ShowParseError --> UserEdits
    DoToggle --> UserEdits
    AbortSave --> UserEdits

    SendRaw --> Compile
    SendBundle --> Compile

    subgraph Server
        Compile[Compile & store]
        Done[Return result]
        Compile --> Done
    end
```

**Key points:**
- MAIN-only view: user sees entry script, client bundles on save by running its resolver chain.
- Raw view: user sees/edits full archive, sent as-is. This path also serves externally-built bundles uploaded directly.
- Resolver chain: each alias is tried against the bundler's resolvers (`.luaurc` disk minimum; viewer may add inventory or others). Chain order and resolver types are bundler-specific (see [Resolver Behaviour](#resolver-behaviour)).
- Ownership check: security measure to prevent leaking resolved source into a script the current user does not own.
- - Open question as to how this should work; needs server-side enrichment.
- Universal fallback: if no resolver succeeds, use the version embedded in the previous bundle. This lets users do
  one-off edits even when they lack the original resolver context (e.g., editing in an external CLI a script that was
  originally bundled by a viewer with inventory access).

## Runtime

- `require()` implemented in C (hides module table/cache from scripts)
- Module results cached after first execution
- Each module runs in sandboxed environment via `dangerouslyexecuterequiredmodule()`
- - This function will be hidden from view and not directly usable.
- Simple string lookup - no alias resolution at runtime. Require strings are made absolute at **compile time**
  (absolutize against the enclosing MODULE/MAIN key, then rewrite through the `ALIAS` table), so the runtime table is
  strictly one key per module and lookups are exact string matches.

## Bytecode Extension

**Problem:** Standard Luau shares one `chunkname` across all functions. Bundles have multiple
 source files. It's useful to be able to have proper filename and line mappings in errors.

**Solution:** Per-Proto source in bytecode. Each function stores its source filename. Loader reads per-function source for correct stack traces.

```cpp
// In lvmload.cpp
TString* protoSource = hasPerProtoSource
    ? strings[readVarInt(data, size, offset)]
    : source;  // Fallback for non-bundles
p->source = protoSource;
```

## Errors

| Error | When | Message |
|-------|------|---------|
| Dynamic require | Compile | `require() argument must be a string literal` |
| Bare identifier | Compile | `require path must start with './', '../', or '@'` |
| Unknown alias | Compile | `unknown alias '@foo' in require` |
| Require escapes alias | Compile | `require '<S>' from <module>: '..' traverses past alias root` |
| Invalid path component | Compile | `component '<c>' starts with '.'` / `component contains NUL` |
| Reserved namespace require | Compile | A require uses the `@sl/` namespace, which is reserved for future use |
| No covering alias | Bundle time | `no alias spans <physical-path>; add a .luaurc entry covering it` (won't fire for files under `project_root` since `@root` always covers it) |
| Reserved alias | Bundle time | `.luaurc` declares an alias name reserved by the format (`root`, `self`, `sl`; any casing) |
| Invalid alias name | Bundle time | `.luaurc` alias name outside upstream's charset (ASCII alphanumerics, `-`, `_`, `.`; no separators, no `.`/`..`, no leading `@`) |
| Invalid MAIN file | Bundle time | `main_path` does not exist, is not a regular file, or is not a `.luau` file |
| Invalid UTF-8 | Bundle time | A source or bundle file on disk is not valid UTF-8 (the format is UTF-8 everywhere) |
| Alias case-fold collision | Bundle time | Two `.luaurc` alias names collide after case folding (alias names are case-insensitive) |
| Alias collision | Bundle time | Two `.luaurc` aliases point at the exact same directory; emitted as a warning, not an error - the `ALIAS` mapping keeps both spellings working |
| MAIN outside project root | Bundle time | The entry script is not under `project_root`; choose a root containing it |
| Remap conflict | Bundle time | One syntactic prefix would map to two targets, or a remapped prefix collides with a module key; only possible when a fallback bundle's alias universe disagrees with the on-disk `.luaurc` |
| Ambiguous file resolution | Bundle time | Both `<leaf>.luau` and `<leaf>/init.luau` exist; remove one to disambiguate |
| No resolver succeeded | Bundle time | `cannot resolve module '<path>': no resolver produced source and no copy in existing bundle` |
| Circular dependency | Compile | `circular dependency: <path1> -> <path2> -> <path1>` |
| Delimiter in source | Compile | `source cannot contain '-- !!LUABUNDLE:'` |
| Depth exceeded | Compile | `require depth exceeds maximum` |
| Duplicate MODULE marker | Parse | Bundle contains two `MODULE` directives with the same canonical key |
| Malformed key | Parse | A `MODULE`/`MAIN`/`ALIAS` key does not begin with `@`, has an empty or non-lowercase alias, or uses `@self` |
| MAIN not under @root | Parse | The `MAIN` directive's key is not `@root` or `@root/...` |
| Invalid ALIAS | Parse | Wrong arity, bare-`@root` source, tailed target, self-mapping, self-nesting (`@b/x -> @b`), duplicate source, placement after `BODY`, or a mapping source shadowing a `MODULE` key |
| Missing MAIN directive | Parse | Bundle has no `MAIN` directive |
| Module not in bundle | Runtime | `module not found: <path>` |

The reference Python bundler raises the bundle-time and parse errors directly. The production C++ Luau compiler will
surface the compile-time errors at compile time; the reference bundler raises analogous errors at bundle time as a
stand-in.

## Limits

- Max dependency depth: 100 (the reference implementation measures BFS level from MAIN - shortest require chain, not
  longest)
- Max total modules: 1000
- No circular dependencies (no different from typical Luau here!)

## Reference implementation caveats

The Python bundler scans for requires with a regex rather than a parser. Consequences, accepted for the prototype:

- `require` matches inside comments and long strings are traced, which can over-bundle or surface a spurious
  "no resolver" error from commented-out requires.
- Dynamic requires (`require(expr)`) are invisible to the regex, so the "dynamic require" compile error cannot fire in
  the reference implementation; the production AST-based compiler rejects them.
- Both quote styles and Luau's paren-less call sugar (`require "./x"`) are matched.

The production compiler resolves requires from the AST and has none of these limitations.

SLua resolves `.luau` files only, deliberately. Upstream delegates extension policy to the embedder (the `luau` CLI
also tries `.lua`); SLua picks a single extension so canonical keys are unambiguous.

## Future Work

- Tree shaking (eliminate unused exports)
- Cross-module inlining (`--!pure` modules)
- Viewer-to-disk sync-back semantics (separate RFC, after viewer implementation experience)
- Marketplace / registry resolver designs (whatever shape they take, they sit alongside disk and inventory as additional bundler-side resolvers; the bundle format does not need to know)
