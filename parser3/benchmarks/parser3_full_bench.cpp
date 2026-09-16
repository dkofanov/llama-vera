// Throughput for the full Vera grammar (examples/full_grammar.h) on a
// synthetic program, used by the flamegraph tooling.
//
// Usage: parser3-full-bench [units] [iterations] [chunk]
//   units:      number of function+class pairs (default 300)
//   iterations: timed repetitions of the whole feed (default 20)
//   chunk:      bytes per Feed call (default 64)

#include "parser3/core/context.h"
#include "parser3/core/context_view.h"
#include "parser3/examples/full_grammar.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>

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

bool FeedChunked(const std::string &src, std::size_t chunk) {
    Context context(kStackSlots, std::in_place_type<Root>);
    for (std::size_t off = 0; off < src.size(); off += chunk) {
        const std::size_t n = std::min(chunk, src.size() - off);
        context.chars.Append(src.data() + off, n);
        if (!context.stack.Head()->Feed(ContextView{context})) {
            return false;
        }
        if (context.stack.Head() == nullptr) {
            break;
        }
    }
    if (context.stack.Head() != nullptr) {
        context.chars.Finish();
        context.stack.Head()->Feed(ContextView{context});
    }
    return context.stack.Head() == nullptr && context.chars.Empty();
}

} // namespace

int main(int argc, char **argv) {
    const int units = argc > 1 ? std::atoi(argv[1]) : 300;
    const int iterations = argc > 2 ? std::atoi(argv[2]) : 20;
    const std::size_t chunk = argc > 3 ? static_cast<std::size_t>(std::strtoull(argv[3], nullptr, 10)) : 64;

    const std::string src = MakeDocument(units);
    if (!FeedChunked(src, chunk)) {
        std::fprintf(stderr, "full grammar rejected the benchmark document\n");
        return 1;
    }

    using Clock = std::chrono::steady_clock;
    Clock::duration span{};
    std::uint64_t bytes = 0;
    for (int i = 0; i < iterations; ++i) {
        const auto start = Clock::now();
        const bool accepted = FeedChunked(src, chunk);
        span += Clock::now() - start;
        if (!accepted) {
            std::fprintf(stderr, "full grammar rejected the benchmark document\n");
            return 1;
        }
        bytes += src.size();
    }

    const double seconds = std::chrono::duration<double>(span).count();
    std::printf("parser3 full-grammar throughput\n");
    std::printf("  config    : %s\n", PARSER3_BUILD_TYPE);
    std::printf("  program   : %d units, %zu bytes\n", units, src.size());
    std::printf("  chunk     : %zu bytes\n", chunk);
    std::printf("  iterations: %d\n", iterations);
    std::printf("  %.2f ns/byte  %.1f MB/s\n", seconds * 1e9 / static_cast<double>(bytes),
                static_cast<double>(bytes) / seconds / 1e6);
    return 0;
}
