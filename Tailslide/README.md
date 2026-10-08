# Intro

Tailslide provides an embeddable parser, AST representation, and tree walker library for
Second Life's Linden Scripting Language. These can be used as the foundation for an
LSL compiler or developing a superset of LSL.

A reference [byte-perfect LSO compiler](tailslide/passes/lso)
and semantically equivalent [CIL compiler](tailslide/passes/mono) are provided. Semantic
conformity with the output of LL's compilers is proven through
[extensive testcases](tests/scripts) and a
[fuzzer](https://github.com/SaladDais/lscript-tailslide#lscript_fuzzer).

Also provided is a CLI utility to quickly lint or optimize LSL scripts,
as well as visualize the AST of the supplied script.

If you're mainly interested in a command-line linting or optimization utility,
please see [lslint](https://github.com/Makopo/lslint) or
[LSL-PyOptimizer](https://github.com/Sei-Lisa/LSL-PyOptimizer), respectively.

# Credits
Tailslide is heavily based on the [lslint](https://github.com/pclewis/lslint)
LSL linter written by pclewis, see the [NOTICE.txt](NOTICE.txt) file for the
original README and credits.

# Build

Tailslide is built as part of the SLua tree, see the top-level README. The library
is the `libtailslide` CMake target and the Makefile's `build/<config>/libtailslide.a`;
its tests run inside `Luau.UnitTest` / `slua-tests`. Pass `-DTAILSLIDE_BUILD_CLI=ON`
for the `tailslide` CLI, and see `fuzz.sh` for the libFuzzer harness.

`flex` and `bison` are optional, the generated scanner and parser are checked in. When
both are found they regenerate in-tree; `-DTAILSLIDE_SKIP_GEN=ON` turns that off. On
macOS you _must_ use the Homebrew versions (`bootstrap_macos.sh`), the ones that ship
with Xcode are extremely old.

# Tech Overview

## Parsing

The parser behaves more or less like the one on SL's servers, quirks and all.

For example:
```
default { $$$$
    '' state_entry() {
        llOwnerSay(L"Hello
world!");
    }"
}/*{
```

is completely handled:

```
$ tailslide --show-tree tests/scripts/parser_abuse.lsl
default
{
    state_entry()
    {
        llOwnerSay("\"Hello\nworld!");
    }
}

TOTAL:: Errors: 0  Warnings: 0
script [none] (cv=) (1,1)
  ast node list [none] (cv=) (1,1)
  ast node list [none] (cv=) (1,1)
    state [none] (cv=) (1,1)
      identifier "default" [none] (cv=) (1,1)
      event handler [none] (cv=) (2,8)
        identifier "state_entry" [none] (cv=) (2,8)
        null [none] (cv=) (2,8)
        compound statement [none] (cv=) (2,22)
          statement [none] (cv=) (3,9)
            function call [none] (cv=) (3,9)
              identifier "llOwnerSay" [none] (cv=) (3,9)
              constant expression [string] (cv=string constant: "\"Hello\nworld!") (3,20)
                string constant: "\"Hello\nworld!" [string] (cv=string constant: "\"Hello\nworld!") (3,20)
```

Special attention has been paid to weird, undocumented corner cases.

## Optimizations

### Constant Folding

The constant folding implementation is currently very simple. Tailslide makes multiple
walks down the tree checking if an expression is constant, and replaces it with its value.

```
integer foo = 2;
foo = foo + 2 + 4 + 3 + 5;
```

won't currently be simplified at all due to how expressions are represented and will
require term rewriting to move non-constant parts of expressions as far to the right (left) as possible.

There are still some cases where constants may be folded but currently aren't, and some differences
in how floating point operations are folded.

### Unused Variable / Function Pruning

Any variable or function that is unused after constant folding may be pruned to save bytecode space.

### Symbol Mangling

Names of user defined functions, their parameters and globals contribute to bytecode size.
To reduce bytecode size, Tailslide can mangle user-defined symbols into more compact ones like
`_a`, `_b`, etc. This may also help with disambiguation when converting the AST to another
language with different scoping / variable shadowing rules.

This isn't the most efficient naming scheme if targeting SL in particular,
since it's better to take advantage of strings already in the constant pool.

# License
MIT, scripts used for test data (barring those added by me and those in `bugs/`) are
property of their respective owners

Serialization code based on Kai Mast's [BitStream library](https://github.com/kaimast/BitStream/blob/master/include/BitStream.h)
is included in BitStream.hh, and is licensed under the BSD 3-clause license.
