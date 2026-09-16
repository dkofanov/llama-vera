#pragma once

#include "llama.h"
#include "vera_analyzer.h"

#include <string>

// Constrain sampling with parser3.  grammar_mode selects the full grammar
// (syntax) or the full_sem grammar with its class semantics.
struct llama_sampler * parser3_sampler_create(const struct llama_vocab * vocab, grammar_mode mode);

void parser3_sampler_feed(struct llama_sampler * smpl, const std::string & text);
bool parser3_sampler_has_candidates(const struct llama_sampler * smpl);
