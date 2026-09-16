#pragma once

#include "llama.h"
#include "parser3_vera.h"

#include <string>

// llama.cpp sampler adapter over a parser3_vera checker.  vera_mode selects the
// full grammar (syntax) or the full_sem grammar with its class semantics.
struct llama_sampler * llama_vera_sampler_create(const struct llama_vocab * vocab, vera_mode mode);

void llama_vera_sampler_feed(struct llama_sampler * smpl, const std::string & text);
bool llama_vera_sampler_has_candidates(const struct llama_sampler * smpl);
