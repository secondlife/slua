#1/bin/bash

set -e

pushd "$( dirname "${BASH_SOURCE[0]}" )" > /dev/null

mkdir -p build_fuzz
pushd build_fuzz
mkdir -p fuzz_inputs

CLANG_BIN=clang
CLANGPP_BIN=clang++

if [[ "$OSTYPE" == "darwin"* ]]; then
  # Make sure we're not using AppleClang because it doesn't support libfuzzer.
  CLANG_BIN="$( brew --prefix llvm )/bin/clang"
  CLANGPP_BIN="$( brew --prefix llvm )/bin/clang++"
fi

find ../tests/ -name "*.lsl" | grep -v "/expected/" | xargs -I'{}' cp '{}' fuzz_inputs/
CXX="${CLANGPP_BIN}" CC="${CLANG_BIN}" cmake .. -DTAILSLIDE_BUILD_FUZZER=on
cmake --build .
./tailslide-fuzzer ./fuzz_inputs/ -fork=8
