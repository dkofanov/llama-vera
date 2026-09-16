#!/usr/bin/env bash
# Run the parser3 throughput matrix (tokenized vs byte chunks) for Debug and
# Release in one shot. Invoked by the `run-benchmarks` build target; can also be
# run directly.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
iters="${PARSER3_BENCH_ITERS:-20000}"
seed="${PARSER3_BENCH_SEED:-1234}"
current_config="${PARSER3_CURRENT_CONFIG:-}"
current_bin="${PARSER3_CURRENT_BIN:-}"
current_lower="$(printf '%s' "${current_config}" | tr '[:upper:]' '[:lower:]')"

# The config we are invoked from is already built (the target DEPENDS on it);
# building it again from within its own ninja would be unsafe, so only that one
# is skipped and the other is configured/built on demand.
ensure_built() {
    local preset="$1"
    if [[ "${preset}" == "${current_lower}" && -n "${current_bin}" ]]; then
        return
    fi
    (
        cd "${root}"
        cmake --preset "${preset}" >/dev/null
        cmake --build "build/${preset}" --target parser3-bench >/dev/null
    )
}

binary_for() {
    local preset="$1"
    if [[ "${preset}" == "${current_lower}" && -n "${current_bin}" ]]; then
        printf '%s\n' "${current_bin}"
    else
        printf '%s\n' "${root}/build/${preset}/benchmarks/parser3-bench"
    fi
}

ensure_built debug
ensure_built release

printf 'parser3 benchmark matrix (iterations=%s seed=%s)\n\n' "${iters}" "${seed}"
"$(binary_for debug)" "${iters}" "${seed}"
"$(binary_for release)" "${iters}" "${seed}"
