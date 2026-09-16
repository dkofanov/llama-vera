#include "parser3_sampler.h"

#include <cmath>
#include <string>
#include <vector>

struct parser3_ctx {
    const llama_vocab * vocab;
    vera_checker checker;
    bool has_candidates = true;

    // A candidate whose first byte the current grammar state cannot consume is
    // rejected outright, so a 256-entry mask prunes most of the vocab before the
    // per-candidate fork.  It is rebuilt lazily whenever the state advances.
    bool mask_valid = false;
    bool mask[256] = {};
    bool eog_ok = false;

    parser3_ctx(const llama_vocab * v, grammar_mode mode) : vocab(v), checker(mode) {}
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

static const char * parser3_name(const struct llama_sampler *) {
    return "parser3";
}

static void rebuild_mask(parser3_ctx * ctx) {
    for (int byte = 0; byte < 256; ++byte) {
        const char c = static_cast<char>(byte);
        ctx->mask[byte] = !ctx->checker.would_introduce_diagnostic(std::string(1, c), false);
    }
    ctx->eog_ok = !ctx->checker.would_introduce_diagnostic(std::string(), true);
    ctx->mask_valid = true;
}

static void parser3_apply(struct llama_sampler * smpl, llama_token_data_array * cur_p) {
    parser3_ctx * ctx = static_cast<parser3_ctx *>(smpl->ctx);
    if (!ctx->mask_valid) {
        rebuild_mask(ctx);
    }
    ctx->has_candidates = false;

    for (size_t i = 0; i < cur_p->size; i++) {
        if (!std::isfinite(cur_p->data[i].logit)) {
            continue;
        }

        const llama_token token = cur_p->data[i].id;
        if (llama_vocab_is_eog(ctx->vocab, token)) {
            if (!ctx->eog_ok) {
                cur_p->data[i].logit = -INFINITY;
                continue;
            }
        } else {
            const std::string piece = token_piece(ctx->vocab, token);
            const bool viable = !piece.empty() && ctx->mask[static_cast<unsigned char>(piece[0])] &&
                                !ctx->checker.would_introduce_diagnostic(piece, false);
            if (!viable) {
                cur_p->data[i].logit = -INFINITY;
                continue;
            }
        }
        ctx->has_candidates = true;
    }

    cur_p->sorted = false;
}

static void parser3_accept(struct llama_sampler * smpl, llama_token token) {
    parser3_ctx * ctx = static_cast<parser3_ctx *>(smpl->ctx);
    ctx->mask_valid = false;
    if (llama_vocab_is_eog(ctx->vocab, token)) {
        ctx->checker.finalize();
        return;
    }

    const std::string piece = token_piece(ctx->vocab, token);
    if (!piece.empty()) {
        ctx->checker.feed(piece);
    }
}

static void parser3_reset(struct llama_sampler * smpl) {
    parser3_ctx * ctx = static_cast<parser3_ctx *>(smpl->ctx);
    ctx->checker.reset();
    ctx->has_candidates = true;
    ctx->mask_valid = false;
}

static struct llama_sampler * parser3_clone(const struct llama_sampler * smpl) {
    const parser3_ctx * src = static_cast<const parser3_ctx *>(smpl->ctx);
    parser3_ctx * dst = new parser3_ctx(src->vocab, src->checker.mode());
    dst->checker = src->checker;
    dst->has_candidates = src->has_candidates;
    dst->mask_valid = src->mask_valid;
    for (int i = 0; i < 256; ++i) {
        dst->mask[i] = src->mask[i];
    }
    dst->eog_ok = src->eog_ok;
    return llama_sampler_init(smpl->iface, dst);
}

static void parser3_free(struct llama_sampler * smpl) {
    delete static_cast<parser3_ctx *>(smpl->ctx);
}

static struct llama_sampler_i parser3_iface = {
    parser3_name,
    parser3_accept,
    parser3_apply,
    parser3_reset,
    parser3_clone,
    parser3_free,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
};

struct llama_sampler * parser3_sampler_create(const struct llama_vocab * vocab, grammar_mode mode) {
    return llama_sampler_init(&parser3_iface, new parser3_ctx(vocab, mode));
}

void parser3_sampler_feed(struct llama_sampler * smpl, const std::string & text) {
    parser3_ctx * ctx = static_cast<parser3_ctx *>(smpl->ctx);
    ctx->checker.feed(text);
    ctx->mask_valid = false;
}

bool parser3_sampler_has_candidates(const struct llama_sampler * smpl) {
    return static_cast<const parser3_ctx *>(smpl->ctx)->has_candidates;
}
