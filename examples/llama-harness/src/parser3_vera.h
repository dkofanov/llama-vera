#pragma once

#include <memory>
#include <string>
#include <vector>

struct diagnostic {
    size_t token_index = 0;  // index into the generated token stream
    std::string name;        // duplicate name
    std::string scope;       // module / class
};

// Which parser3 grammar the checker runs:
//   syntax    - the full VERA grammar only
//   semantics - the full grammar plus the class/module constraints (duplicate
//               class/field names and type references that must resolve)
enum class vera_mode { syntax, semantics };

// A parser3 instance for VERA: incremental feed of generated text, per-candidate
// viability probes, and (in semantics mode) diagnostics.  It does not depend on
// llama.cpp; the sampler adapter in llama_vera_sampler.h wraps it.
class parser3_vera {
public:
    parser3_vera();
    explicit parser3_vera(vera_mode mode);
    ~parser3_vera();
    parser3_vera(const parser3_vera & other);
    parser3_vera & operator=(const parser3_vera & other);

    void feed(const std::string & piece);
    void finalize();
    bool would_introduce_diagnostic(const std::string & piece, bool finalize) const;
    const std::vector<diagnostic> & diagnostics() const;
    bool failed() const;
    vera_mode mode() const;
    parser3_vera clone() const;
    void reset();

private:
    struct impl;
    std::unique_ptr<impl> p_;
};

std::vector<diagnostic> vera_diagnostics(const std::string & text);
