#!/usr/bin/env bash

cd "$(dirname "$0")"

# turn on verbose debugging output for parabuild logs.
exec 4>&1; export BASH_XTRACEFD=4; set -x
# make errors fatal
set -e
# complain about unset env variables
set -u

if [ -z "$AUTOBUILD" ] ; then
    exit 1
fi

if [ "$OSTYPE" = "cygwin" ] ; then
    autobuild="$(cygpath -u $AUTOBUILD)"
else
    autobuild="$AUTOBUILD"
fi

top="$(pwd)"
stage="$(pwd)/stage"

# load autobuild provided shell functions and variables
source_environment_tempfile="$stage/source_environment.sh"
"$autobuild" source_environment > "$source_environment_tempfile"
. "$source_environment_tempfile"

# remove_cxxstd
source "$(dirname "$AUTOBUILD_VARIABLES_FILE")/functions"

build=${AUTOBUILD_BUILD_ID:=0}

mkdir -p "$stage/include/luau"
mkdir -p "$stage/lib/release"

pushd "$top"
    pushd "VM/include"
    cp -v lua.h luaconf.h lualib.h llsl.h "$stage/include/luau/"
    popd
    pushd "Compiler/include"
    cp -v luacode.h "$stage/include/luau/"
    popd
    cp -rv "Common/include/Luau" "$stage/include/luau/"

    # Don't litter the source directory with build artifacts
    mkdir -p "$stage/build"
    cd "$stage/build"

    # 32-bit linux needs some different vars
    case "$AUTOBUILD_PLATFORM" in
        linux)
            # These flags are all that are needed to successfully pass the conformance tests under -m32.
            # 64-bit times are needed for the time tests, and SSE is needed so that x87 is not
            # used for floating point math. SSE is the default under x64, so not a problem.
            LL_BUILD_RELEASE="${LL_BUILD_RELEASE} -msse3 -mfpmath=sse -D_TIME_BITS=64 -D_FILE_OFFSET_BITS=64"
        ;;
    esac

    case "$AUTOBUILD_PLATFORM" in
        windows*)
            set -o igncr
            opts="$LL_BUILD_RELEASE /EHsc"
            cmake -G "$AUTOBUILD_WIN_CMAKE_GEN" -A "$AUTOBUILD_WIN_VSPLATFORM" \
                  -DCMAKE_INSTALL_PREFIX="$(cygpath -m "$stage")" \
                  -DCMAKE_C_FLAGS="$(remove_cxxstd $opts)" \
                  -DCMAKE_CXX_FLAGS="$opts" \
                  -DLUAU_USE_TAILSLIDE=ON \
                  "$top"
            cmake --build . -- /p:Configuration=Release
            cmake --build . --target Luau.Repl.CLI -- /p:Configuration=Release

            mkdir -p "$stage/bin"

            cp -v "Release/Luau.Ast.lib" "$stage/lib/release/"
            cp -v "Release/Luau.CodeGen.lib" "$stage/lib/release/"
            cp -v "Release/Luau.Compiler.lib" "$stage/lib/release/"
            cp -v "Release/Luau.Config.lib" "$stage/lib/release/"
            cp -v "Release/Luau.VM.lib" "$stage/lib/release/"
            cp -v "Release/Luau.Common.lib" "$stage/lib/release/"

            cp -v Release/slua.exe "$stage/bin/"
        ;;
        darwin*|linux*)
            # Continue compiling with asserts for now
            LL_BUILD_RELEASE="$(remove_switch -DNDEBUG $LL_BUILD_RELEASE)"
            cmake -DCMAKE_INSTALL_PREFIX:STRING="${stage}" \
                  -DCMAKE_CXX_FLAGS="$LL_BUILD_RELEASE -m$AUTOBUILD_ADDRSIZE" \
                  -DCMAKE_C_FLAGS="$(remove_cxxstd $LL_BUILD_RELEASE) -m$AUTOBUILD_ADDRSIZE" \
                  -DLUAU_USE_TAILSLIDE=ON \
                  "$top"
            cmake --build . -- -j8

            mkdir -p "$stage/bin"

            cp -v "libLuau.Ast.a" "$stage/lib/release"
            cp -v "libLuau.CodeGen.a" "$stage/lib/release"
            cp -v "libLuau.Common.a" "$stage/lib/release"
            cp -v "libLuau.Compiler.a" "$stage/lib/release"
            cp -v "libLuau.Config.a" "$stage/lib/release"
            cp -v "libLuau.VM.a" "$stage/lib/release"

            cp -v "slua" "$stage/bin/"

            # Run the conformance test for good measure
            "${stage}/build/Luau.Conformance"
        ;;
    esac
popd

mkdir -p "$stage/LICENSES"
cp "$top/LICENSE.txt" "$stage/LICENSES/luau.txt"
