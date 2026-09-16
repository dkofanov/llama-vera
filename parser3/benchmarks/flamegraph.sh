#!/usr/bin/env bash
# Generate parser3 flamegraphs with perf.
#
# Release builds carry debug info and frame pointers, so the flamegraphs are
# generated directly from the O3 `release` build (no separate configuration).
#
# Usage: benchmarks/flamegraph.sh [mode]
#   mode: synthetic | full | both   (default: both)
#
# Artifacts: build/flamegraph/parser3-*.svg plus a top-symbols *.txt per run.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
mode="${1:-both}"
flame_dir="${root}/third_party/FlameGraph"
out_dir="${root}/build/flamegraph"
mkdir -p "${out_dir}"

if [[ ! -x "${flame_dir}/flamegraph.pl" ]]; then
    echo "missing ${flame_dir}/flamegraph.pl" >&2
    exit 1
fi

configure_and_build() {
    local preset="$1"
    (
        cd "${root}"
        cmake --preset "${preset}" >/dev/null
        cmake --build "build/${preset}" --target parser3-bench parser3-full-bench >/dev/null
    )
}

# C++ template types are verbose enough to bloat the SVG; collapse `<...>`
# arguments (innermost first) while leaving non-templated names intact.
shorten_symbols() {
    sed -E ':a; s/<[^<>]*>//g; ta' | cut -c1-400
}

record() {
    local preset="$1" label="$2" title="$3" bench="$4"
    shift 4
    local binary="${root}/build/${preset}/benchmarks/${bench}"
    local data="${out_dir}/perf-${label}.data"
    local svg="${out_dir}/parser3-${label}.svg"
    local txt="${out_dir}/parser3-${label}.txt"
    echo "recording ${label}: ${bench} $*"
    perf record -q -F 4000 --call-graph fp -o "${data}" -- "${binary}" "$@" >/dev/null
    perf report -i "${data}" --stdio --no-children --sort symbol -g none 2>/dev/null \
        | shorten_symbols | cut -c1-220 | head -40 > "${txt}" || true
    perf script -i "${data}" \
        | "${flame_dir}/stackcollapse-perf.pl" \
        | shorten_symbols \
        | "${flame_dir}/flamegraph.pl" --title "${title}" \
        > "${svg}"
    rm -f "${data}"
    echo "wrote ${svg}"
}

if [[ "${mode}" == "synthetic" || "${mode}" == "both" ]]; then
    configure_and_build release
    record release "synthetic" "parser3 synthetic (O3)" parser3-bench 20000 1234
fi

if [[ "${mode}" == "full" || "${mode}" == "both" ]]; then
    configure_and_build release
    record release "full" "parser3 full grammar (O3)" parser3-full-bench 300 40 64
fi

echo "done"
