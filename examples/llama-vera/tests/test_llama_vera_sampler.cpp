#include "llama_vera_sampler.h"

#include "llama.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

static int failures = 0;

static std::vector<llama_token> tokenize(const llama_vocab * vocab, const std::string & text) {
    int32_t count = llama_tokenize(vocab, text.data(), text.size(), nullptr, 0, false, false);
    if (count < 0) {
        count = -count;
    }
    std::vector<llama_token> tokens(static_cast<size_t>(count));
    if (llama_tokenize(vocab, text.data(), text.size(), tokens.data(), count, false, false) < 0) {
        return {};
    }
    return tokens;
}

static llama_token token_for(const llama_vocab * vocab, const std::string & text) {
    const std::vector<llama_token> tokens = tokenize(vocab, text);
    return tokens.size() == 1 ? tokens[0] : LLAMA_TOKEN_NULL;
}

static float filtered_logit(llama_sampler * sampler, llama_token token) {
    llama_token_data candidate = {token, 1.0f, 0.0f};
    llama_token_data_array candidates = {&candidate, 1, -1, false};
    llama_sampler_apply(sampler, &candidates);
    return candidate.logit;
}

static void expect(bool value, const char * name) {
    if (!value) {
        ++failures;
        std::printf("FAIL  %s\n", name);
    }
}

int main(int argc, char ** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s MODEL\n", argv[0]);
        return 1;
    }

    llama_backend_init();
    llama_model * model = llama_model_load_from_file(argv[1], llama_model_default_params());
    if (!model) {
        std::fprintf(stderr, "failed to load model\n");
        llama_backend_free();
        return 1;
    }

    const llama_vocab * vocab = llama_model_get_vocab(model);
    llama_sampler * sampler = llama_vera_sampler_create(vocab, vera_mode::semantics);
    // A duplicate class whose body is about to close: the closing brace is the
    // point at which the duplicate name becomes visible.
    llama_vera_sampler_feed(sampler, "class Foo {} class Foo { x: int; ");

    const llama_token close = token_for(vocab, "}");
    const llama_token extension = token_for(vocab, "X");
    const llama_token start = token_for(vocab, "class");
    expect(close != LLAMA_TOKEN_NULL, "tokenize close brace");
    expect(extension != LLAMA_TOKEN_NULL, "tokenize extension");
    expect(start != LLAMA_TOKEN_NULL, "tokenize identifier");
    if (close != LLAMA_TOKEN_NULL) {
        expect(!std::isfinite(filtered_logit(sampler, close)), "filter closing a duplicate class");
    }
    if (extension != LLAMA_TOKEN_NULL) {
        expect(std::isfinite(filtered_logit(sampler, extension)), "allow identifier extension");
    }

    // Complete the duplicate; the checker fails and filters everything.
    llama_vera_sampler_feed(sampler, "}");
    if (start != LLAMA_TOKEN_NULL) {
        expect(!std::isfinite(filtered_logit(sampler, start)), "failed checker filters candidates");
    }

    llama_sampler * clone = llama_sampler_clone(sampler);
    llama_sampler_reset(clone);
    if (start != LLAMA_TOKEN_NULL) {
        expect(std::isfinite(filtered_logit(clone, start)), "reset clears declarations");
    }

    llama_sampler_free(clone);
    llama_sampler_free(sampler);
    llama_model_free(model);
    llama_backend_free();

    if (failures != 0) {
        std::printf("%d test(s) failed\n", failures);
        return 1;
    }
    std::printf("all tests passed\n");
    return 0;
}
