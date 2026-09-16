// Measures the overhead of the copy-based Context fork: each ptoken is tested in
// N forks (all fed the same bytes) and one is committed, versus parsing the same
// document in a single context.
//
// Usage: llama-vera-fork-bench [units] [iterations] [chunk]
//   units:      function+class pairs (default 50)
//   iterations: timed repetitions per mode (default 10)
//   chunk:      bytes per ptoken (default 64)

#include "parser3/core/context.h"
#include "parser3/core/context_view.h"
#include "parser3-generated/full_grammar.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace vera::parser3;
using vera::parser3::full::Root;

#ifndef PARSER3_BUILD_TYPE
#define PARSER3_BUILD_TYPE "unknown"
#endif

constexpr std::size_t kStackSlots = 1 << 20;

std::string MakeDocument(int units) {
    std::string src;
    for (int i = 0; i < units; ++i) {
        src += "function fn";
        src += std::to_string(i);
        src += "(a: int, b: number): int { let x: int = a + b * 2; if (x > 0) { return x; } else { return 0; } }\n";
        src += "class C";
        src += std::to_string(i);
        src += " { f: int; g: string; }\n";
    }
    return src;
}

// Run the document, forking `forks` copies per ptoken and committing one. A
// single fork (forks == 1) is the no-fork baseline.
bool RunOnce(const std::string &src, std::size_t chunk, int forks) {
    Context base(kStackSlots, std::in_place_type<Root>);
    for (std::size_t off = 0; off < src.size(); off += chunk) {
        const std::size_t n = std::min(chunk, src.size() - off);
        const char *bytes = src.data() + off;

        if (forks <= 1) {
            base.chars.Append(bytes, n);
            if (!base.stack.Head()->Feed(ContextView{base})) {
                return false;
            }
        } else {
            std::vector<Context> trials;
            trials.reserve(static_cast<std::size_t>(forks));
            for (int i = 0; i < forks; ++i) {
                trials.push_back(base.Fork());
            }
            bool accepted = false;
            std::size_t winner = 0;
            for (std::size_t i = 0; i < trials.size(); ++i) {
                trials[i].chars.Append(bytes, n);
                const bool ok = trials[i].stack.Head()->Feed(ContextView{trials[i]});
                if (ok && !accepted) {
                    accepted = true;
                    winner = i;
                }
            }
            if (!accepted) {
                return false;
            }
            base.Commit(std::move(trials[winner]));
        }

        if (base.stack.Head() == nullptr) {
            break;
        }
    }
    if (base.stack.Head() != nullptr) {
        base.chars.Finish();
        base.stack.Head()->Feed(ContextView{base});
    }
    return base.stack.Head() == nullptr && base.chars.Empty();
}

// Per-fork cost at a representative (half-parsed) state, isolating the copy from
// parsing. Returns microseconds per fork.
double ForkOnlyMicros(const std::string &src, std::size_t chunk, std::size_t repetitions) {
    Context base(kStackSlots, std::in_place_type<Root>);
    const std::size_t half = src.size() / 2;
    for (std::size_t off = 0; off < half; off += chunk) {
        const std::size_t n = std::min(chunk, half - off);
        base.chars.Append(src.data() + off, n);
        base.stack.Head()->Feed(ContextView{base});
        if (base.stack.Head() == nullptr) {
            break;
        }
    }

    using Clock = std::chrono::steady_clock;
    std::size_t sink = 0;
    const auto start = Clock::now();
    for (std::size_t i = 0; i < repetitions; ++i) {
        Context fork = base.Fork();
        sink += fork.chars.CurrentOffset();
    }
    const auto span = Clock::now() - start;
    volatile std::size_t escaped = sink; // keep the fork copies observable
    (void)escaped;
    const double seconds = std::chrono::duration<double>(span).count();
    return seconds * 1e6 / static_cast<double>(repetitions);
}

} // namespace

int main(int argc, char **argv) {
    const int units = argc > 1 ? std::atoi(argv[1]) : 50;
    const int iterations = argc > 2 ? std::atoi(argv[2]) : 10;
    const std::size_t chunk = argc > 3 ? static_cast<std::size_t>(std::strtoull(argv[3], nullptr, 10)) : 64;

    const std::string src = MakeDocument(units);
    const int fork_counts[] = {1, 2, 3};

    for (int forks : fork_counts) {
        if (!RunOnce(src, chunk, forks)) {
            std::fprintf(stderr, "full grammar rejected the benchmark document (forks=%d)\n", forks);
            return 1;
        }
    }

    double baseline = 0.0;
    std::printf("parser3 fork/commit overhead\n");
    std::printf("  config    : %s\n", PARSER3_BUILD_TYPE);
    std::printf("  program   : %d units, %zu bytes\n", units, src.size());
    std::printf("  chunk     : %zu bytes/ptoken\n", chunk);
    std::printf("  iterations: %d\n", iterations);
    std::printf("  %-8s %14s %12s %10s\n", "forks", "ns/byte", "MB/s", "overhead");
    for (int forks : fork_counts) {
        using Clock = std::chrono::steady_clock;
        Clock::duration span{};
        std::uint64_t bytes = 0;
        for (int i = 0; i < iterations; ++i) {
            const auto start = Clock::now();
            const bool accepted = RunOnce(src, chunk, forks);
            span += Clock::now() - start;
            if (!accepted) {
                std::fprintf(stderr, "full grammar rejected the benchmark document (forks=%d)\n", forks);
                return 1;
            }
            bytes += src.size();
        }
        const double seconds = std::chrono::duration<double>(span).count();
        const double ns_per_byte = seconds * 1e9 / static_cast<double>(bytes);
        if (forks == 1) {
            baseline = ns_per_byte;
        }
        std::printf("  %-8d %14.1f %12.1f %9.2fx\n", forks, ns_per_byte,
                    static_cast<double>(bytes) / seconds / 1e6, ns_per_byte / baseline);
    }

    const double fork_micros = ForkOnlyMicros(src, chunk, 20000);
    std::printf("  fork copy (half-parsed) : %.2f us/fork\n", fork_micros);
    return 0;
}
