#!/usr/bin/env bash
# Regenerate everything derived from lsl-definitions.
#
# usage: regen_lsl_definitions.sh <lsl-definitions checkout>

set -eu

if [ $# -ne 1 ]; then
    echo "usage: $0 <lsl-definitions checkout>" >&2
    exit 1
fi

top="$(cd "$(dirname "$0")" && pwd)"
defs="$(cd "$1" && pwd)"

(cd "$defs" && python3 gen_definitions.py lsl_definitions.yaml gen_builtins_txt "$top/builtins.txt")
python3 "$top/tools/gen_primparams_methods.py" "$defs"
