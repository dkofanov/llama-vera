# llama-vera

This repository publishes **parser3**, a header-only, incremental, byte-level PEG
engine for constraining token sampling, and ships **`examples/llama-harness`**, an
example that uses it to drive llama.cpp completions.

```
parser3/                 the library
examples/llama-harness/  example consumer
```

## parser3

Header-only C++17; it needs no build step beyond adding its include directory.

Consume it from a CMake project:

```cmake
add_subdirectory(path/to/llama-vera/parser3)   # or find_package(parser3 REQUIRED)
target_link_libraries(myapp PRIVATE parser3::parser3)
```

Then in a source file:

```cpp
#include "parser3/core/context.h"
```

See `parser3/ARCHITECTURE.md` for the engine, the grammar DSL, and the semantic
hooks, and `parser3/grammars` for the grammar sources.

Build and test the library by itself:

```sh
cmake -S parser3 -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Install it (so external projects can `find_package(parser3)`):

```sh
cmake --install build --prefix /some/prefix
```

## Example harness (`examples/llama-harness`)

A direct llama.cpp completion driver with optional parser3 semantic filtering. It
needs a compatible installed llama.cpp CMake package — without one, the top-level
configure skips the example and still builds parser3.

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/llama.cpp/install
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Set `LLAMA_VERA_TEST_MODEL` to a GGUF path to enable the sampler integration test.

For the sibling development package in this workspace:

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=../llama-cli/third_party/llama
```

Usage:

```sh
./build/examples/llama-harness/llama-vera -m model.gguf -p 'class Point { x: int; } class ' -n 64
```

Generation is constrained by default with the parser3 `full_sem` grammar plus its
class semantics. The constraint mode is selectable:

| mode | flag | engine |
|---|---|---|
| default | *(none)* | parser3 `full_sem` + class semantics |
| parser3 syntax | `--no-sem` | parser3 full grammar, no semantics |
| built-in GBNF | `--gbnf` | llama.cpp GBNF (the bundled full grammar) |
| unconstrained | `--no-grammar` | none |

`-f FILE` reads one complete prompt. With neither `-p` nor `-f`, the prompt is
read from stdin. The constraint applies to the generated VERA program; the
natural-language prompt is not VERA source, so it is never fed to the parser.

## License

No license is granted yet.
