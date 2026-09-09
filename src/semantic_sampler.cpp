#include "semantic_sampler.h"

#include <cmath>
#include <string>
#include <vector>

struct semantic_ctx {
    const llama_vocab * vocab;
    vera_checker checker;
    bool has_candidates = true;
};

static std::string token_piece(const llama_vocab * vocab, llama_token token) {
    std::vector<char> buffer(64);
    int32_t n = llama_token_to_piece(vocab, token, buffer.data(), buffer.size(), 0, false);
    if (n < 0) {
        buffer.resize(static_cast<size_t>(-n));
        n = llama_token_to_piece(vocab, token, buffer.data(), buffer.size(), 0, false);
    }
    if (n <= 0) {
        return {};
    }
    return std::string(buffer.data(), static_cast<size_t>(n));
}

static const char * semantic_name(const struct llama_sampler *) {
    return "semantic-no-dup";
}

static void semantic_apply(struct llama_sampler * smpl, llama_token_data_array * cur_p) {
    semantic_ctx * ctx = static_cast<semantic_ctx *>(smpl->ctx);
    ctx->has_candidates = false;

    for (size_t i = 0; i < cur_p->size; i++) {
        if (!std::isfinite(cur_p->data[i].logit)) {
            continue;
        }

        const llama_token token = cur_p->data[i].id;
        const bool eog = llama_vocab_is_eog(ctx->vocab, token);
        const std::string piece = eog ? std::string() : token_piece(ctx->vocab, token);
        if ((!eog && piece.empty()) || ctx->checker.would_introduce_diagnostic(piece, eog)) {
            cur_p->data[i].logit = -INFINITY;
            continue;
        }
        ctx->has_candidates = true;
    }

    cur_p->sorted = false;
}

static void semantic_accept(struct llama_sampler * smpl, llama_token token) {
    semantic_ctx * ctx = static_cast<semantic_ctx *>(smpl->ctx);
    if (llama_vocab_is_eog(ctx->vocab, token)) {
        ctx->checker.finalize();
        return;
    }

    const std::string piece = token_piece(ctx->vocab, token);
    if (!piece.empty()) {
        ctx->checker.feed(piece);
    }
}

static void semantic_reset(struct llama_sampler * smpl) {
    semantic_ctx * ctx = static_cast<semantic_ctx *>(smpl->ctx);
    ctx->checker.reset();
    ctx->has_candidates = true;
}

static struct llama_sampler * semantic_clone(const struct llama_sampler * smpl) {
    const semantic_ctx * src = static_cast<const semantic_ctx *>(smpl->ctx);
    semantic_ctx * dst = new semantic_ctx();
    dst->vocab = src->vocab;
    dst->checker = src->checker;
    dst->has_candidates = src->has_candidates;
    return llama_sampler_init(smpl->iface, dst);
}

static void semantic_free(struct llama_sampler * smpl) {
    delete static_cast<semantic_ctx *>(smpl->ctx);
}

static struct llama_sampler_i semantic_iface = {
    semantic_name,
    semantic_accept,
    semantic_apply,
    semantic_reset,
    semantic_clone,
    semantic_free,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
};

struct llama_sampler * semantic_sampler_create(const struct llama_vocab * vocab) {
    semantic_ctx * ctx = new semantic_ctx();
    ctx->vocab = vocab;
    return llama_sampler_init(&semantic_iface, ctx);
}

bool semantic_sampler_is(const struct llama_sampler * smpl) {
    return smpl != nullptr && smpl->iface == &semantic_iface;
}

void semantic_sampler_feed(struct llama_sampler * smpl, const std::string & text) {
    static_cast<semantic_ctx *>(smpl->ctx)->checker.feed(text);
}

bool semantic_sampler_has_candidates(const struct llama_sampler * smpl) {
    return static_cast<const semantic_ctx *>(smpl->ctx)->has_candidates;
}
