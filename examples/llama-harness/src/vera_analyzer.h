#pragma once

#include <memory>
#include <string>
#include <vector>

struct diagnostic {
    size_t token_index = 0;  // index into the generated token stream
    std::string name;        // duplicate name
    std::string scope;       // module / function / lambda / block / class
};

class vera_checker {
public:
    vera_checker();
    ~vera_checker();
    vera_checker(const vera_checker & other);
    vera_checker & operator=(const vera_checker & other);

    void feed(const std::string & piece);
    void finalize();
    bool would_introduce_diagnostic(const std::string & piece, bool finalize) const;
    const std::vector<diagnostic> & diagnostics() const;
    vera_checker clone() const;
    void reset();

private:
    struct impl;
    std::unique_ptr<impl> p_;
};

std::vector<diagnostic> vera_analyze(const std::string & text);
