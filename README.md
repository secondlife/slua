# SLua ![CI](https://github.com/secondlife/SLua/actions/workflows/build.yml/badge.svg) [![codecov](https://codecov.io/gh/secondlife/SLua/branch/main/graph/badge.svg)](https://codecov.io/gh/secondlife/SLua)

SLua (short for ServerLua) is a friendly fork of [Luau](https://github.com/Roblox/luau/) implementing efficient
serializable relocatable scripted entities in virtual worlds (specifically Second Life).
It uses a modified [Eris](https://github.com/fnuecke/eris) called "Ares" to serialize agent execution state.

Its intended use is scripting for mixed-author environments with stateful, semi-autonomous objects that can seamlessly
roam across server instances.

This is a friendly fork, and includes a lot of changes to support transparently serializable execution state
that are unlikely to be up-streamable to Luau, but might be a helpful reference. Changes that might make sense
in Luau proper are to be submitted upstream as required.

The changes to Luau proper mainly involve adding hooks to support VM state serialization, and executing LSL.

[See the diff between SLua and the upstream Luau base](https://github.com/secondlife/slua/compare/slua_base...main?expand=1).

* Basic VM state serialization is complete. A yielded thread can be serialized, along with its global environment
  without unnecessary duplication of protos from the "base" system image.
* Many cheap "forks" of a base script may be spawned inside a VM, each with their own isolated state
* Iterators are stable across `deserialize(serialize(vm_state))` trips, regardless of hash bucketing changes
* Luau's JIT can be used mostly as-is, and serializing state while inside a JITed function is fully supported.
* Per-script Memory limits are implemented through Luau's alloc hook + memcat system.
* * This is due for removal, we use a custom heap traversal function to get "logical" script size now.
* Hooks for pre-emptive scheduling are implemented

# Contributing

Sure! If you're interested in adding a feature, please make sure you file an issue before making a PR, but PRs
for fixes are most welcome. Please note that since this fork tries to track upstream Luau relatively closely,
some contributions are better submitted directly upstream so the broader Luau ecosystem can benefit, supposing
they aren't in SLua-specific code.

## Building

If building with LSL support (the default), you must first install [`autobuild`](https://github.com/secondlife/autobuild),
which will pull in the [`tailslide`](https://github.com/secondlife/tailslide) dependency:

```sh
pipx install autobuild
autobuild install
```

On all platforms, you can use CMake to run the following commands to build Luau binaries from source:

```sh
mkdir cmake && cd cmake
cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build . --target Luau.Repl.CLI --config RelWithDebInfo
cmake --build . --target Luau.Analyze.CLI --config RelWithDebInfo
```

Alternatively, on Linux and macOS, you can also use `make`:

```sh
make config=release luau luau-analyze
```

To integrate SLua into your CMake application projects as a library, at the minimum, you'll need to depend on `Luau.Compiler` and `Luau.VM` projects. From there you need to create a new Luau state (using Lua 5.x API such as `lua_newstate`), compile source to bytecode and load it into the VM like this:

```cpp
// needs lua.h and luacode.h
size_t bytecodeSize = 0;
char* bytecode = luau_compile(source, strlen(source), NULL, &bytecodeSize);
int result = luau_load(L, chunkname, bytecode, bytecodeSize, 0);
free(bytecode);

if (result == 0)
    return 1; /* return chunk main function */
```

For more details about the use of the host API, you currently need to consult [Lua 5.x API](https://www.lua.org/manual/5.1/manual.html#3). Luau closely tracks that API but has a few deviations, such as the need to compile source separately (which is important to be able to deploy VM without a compiler), and the lack of `__gc` support (use `lua_newuserdatadtor` instead).

To gain advantage of many performance improvements, it's highly recommended to use the `safeenv` feature, which sandboxes individual scripts' global tables from each other, and protects builtin libraries from monkey-patching. For this to work, you must call `luaL_sandbox` on the global state and `luaL_sandboxthread` for each new script's execution thread.

# Testing

SLua has an internal test suite; in CMake builds, it is split into two targets, `Luau.UnitTest` (for the bytecode compiler and type checker/linter tests) and `Luau.Conformance` (for the VM tests). The unit tests are written in C++, whereas the conformance tests are largely written in Luau (see `tests/conformance`).

Makefile builds combine both into a single target that can be run via `make test`.

# Dependencies

SLua uses C++ as its implementation language. The runtime requires C++11, while the compiler and analysis components require C++17. It should build without issues using Microsoft Visual Studio 2017 or later, or gcc-7 or clang-7 or later.

Other than the STL/CRT, SLua library components don't have external dependencies. The test suite depends on the [doctest](https://github.com/onqtam/doctest) testing framework, and the REPL command-line depends on [isocline](https://github.com/daanx/isocline).

Note that LSL support does require that Tailslide be installed, but this is optional.

# License

SLua implementation is distributed under the terms of [MIT License](https://github.com/secondlife/SLua/blob/master/LICENSE.txt). It is based on Luau, also under the MIT License.
