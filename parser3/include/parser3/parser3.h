#pragma once

// Umbrella header for the parser3 library.

#include "parser3/util/bump_allocator.h"
#include "parser3/util/check.h"
#include "parser3/util/constexpr_bitset.h"
#include "parser3/util/type_traits.h"

#include "parser3/core/char_stream.h"
#include "parser3/core/context.h"
#include "parser3/core/context_view.h"
#include "parser3/core/grammar_element.h"
#include "parser3/core/grammar_frame.h"
#include "parser3/core/grammar_stack.h"
#include "parser3/core/semantics.h"
