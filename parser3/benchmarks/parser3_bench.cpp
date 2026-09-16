#include "synthetic_grammar.h"

#include "parser3/core/context.h"
#include "parser3/core/context_view.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace vera::parser3;
using namespace parser3_bench;

#ifndef PARSER3_BUILD_TYPE
#define PARSER3_BUILD_TYPE "unknown"
#endif

using Clock = std::chrono::steady_clock;

double ToSeconds(Clock::duration span) {
    return std::chrono::duration<double>(span).count();
}

// Consecutive chunk lengths covering `size` bytes.
std::vector<std::size_t> FixedLengths(std::size_t size, std::size_t chunk) {
    std::vector<std::size_t> lengths;
    for (std::size_t pos = 0; pos < size; pos += chunk) {
        lengths.push_back(std::min(chunk, size - pos));
    }
    return lengths;
}

// Random 1..max piece lengths, simulating tokenizer pieces.
std::vector<std::size_t> TokenLengths(std::size_t size, unsigned seed, std::size_t max) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<std::size_t> dist(1, max);
    std::vector<std::size_t> lengths;
    std::size_t remaining = size;
    while (remaining > 0) {
        const std::size_t length = std::min(dist(rng), remaining);
        lengths.push_back(length);
        remaining -= length;
    }
    return lengths;
}

bool FeedOnce(const std::string &document, const std::vector<std::size_t> &lengths) {
    Context context(kStackSlots, std::in_place_type<Root>);
    std::size_t pos = 0;
    for (std::size_t length : lengths) {
        context.chars.Append(document.data() + pos, length);
        pos += length;
        if (!context.stack.Head()->Feed(ContextView{context})) {
            return false;
        }
    }
    return true;
}

struct Metrics {
    double feed_ns_per_byte = 0.0;
    double feed_mbps = 0.0;
    double total_ns_per_byte = 0.0;
};

Metrics RunMode(const std::string &document, const std::vector<std::size_t> &lengths, std::size_t iterations) {
    if (!FeedOnce(document, lengths)) {
        std::fprintf(stderr, "grammar rejected the benchmark document\n");
        std::exit(1);
    }

    std::uint64_t bytes = 0;
    Clock::duration feed_span{};
    Clock::duration total_span{};
    for (std::size_t i = 0; i < iterations; ++i) {
        const auto total_start = Clock::now();
        Context context(kStackSlots, std::in_place_type<Root>);
        const auto feed_start = Clock::now();
        std::size_t pos = 0;
        bool accepted = true;
        for (std::size_t length : lengths) {
            context.chars.Append(document.data() + pos, length);
            pos += length;
            accepted = context.stack.Head()->Feed(ContextView{context});
            if (!accepted) {
                break;
            }
        }
        const auto feed_end = Clock::now();
        feed_span += feed_end - feed_start;
        total_span += feed_end - total_start;
        bytes += accepted ? pos : 0;
    }

    const double feed_seconds = ToSeconds(feed_span);
    const double total_seconds = ToSeconds(total_span);
    const double byte_count = static_cast<double>(bytes);
    Metrics metrics;
    metrics.feed_ns_per_byte = feed_seconds * 1e9 / byte_count;
    metrics.feed_mbps = byte_count / feed_seconds / 1e6;
    metrics.total_ns_per_byte = total_seconds * 1e9 / byte_count;
    return metrics;
}

} // namespace

int main(int argc, char **argv) {
    std::size_t iterations = 20000;
    unsigned seed = 1234;
    if (argc > 1) {
        iterations = static_cast<std::size_t>(std::strtoull(argv[1], nullptr, 10));
    }
    if (argc > 2) {
        seed = static_cast<unsigned>(std::strtoul(argv[2], nullptr, 10));
    }

    const std::string document = MakeDocument();
    const std::size_t size = document.size();

    struct Mode {
        const char *name;
        std::vector<std::size_t> lengths;
    };
    const Mode modes[] = {
        {"random 1-5 B tokens", TokenLengths(size, seed, 5)},
        {"bytes (1)", FixedLengths(size, 1)},
        {"bytes (4)", FixedLengths(size, 4)},
    };

    std::printf("parser3 throughput\n");
    std::printf("  config    : %s\n", PARSER3_BUILD_TYPE);
    std::printf("  grammar   : synthetic, %zu units\n", kUnits);
    std::printf("  input     : %zu bytes\n", size);
    std::printf("  seed      : %u\n", seed);
    std::printf("  iterations: %zu\n", iterations);
    std::printf("  %-20s %12s %12s %12s\n", "chunking", "feed ns/B", "feed MB/s", "total ns/B");
    for (const Mode &mode : modes) {
        const Metrics metrics = RunMode(document, mode.lengths, iterations);
        std::printf("  %-20s %12.1f %12.1f %12.1f\n", mode.name, metrics.feed_ns_per_byte, metrics.feed_mbps,
                    metrics.total_ns_per_byte);
    }
    std::printf("\n");
    return 0;
}
