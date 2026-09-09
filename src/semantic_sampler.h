#pragma once

#include "llama.h"
#include "vera_analyzer.h"

#include <string>

struct llama_sampler * semantic_sampler_create(const struct llama_vocab * vocab);

bool semantic_sampler_is(const struct llama_sampler * smpl);
void semantic_sampler_feed(struct llama_sampler * smpl, const std::string & text);
bool semantic_sampler_has_candidates(const struct llama_sampler * smpl);
