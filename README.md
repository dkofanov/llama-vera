# llama-vera

A direct llama.cpp completion driver with optional VERA duplicate-declaration filtering.

## Build

Provide a compatible installed llama.cpp CMake package:

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/llama.cpp/install
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Set `LLAMA_VERA_TEST_MODEL` to a GGUF path to enable the sampler integration test:

```sh
cmake -S . -B build \
  -DCMAKE_PREFIX_PATH=/path/to/llama.cpp/install \
  -DLLAMA_VERA_TEST_MODEL=/path/to/model.gguf
```

For the sibling development package in this workspace:

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=../llama-cli/third_party/llama
```

## Usage

```sh
./build/llama-vera \
  -m model.gguf \
  -p 'function foo(): null {} function ' \
  --semantic-no-dup \
  -n 64
```

`-f FILE` reads one complete prompt. With neither `-p` nor `-f`, the prompt is read from stdin. Use `--grammar` or `--grammar-file` for optional GBNF constraints. Duplicate checking includes both prompt and generated text when `--semantic-no-dup` is enabled.
