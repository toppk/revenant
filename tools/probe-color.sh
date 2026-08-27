#!/bin/sh
# Human-run sampler for attributes, 16/256 colors, and truecolor.

set -eu

probe_dir=$(CDPATH= cd "$(dirname "$0")" && pwd)
exec "$probe_dir/../tdn/tools/sgr-sampler" "$@"
