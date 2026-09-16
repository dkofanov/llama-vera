#!/usr/bin/env bash
# Regenerate the committed parser3 grammar headers from their .grammar sources.
#
#   tools/generate_grammars.sh          rewrite the headers
#   tools/generate_grammars.sh --check  fail if a header is out of date
#
# The pairs live in tools/grammars.txt.  Headers stay committed, so a consumer
# never needs Python; this is a development step only.  Paths are relative to
# the parser3 root so the generated "Generated from ..." comments are stable.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${root}"

gen="tools/grammar_gen.py"
manifest="tools/grammars.txt"

check=0
if [[ "${1:-}" == "--check" ]]; then
    check=1
fi

status=0
while read -r src out ns; do
    if [[ -z "${src}" || "${src}" == \#* ]]; then
        continue
    fi
    tmp="$(mktemp)"
    python3 "${gen}" "${src}" -n "${ns}" -o "${tmp}" --clang-format
    if [[ ${check} -eq 1 ]]; then
        if ! diff -q "${out}" "${tmp}" >/dev/null; then
            echo "out of date: ${out}" >&2
            status=1
        fi
    else
        mv "${tmp}" "${out}"
        echo "wrote ${out}"
    fi
    rm -f "${tmp}"
done < "${manifest}"

exit ${status}
