#pragma once

#include <memory>
#include <string>
#include <vector>

struct diagnostic {
    size_t token_index = 0;  // index into the generated token stream
    std::string name;        // duplicate name
    std::string scope;       // module / function / lambda / block / class
};

// Which parser3 grammar the checker runs:
//   syntax    - the full VERA grammar only
//   semantics - the full grammar plus the class/module constraints (duplicate
//               class/field names and type references that must resolve)
enum class grammar_mode { syntax, semantics };

class vera_checker {
public:
    vera_checker();
    explicit vera_checker(grammar_mode mode);
    ~vera_checker();
    vera_checker(const vera_checker & other);
    vera_checker & operator=(const vera_checker & other);

    void feed(const std::string & piece);
    void finalize();
    bool would_introduce_diagnostic(const std::string & piece, bool finalize) const;
    const std::vector<diagnostic> & diagnostics() const;
    bool failed() const;
    grammar_mode mode() const;
    vera_checker clone() const;
    void reset();

private:
    struct impl;
    std::unique_ptr<impl> p_;
};

std::vector<diagnostic> vera_analyze(const std::string & text);
