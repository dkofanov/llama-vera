#include "vera_analyzer.h"

#include <algorithm>
#include <cctype>
#include <set>

namespace {

enum class kind {
    KEYWORD,
    IDENT,
    STRING,
    NUMBER,
    PUNCT,
};

struct tok {
    kind k = kind::PUNCT;
    std::string text;
    size_t piece_index = 0;
};

bool is_ident_start(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

bool is_ident_cont(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '$';
}

const std::set<std::string> & keywords() {
    static const std::set<std::string> kw = {
        "as", "async", "await", "boolean", "break", "catch", "class", "continue",
        "else", "error", "export", "false", "for", "from", "function", "if",
        "import", "int", "let", "null", "number", "of", "return", "string",
        "true", "try", "while",
    };
    return kw;
}

struct parser {
    const std::vector<tok> & toks;
    size_t idx = 0;
    std::vector<diagnostic> diags;
    std::vector<std::pair<std::string, std::set<std::string>>> scopes;

    explicit parser(const std::vector<tok> & t) : toks(t) {}

    const tok & peek(size_t ahead = 0) {
        static const tok sentinel{kind::PUNCT, "", 0};
        const size_t p = idx + ahead;
        if (p >= toks.size()) {
            return sentinel;
        }
        return toks[p];
    }

    tok next() {
        static const tok sentinel{kind::PUNCT, "", 0};
        if (idx >= toks.size()) {
            return sentinel;
        }
        return toks[idx++];
    }

    bool at_end() {
        return idx >= toks.size();
    }

    bool kw(const std::string & w) {
        return peek().k == kind::KEYWORD && peek().text == w;
    }

    bool punct(const std::string & p) {
        return peek().k == kind::PUNCT && peek().text == p;
    }

    bool eat_kw(const std::string & w) {
        if (kw(w)) {
            next();
            return true;
        }
        return false;
    }

    bool eat_punct(const std::string & p) {
        if (punct(p)) {
            next();
            return true;
        }
        return false;
    }

    void push_scope(const std::string & k) {
        scopes.push_back({k, {}});
    }

    void pop_scope() {
        if (!scopes.empty()) {
            scopes.pop_back();
        }
    }

    void declare(const std::string & name, size_t piece_index) {
        if (scopes.empty()) {
            return;
        }
        auto & names = scopes.back().second;
        if (!names.insert(name).second) {
            diags.push_back({piece_index, name, scopes.back().first});
        }
    }

    void parse_type() {
        if (kw("int") || kw("number") || kw("boolean") || kw("string")) {
            next();
        } else if (kw("async")) {
            next();
            if (kw("function")) {
                next();
                parse_signature(false);
            }
        } else if (kw("function")) {
            next();
            parse_signature(false);
        } else if (punct("(")) {
            next();
            parse_type();
            if (eat_punct("|")) {
                eat_kw("null");
            }
            eat_punct(")");
        } else if (peek().k == kind::IDENT) {
            next();
            if (eat_punct(".")) {
                if (peek().k == kind::IDENT) {
                    next();
                }
            }
        } else {
            next();
        }
        while (eat_punct("[")) {
            eat_punct("]");
        }
    }

    void parse_signature(bool declare_params) {
        if (eat_punct("(")) {
            while (!at_end() && !punct(")")) {
                if (peek().k == kind::IDENT) {
                    tok p = next();
                    if (declare_params) {
                        declare(p.text, p.piece_index);
                    }
                } else {
                    next();
                }
                eat_punct(":");
                parse_type();
                if (!eat_punct(",")) {
                    break;
                }
            }
            eat_punct(")");
        }
        if (eat_punct(":")) {
            parse_type();
        }
    }

    void parse_block() {
        if (!eat_punct("{")) {
            return;
        }
        push_scope("block");
        while (!at_end() && !punct("}")) {
            parse_statement();
        }
        eat_punct("}");
        pop_scope();
    }

    void parse_import() {
        next();  // import
        eat_punct("*");
        eat_kw("as");
        if (peek().k == kind::IDENT) {
            tok t = next();
            declare(t.text, t.piece_index);
        }
        while (!at_end()) {
            if (peek().k == kind::STRING) {
                next();
                break;
            }
            if (peek().k == kind::KEYWORD &&
                (peek().text == "import" || peek().text == "function" ||
                 peek().text == "class" || peek().text == "export" ||
                 peek().text == "async")) {
                break;
            }
            next();
        }
    }

    void parse_function() {
        next();  // function
        if (peek().k == kind::IDENT) {
            tok t = next();
            declare(t.text, t.piece_index);
        }
        push_scope("function");
        parse_signature(true);
        parse_block();
        pop_scope();
    }

    void parse_class() {
        next();  // class
        if (peek().k == kind::IDENT) {
            tok t = next();
            declare(t.text, t.piece_index);
        }
        if (!eat_punct("{")) {
            return;
        }
        push_scope("class");
        while (!at_end() && !punct("}")) {
            if (peek().k == kind::IDENT) {
                tok t = next();
                declare(t.text, t.piece_index);
                eat_punct(":");
                parse_type();
                eat_punct(";");
            } else {
                next();
            }
        }
        eat_punct("}");
        pop_scope();
    }

    void parse_declaration() {
        eat_kw("export");
        if (kw("function")) {
            parse_function();
        } else if (kw("async")) {
            next();
            if (kw("function")) {
                parse_function();
            } else {
                next();
            }
        } else if (punct("@")) {
            next();
            if (peek().k == kind::IDENT) {
                next();
            }
            if (kw("class")) {
                parse_class();
            } else {
                next();
            }
        } else if (kw("class")) {
            parse_class();
        } else {
            next();
        }
    }

    void parse_lambda() {
        eat_kw("async");
        if (!eat_kw("function")) {
            return;
        }
        push_scope("lambda");
        parse_signature(true);
        parse_block();
        pop_scope();
    }

    void parse_primary() {
        if (peek().k == kind::IDENT || peek().k == kind::NUMBER || peek().k == kind::STRING) {
            next();
        } else if (kw("true") || kw("false") || kw("null")) {
            next();
        } else if (kw("async") || kw("function")) {
            parse_lambda();
            return;
        } else if (punct("(")) {
            next();
            parse_expression();
            eat_punct(")");
        } else if (punct("[")) {
            next();
            while (!at_end() && !punct("]")) {
                parse_expression();
                if (!eat_punct(",")) {
                    break;
                }
            }
            eat_punct("]");
        } else if (punct("{")) {
            next();
            while (!at_end() && !punct("}")) {
                if (peek().k == kind::IDENT) {
                    next();
                }
                eat_punct(":");
                parse_expression();
                if (!eat_punct(",")) {
                    break;
                }
            }
            eat_punct("}");
        } else {
            next();
            return;
        }

        while (true) {
            if (punct("(")) {
                next();
                while (!at_end() && !punct(")")) {
                    parse_expression();
                    if (!eat_punct(",")) {
                        break;
                    }
                }
                eat_punct(")");
            } else if (punct("[")) {
                next();
                parse_expression();
                eat_punct("]");
            } else if (punct(".")) {
                next();
                if (peek().k == kind::IDENT) {
                    next();
                }
            } else if (punct("!")) {
                next();
            } else {
                break;
            }
        }
    }

    void parse_unary() {
        if (punct("!") || punct("-")) {
            next();
            parse_unary();
            return;
        }
        if (kw("await")) {
            next();
            parse_unary();
            return;
        }
        parse_primary();
    }

    int binary_prec(const tok & t) {
        if (t.k != kind::PUNCT) {
            return -1;
        }
        const std::string & p = t.text;
        if (p == "||") {
            return 1;
        }
        if (p == "&&") {
            return 2;
        }
        if (p == "===" || p == "!==") {
            return 3;
        }
        if (p == "<" || p == "<=" || p == ">" || p == ">=") {
            return 4;
        }
        if (p == "+" || p == "-") {
            return 5;
        }
        if (p == "*" || p == "/" || p == "%") {
            return 6;
        }
        return -1;
    }

    void parse_binary(int min_prec) {
        parse_unary();
        while (true) {
            const int prec = binary_prec(peek());
            if (prec < min_prec) {
                break;
            }
            next();
            parse_binary(prec + 1);
        }
    }

    void parse_expression() {
        parse_binary(0);
    }

    void parse_statement() {
        if (kw("if")) {
            next();
            eat_punct("(");
            parse_expression();
            eat_punct(")");
            parse_block();
            if (eat_kw("else")) {
                if (kw("if")) {
                    parse_statement();
                } else {
                    parse_block();
                }
            }
        } else if (kw("while")) {
            next();
            eat_punct("(");
            parse_expression();
            eat_punct(")");
            parse_block();
        } else if (kw("for")) {
            next();
            eat_punct("(");
            eat_kw("let");
            std::string loop_var;
            size_t loop_var_index = 0;
            if (peek().k == kind::IDENT) {
                tok t = next();
                loop_var = t.text;
                loop_var_index = t.piece_index;
            }
            eat_punct(":");
            parse_type();
            eat_kw("of");
            parse_expression();
            eat_punct(")");
            if (!eat_punct("{")) {
                return;
            }
            push_scope("block");
            if (!loop_var.empty()) {
                declare(loop_var, loop_var_index);
            }
            while (!at_end() && !punct("}")) {
                parse_statement();
            }
            eat_punct("}");
            pop_scope();
        } else if (kw("break") || kw("continue")) {
            next();
            eat_punct(";");
        } else if (kw("return")) {
            next();
            parse_expression();
            eat_punct(";");
        } else if (kw("try")) {
            next();
            parse_block();
            eat_kw("catch");
            eat_punct("(");
            std::string catch_var;
            size_t catch_var_index = 0;
            if (peek().k == kind::IDENT) {
                tok t = next();
                catch_var = t.text;
                catch_var_index = t.piece_index;
            }
            eat_punct(")");
            if (!eat_punct("{")) {
                return;
            }
            push_scope("block");
            if (!catch_var.empty()) {
                declare(catch_var, catch_var_index);
            }
            while (!at_end() && !punct("}")) {
                parse_statement();
            }
            eat_punct("}");
            pop_scope();
        } else if (kw("let")) {
            next();
            if (peek().k == kind::IDENT) {
                tok t = next();
                declare(t.text, t.piece_index);
            }
            eat_punct(":");
            parse_type();
            if (eat_punct("=")) {
                parse_expression();
            }
            eat_punct(";");
        } else {
            parse_expression();
            if (eat_punct("=")) {
                parse_expression();
            }
            eat_punct(";");
        }
    }

    void parse_module() {
        push_scope("module");
        while (!at_end()) {
            if (kw("import")) {
                parse_import();
            } else {
                parse_declaration();
            }
        }
        pop_scope();
    }

    std::vector<diagnostic> run() {
        parse_module();
        return diags;
    }
};

}  // namespace

struct vera_checker::impl {
    std::string pending;
    size_t pending_abs = 0;
    std::vector<size_t> piece_ends;
    std::vector<tok> committed;
    std::vector<diagnostic> diags;

    size_t piece_of(size_t abs) const {
        return static_cast<size_t>(
            std::upper_bound(piece_ends.begin(), piece_ends.end(), abs) - piece_ends.begin());
    }

    void commit(kind k, const std::string & text, size_t rel_start) {
        committed.push_back({k, text, piece_of(pending_abs + rel_start)});
    }

    void reparse() {
        parser p(committed);
        diags = p.run();
    }

    void lex_pending() {
        const size_t n = pending.size();
        size_t i = 0;
        while (i < n) {
            const char c = pending[i];
            if (std::isspace(static_cast<unsigned char>(c))) {
                i++;
                continue;
            }
            if (c == '/' && i + 1 < n && pending[i + 1] == '/') {
                const size_t nl = pending.find('\n', i);
                if (nl == std::string::npos) {
                    break;
                }
                i = nl + 1;
                continue;
            }
            if (c == '"') {
                size_t j = i + 1;
                bool closed = false;
                while (j < n) {
                    if (pending[j] == '\\' && j + 1 < n) {
                        j += 2;
                        continue;
                    }
                    if (pending[j] == '"') {
                        closed = true;
                        j++;
                        break;
                    }
                    j++;
                }
                if (!closed) {
                    break;
                }
                commit(kind::STRING, pending.substr(i, j - i), i);
                i = j;
                continue;
            }
            if (is_ident_start(c)) {
                size_t j = i + 1;
                while (j < n && is_ident_cont(pending[j])) {
                    j++;
                }
                if (j >= n) {
                    break;
                }
                const std::string word = pending.substr(i, j - i);
                const kind k = keywords().count(word) ? kind::KEYWORD : kind::IDENT;
                commit(k, word, i);
                i = j;
                continue;
            }
            if (std::isdigit(static_cast<unsigned char>(c))) {
                size_t j = i;
                while (j < n && std::isdigit(static_cast<unsigned char>(pending[j]))) {
                    j++;
                }
                if (j < n && pending[j] == '.' && j + 1 < n &&
                    std::isdigit(static_cast<unsigned char>(pending[j + 1]))) {
                    j++;
                    while (j < n && std::isdigit(static_cast<unsigned char>(pending[j]))) {
                        j++;
                    }
                }
                if (j < n && (pending[j] == 'e' || pending[j] == 'E')) {
                    size_t k = j + 1;
                    if (k < n && (pending[k] == '+' || pending[k] == '-')) {
                        k++;
                    }
                    if (k < n && std::isdigit(static_cast<unsigned char>(pending[k]))) {
                        j = k;
                        while (j < n && std::isdigit(static_cast<unsigned char>(pending[j]))) {
                            j++;
                        }
                    }
                }
                if (j >= n) {
                    break;
                }
                commit(kind::NUMBER, pending.substr(i, j - i), i);
                i = j;
                continue;
            }
            const std::string puncts = "{}()[];:.,=<>+-*/%!|&@?";
            if (puncts.find(c) != std::string::npos) {
                commit(kind::PUNCT, std::string(1, c), i);
                i++;
                continue;
            }
            i++;
        }
        if (i > 0) {
            pending.erase(0, i);
            pending_abs += i;
        }
    }

    void feed(const std::string & piece) {
        if (pending.empty()) {
            pending_abs = piece_ends.empty() ? 0 : piece_ends.back();
        }
        pending += piece;
        piece_ends.push_back(pending_abs + pending.size());

        const size_t before = committed.size();
        lex_pending();
        if (committed.size() != before) {
            reparse();
        }
    }

    void finalize() {
        const size_t n = pending.size();
        if (n == 0) {
            reparse();
            return;
        }
        if (is_ident_start(pending[0])) {
            size_t j = 1;
            while (j < n && is_ident_cont(pending[j])) {
                j++;
            }
            const std::string word = pending.substr(0, j);
            const kind k = keywords().count(word) ? kind::KEYWORD : kind::IDENT;
            commit(k, word, 0);
            pending.erase(0, j);
            pending_abs += j;
        } else if (std::isdigit(static_cast<unsigned char>(pending[0]))) {
            size_t j = 1;
            while (j < n && std::isdigit(static_cast<unsigned char>(pending[j]))) {
                j++;
            }
            if (j < n && pending[j] == '.' && j + 1 < n &&
                std::isdigit(static_cast<unsigned char>(pending[j + 1]))) {
                j++;
                while (j < n && std::isdigit(static_cast<unsigned char>(pending[j]))) {
                    j++;
                }
            }
            commit(kind::NUMBER, pending.substr(0, j), 0);
            pending.erase(0, j);
            pending_abs += j;
        }
        reparse();
    }

    void reset() {
        pending.clear();
        pending_abs = 0;
        piece_ends.clear();
        committed.clear();
        diags.clear();
    }
};

vera_checker::vera_checker() : p_(new impl()) {}

vera_checker::~vera_checker() = default;

vera_checker::vera_checker(const vera_checker & other) : p_(new impl(*other.p_)) {}

vera_checker & vera_checker::operator=(const vera_checker & other) {
    if (this != &other) {
        p_.reset(new impl(*other.p_));
    }
    return *this;
}

void vera_checker::feed(const std::string & piece) {
    p_->feed(piece);
}

void vera_checker::finalize() {
    p_->finalize();
}

bool vera_checker::would_introduce_diagnostic(const std::string & piece, bool finalize_candidate) const {
    vera_checker candidate(*this);
    const size_t before = candidate.diagnostics().size();
    if (!piece.empty()) {
        candidate.feed(piece);
    }
    if (finalize_candidate) {
        candidate.finalize();
    }
    return candidate.diagnostics().size() > before;
}

const std::vector<diagnostic> & vera_checker::diagnostics() const {
    return p_->diags;
}

vera_checker vera_checker::clone() const {
    return vera_checker(*this);
}

void vera_checker::reset() {
    p_->reset();
}

std::vector<diagnostic> vera_analyze(const std::string & text) {
    vera_checker c;
    c.feed(text);
    c.finalize();
    return c.diagnostics();
}
