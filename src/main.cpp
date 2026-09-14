#include "semantic_sampler.h"

#include "llama.h"

#include <cerrno>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

struct options {
    std::string model;
    std::string prompt;
    std::string prompt_file;
    std::string grammar;
    std::string grammar_file;
    int32_t n_ctx = 0;
    int32_t n_predict = -1;
    int32_t n_gpu_layers = 0;
    int32_t threads = 0;
    int32_t top_k = 40;
    int32_t repeat_last_n = 64;
    float top_p = 0.95f;
    float min_p = 0.05f;
    float temp = 0.8f;
    float repeat_penalty = 1.0f;
    uint32_t seed = LLAMA_DEFAULT_SEED;
    bool semantic_no_dup = false;
    bool verbose = false;
};

static void print_usage(const char * program) {
    std::printf("usage: %s [options]\n\n", program);
    std::printf("  -m, --model FILE             model path\n");
    std::printf("  -p, --prompt TEXT            prompt text\n");
    std::printf("  -f, --file FILE              read the complete prompt from FILE\n");
    std::printf("  -c, --ctx-size N             context size (default: model setting)\n");
    std::printf("  -n, --predict N              tokens to predict (default: -1)\n");
    std::printf("  -t, --threads N              CPU threads (default: auto)\n");
    std::printf("  -ngl, --gpu-layers N         layers to offload (default: 0)\n");
    std::printf("  -s, --seed N                 RNG seed (default: random)\n");
    std::printf("      --temp F                 temperature (default: 0.8)\n");
    std::printf("      --top-k N                top-k (default: 40)\n");
    std::printf("      --top-p F                top-p (default: 0.95)\n");
    std::printf("      --min-p F                min-p (default: 0.05)\n");
    std::printf("      --repeat-last-n N        repetition window (default: 64)\n");
    std::printf("      --repeat-penalty F       repetition penalty (default: 1.0)\n");
    std::printf("      --grammar GBNF           GBNF grammar text\n");
    std::printf("      --grammar-file FILE      read GBNF grammar from FILE\n");
    std::printf("      --semantic-no-dup        forbid duplicate VERA declarations\n");
    std::printf("  -v, --verbose                show llama.cpp logs\n");
    std::printf("  -h, --help                   show this help\n");
}

static bool parse_i32(const char * text, int32_t & value) {
    errno = 0;
    char * end = nullptr;
    const long parsed = std::strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed < INT32_MIN || parsed > INT32_MAX) {
        return false;
    }
    value = static_cast<int32_t>(parsed);
    return true;
}

static bool parse_float(const char * text, float & value) {
    errno = 0;
    char * end = nullptr;
    const float parsed = std::strtof(text, &end);
    if (errno != 0 || end == text || *end != '\0') {
        return false;
    }
    value = parsed;
    return true;
}

static bool parse_args(int argc, char ** argv, options & opt) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto value = [&](const std::string & flag) -> const char * {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "error: missing value for %s\n", flag.c_str());
                return nullptr;
            }
            return argv[++i];
        };

        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            std::exit(0);
        } else if (arg == "-m" || arg == "--model") {
            const char * v = value(arg);
            if (!v) return false;
            opt.model = v;
        } else if (arg == "-p" || arg == "--prompt") {
            const char * v = value(arg);
            if (!v) return false;
            opt.prompt = v;
        } else if (arg == "-f" || arg == "--file") {
            const char * v = value(arg);
            if (!v) return false;
            opt.prompt_file = v;
        } else if (arg == "-c" || arg == "--ctx-size") {
            const char * v = value(arg);
            if (!v || !parse_i32(v, opt.n_ctx) || opt.n_ctx < 0) return false;
        } else if (arg == "-n" || arg == "--predict" || arg == "--n-predict") {
            const char * v = value(arg);
            if (!v || !parse_i32(v, opt.n_predict) || opt.n_predict < -1) return false;
        } else if (arg == "-t" || arg == "--threads") {
            const char * v = value(arg);
            if (!v || !parse_i32(v, opt.threads) || opt.threads < 0) return false;
        } else if (arg == "-ngl" || arg == "--gpu-layers" || arg == "--n-gpu-layers") {
            const char * v = value(arg);
            if (!v || !parse_i32(v, opt.n_gpu_layers)) return false;
        } else if (arg == "-s" || arg == "--seed") {
            int32_t seed = 0;
            const char * v = value(arg);
            if (!v || !parse_i32(v, seed)) return false;
            opt.seed = seed < 0 ? LLAMA_DEFAULT_SEED : static_cast<uint32_t>(seed);
        } else if (arg == "--temp" || arg == "--temperature") {
            const char * v = value(arg);
            if (!v || !parse_float(v, opt.temp) || opt.temp < 0.0f) return false;
        } else if (arg == "--top-k") {
            const char * v = value(arg);
            if (!v || !parse_i32(v, opt.top_k)) return false;
        } else if (arg == "--top-p") {
            const char * v = value(arg);
            if (!v || !parse_float(v, opt.top_p) || opt.top_p < 0.0f || opt.top_p > 1.0f) return false;
        } else if (arg == "--min-p") {
            const char * v = value(arg);
            if (!v || !parse_float(v, opt.min_p) || opt.min_p < 0.0f || opt.min_p > 1.0f) return false;
        } else if (arg == "--repeat-last-n") {
            const char * v = value(arg);
            if (!v || !parse_i32(v, opt.repeat_last_n)) return false;
        } else if (arg == "--repeat-penalty") {
            const char * v = value(arg);
            if (!v || !parse_float(v, opt.repeat_penalty) || opt.repeat_penalty <= 0.0f) return false;
        } else if (arg == "--grammar") {
            const char * v = value(arg);
            if (!v) return false;
            opt.grammar = v;
        } else if (arg == "--grammar-file") {
            const char * v = value(arg);
            if (!v) return false;
            opt.grammar_file = v;
        } else if (arg == "--semantic-no-dup") {
            opt.semantic_no_dup = true;
        } else if (arg == "-v" || arg == "--verbose") {
            opt.verbose = true;
        } else {
            std::fprintf(stderr, "error: unknown option: %s\n", arg.c_str());
            return false;
        }
    }

    if (opt.model.empty()) {
        std::fprintf(stderr, "error: a model is required\n");
        return false;
    }
    if (!opt.prompt.empty() && !opt.prompt_file.empty()) {
        std::fprintf(stderr, "error: --prompt and --file are mutually exclusive\n");
        return false;
    }
    if (!opt.grammar.empty() && !opt.grammar_file.empty()) {
        std::fprintf(stderr, "error: --grammar and --grammar-file are mutually exclusive\n");
        return false;
    }
    return true;
}

static bool read_file(const std::string & path, std::string & result) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return false;
    }
    result.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    return true;
}

static bool tokenize(const llama_vocab * vocab, const std::string & text, std::vector<llama_token> & tokens) {
    int32_t count = llama_tokenize(vocab, text.data(), text.size(), nullptr, 0, true, false);
    if (count == INT32_MIN) {
        return false;
    }
    if (count < 0) {
        count = -count;
    }
    tokens.resize(static_cast<size_t>(count));
    return llama_tokenize(vocab, text.data(), text.size(), tokens.data(), count, true, false) >= 0;
}

static std::string token_piece(const llama_vocab * vocab, llama_token token) {
    std::vector<char> buffer(64);
    int32_t count = llama_token_to_piece(vocab, token, buffer.data(), buffer.size(), 0, true);
    if (count < 0) {
        buffer.resize(static_cast<size_t>(-count));
        count = llama_token_to_piece(vocab, token, buffer.data(), buffer.size(), 0, true);
    }
    if (count <= 0) {
        return {};
    }
    return std::string(buffer.data(), static_cast<size_t>(count));
}

int main(int argc, char ** argv) {
    options opt;
    if (!parse_args(argc, argv, opt)) {
        print_usage(argv[0]);
        return 1;
    }

    if (!opt.prompt_file.empty() && !read_file(opt.prompt_file, opt.prompt)) {
        std::fprintf(stderr, "error: failed to read prompt file %s\n", opt.prompt_file.c_str());
        return 1;
    }
    if (opt.prompt.empty() && opt.prompt_file.empty()) {
        opt.prompt.assign(std::istreambuf_iterator<char>(std::cin), std::istreambuf_iterator<char>());
    }
    if (!opt.grammar_file.empty() && !read_file(opt.grammar_file, opt.grammar)) {
        std::fprintf(stderr, "error: failed to read grammar file %s\n", opt.grammar_file.c_str());
        return 1;
    }

    if (!opt.verbose) {
        llama_log_set([](ggml_log_level level, const char * text, void *) {
            if (level == GGML_LOG_LEVEL_ERROR) {
                std::fputs(text, stderr);
            }
        }, nullptr);
    }

    llama_backend_init();

    llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = opt.n_gpu_layers;
    llama_model * model = llama_model_load_from_file(opt.model.c_str(), model_params);
    if (!model) {
        std::fprintf(stderr, "error: failed to load model %s\n", opt.model.c_str());
        llama_backend_free();
        return 1;
    }

    const llama_vocab * vocab = llama_model_get_vocab(model);
    llama_context_params context_params = llama_context_default_params();
    context_params.n_ctx = opt.n_ctx > 0 ? opt.n_ctx : llama_model_n_ctx_train(model);
    context_params.n_batch = context_params.n_ctx;
    if (opt.threads > 0) {
        context_params.n_threads = opt.threads;
        context_params.n_threads_batch = opt.threads;
    }

    llama_context * ctx = llama_init_from_model(model, context_params);
    if (!ctx) {
        std::fprintf(stderr, "error: failed to create context\n");
        llama_model_free(model);
        llama_backend_free();
        return 1;
    }

    std::vector<llama_token> prompt_tokens;
    if (!tokenize(vocab, opt.prompt, prompt_tokens) || prompt_tokens.empty()) {
        std::fprintf(stderr, "error: failed to tokenize prompt\n");
        llama_free(ctx);
        llama_model_free(model);
        llama_backend_free();
        return 1;
    }
    if (prompt_tokens.size() >= llama_n_ctx(ctx)) {
        std::fprintf(stderr, "error: prompt exceeds context size\n");
        llama_free(ctx);
        llama_model_free(model);
        llama_backend_free();
        return 1;
    }

    llama_sampler * chain = llama_sampler_chain_init(llama_sampler_chain_default_params());
    llama_sampler * semantic = nullptr;

    if (!opt.grammar.empty()) {
        llama_sampler * grammar = llama_sampler_init_grammar(vocab, opt.grammar.c_str(), "root");
        if (!grammar) {
            std::fprintf(stderr, "error: failed to parse grammar\n");
            llama_sampler_free(chain);
            llama_free(ctx);
            llama_model_free(model);
            llama_backend_free();
            return 1;
        }
        llama_sampler_chain_add(chain, grammar);
    }
    if (opt.semantic_no_dup) {
        // parser3 constrains the generated VERA program only; the natural-language
        // prompt is not VERA source, so it is not fed to the checker.
        semantic = semantic_sampler_create(vocab);
        llama_sampler_chain_add(chain, semantic);
    }

    llama_sampler_chain_add(chain, llama_sampler_init_penalties(
        llama_vocab_n_tokens(vocab), opt.repeat_last_n, opt.repeat_penalty, 0.0f, 0.0f));
    llama_sampler_chain_add(chain, llama_sampler_init_top_k(opt.top_k));
    llama_sampler_chain_add(chain, llama_sampler_init_top_p(opt.top_p, 1));
    llama_sampler_chain_add(chain, llama_sampler_init_min_p(opt.min_p, 1));
    llama_sampler_chain_add(chain, llama_sampler_init_temp(opt.temp));
    llama_sampler_chain_add(chain, llama_sampler_init_dist(opt.seed));

    llama_batch batch = llama_batch_get_one(prompt_tokens.data(), prompt_tokens.size());
    int32_t generated = 0;
    int result = 0;

    while (opt.n_predict < 0 || generated < opt.n_predict) {
        const int32_t used = llama_memory_seq_pos_max(llama_get_memory(ctx), 0) + 1;
        if (used + batch.n_tokens > static_cast<int32_t>(llama_n_ctx(ctx))) {
            std::fprintf(stderr, "error: context size exceeded\n");
            result = 1;
            break;
        }
        if (llama_decode(ctx, batch) != 0) {
            std::fprintf(stderr, "error: decode failed\n");
            result = 1;
            break;
        }

        llama_token token = LLAMA_TOKEN_NULL;
        if (semantic) {
            const float * logits = llama_get_logits_ith(ctx, -1);
            const int32_t n_vocab = llama_vocab_n_tokens(vocab);
            std::vector<llama_token_data> candidates;
            candidates.reserve(static_cast<size_t>(n_vocab));
            for (llama_token id = 0; id < n_vocab; ++id) {
                candidates.push_back({id, logits[id], 0.0f});
            }
            llama_token_data_array candidate_array = {candidates.data(), candidates.size(), -1, false};
            llama_sampler_apply(chain, &candidate_array);
            if (!semantic_sampler_has_candidates(semantic)) {
                std::fprintf(stderr, "error: semantic constraints rejected every candidate\n");
                result = 2;
                break;
            }
            if (candidate_array.selected < 0 ||
                candidate_array.selected >= static_cast<int64_t>(candidate_array.size) ||
                !std::isfinite(candidate_array.data[candidate_array.selected].logit)) {
                std::fprintf(stderr, "error: sampler did not select a valid token\n");
                result = 1;
                break;
            }
            token = candidate_array.data[candidate_array.selected].id;
            llama_sampler_accept(chain, token);
        } else {
            token = llama_sampler_sample(chain, ctx, -1);
        }
        if (llama_vocab_is_eog(vocab, token)) {
            break;
        }

        const std::string piece = token_piece(vocab, token);
        if (piece.empty()) {
            std::fprintf(stderr, "error: failed to decode token piece\n");
            result = 1;
            break;
        }
        std::fwrite(piece.data(), 1, piece.size(), stdout);
        std::fflush(stdout);

        batch = llama_batch_get_one(&token, 1);
        ++generated;
    }

    llama_sampler_free(chain);
    llama_free(ctx);
    llama_model_free(model);
    llama_backend_free();
    return result;
}
