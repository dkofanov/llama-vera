#pragma once

// Author bindings for grammars/full_sem.grammar: class semantic constraints.
// Bindings are invoked at the @-elements of the grammar. Rejections are hard
// feed failures (return false); there are no diagnostics and no throws.

#include "parser3/core/char_stream.h"
#include "parser3/core/context_view.h"
#include "parser3/core/grammar_element.h"
#include "parser3/core/grammar_stack.h"
#include "parser3/core/semantics.h"

#include <cstddef>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace vera::parser3 {

// Tracks the module and class scopes while a program is parsed:
//  - a class name is declared in the module scope when its definition closes;
//  - a field name is declared in the class scope when it is read;
//  - a type reference is validated, character by character, against the type
//    names declared so far (built-ins plus earlier classes).
class ClassSemantics : public SemanticState {
  public:
    struct Field {
        std::string name;
        std::string type; // resolved type name; empty for built-in/structural
    };
    struct Scope {
        std::string class_name; // empty for the module scope
        bool is_class = false;
        std::vector<Field> fields;
        std::unordered_map<std::string, std::size_t> names;
    };

    ClassSemantics() {
        scopes_.emplace_back();
        for (const char *builtin : {"int", "number", "boolean", "string", "null"}) {
            builtins_.insert(builtin);
        }
    }

    // Last violation seen, for callers that report diagnostics (name + scope).
    const std::string &violated_name() const {
        return violated_name_;
    }
    const std::string &violated_scope() const {
        return violated_scope_;
    }

    std::unique_ptr<SemanticState> Clone() const override {
        return std::make_unique<ClassSemantics>(*this);
    }

    void CopyFrom(const SemanticState &other) override {
        *this = static_cast<const ClassSemantics &>(other);
    }

    std::size_t Watermark() const override {
        return (scopes_.size() & 0xffffu) | ((log_.size() & 0xffffu) << 16) | ((name_acc_.size() & 0xffu) << 32) |
               ((type_acc_.size() & 0xffu) << 40) | ((completed_names_.size() & 0xffu) << 48);
    }

    void Rewind(std::size_t watermark) override {
        const std::size_t want_scopes = watermark & 0xffffu;
        const std::size_t want_log = (watermark >> 16) & 0xffffu;
        const std::size_t want_name = (watermark >> 32) & 0xffu;
        const std::size_t want_type = (watermark >> 40) & 0xffu;
        const std::size_t want_completed = (watermark >> 48) & 0xffu;
        while (log_.size() > want_log) {
            const Undo undo = std::move(log_.back());
            log_.pop_back();
            scopes_[undo.scope].names.erase(undo.name);
            if (undo.type_decl) {
                declared_.erase(undo.name);
            }
            if (undo.field && !scopes_[undo.scope].fields.empty()) {
                scopes_[undo.scope].fields.pop_back();
            }
        }
        scopes_.resize(want_scopes);
        name_acc_.resize(want_name);
        type_acc_.resize(want_type);
        completed_names_.resize(want_completed);
    }

    // -- bindings ----------------------------------------------------------

    void name_begin() {
        name_acc_.emplace_back();
    }
    void name_char(char c) {
        name_acc_.back().push_back(c);
    }
    void name_end() {
        completed_names_.push_back(std::move(name_acc_.back()));
        name_acc_.pop_back();
    }

    void type_begin() {
        type_acc_.emplace_back();
    }
    bool type_char(char c) {
        type_acc_.back().push_back(c);
        if (!viable(type_acc_.back())) {
            violated_name_ = type_acc_.back();
            violated_scope_ = "class";
            return false;
        }
        return true;
    }
    bool type_end() {
        std::string name = std::move(type_acc_.back());
        type_acc_.pop_back();
        if (builtins_.count(name) == 0 && declared_.count(name) == 0) {
            violated_name_ = name;
            violated_scope_ = "class";
            return false;
        }
        last_type_ = std::move(name);
        return true;
    }

    bool field_declare() {
        if (scopes_.empty() || !scopes_.back().is_class) {
            return false;
        }
        std::string name = pop_name();
        Scope &scope = scopes_.back();
        if (scope.names.count(name) != 0) {
            violated_name_ = name;
            violated_scope_ = "class";
            return false;
        }
        scope.names.emplace(name, scope.fields.size());
        scope.fields.push_back(Field{name, {}});
        log_.push_back(Undo{scopes_.size() - 1, std::move(name), false, true});
        return true;
    }

    void field_bind() {
        if (!scopes_.empty() && scopes_.back().is_class && !scopes_.back().fields.empty()) {
            scopes_.back().fields.back().type = last_type_;
        }
        last_type_.clear();
    }

    bool scope_open() {
        Scope scope;
        scope.is_class = true;
        scope.class_name = pop_name();
        scopes_.push_back(std::move(scope));
        return true;
    }

    bool scope_close() {
        Scope scope = std::move(scopes_.back());
        scopes_.pop_back();
        Scope &module = scopes_.front();
        if (module.names.count(scope.class_name) != 0) {
            violated_name_ = scope.class_name;
            violated_scope_ = "module";
            return false;
        }
        module.names.emplace(scope.class_name, 0);
        declared_.insert(scope.class_name);
        log_.push_back(Undo{0, scope.class_name, true, false});
        return true;
    }

  private:
    struct Undo {
        std::size_t scope;
        std::string name;
        bool type_decl;
        bool field;
    };

    bool viable(std::string_view prefix) const {
        for (const std::string &name : builtins_) {
            if (name.size() >= prefix.size() && name.compare(0, prefix.size(), prefix) == 0) {
                return true;
            }
        }
        for (const std::string &name : declared_) {
            if (name.size() >= prefix.size() && name.compare(0, prefix.size(), prefix) == 0) {
                return true;
            }
        }
        return false;
    }

    std::string pop_name() {
        std::string name = std::move(completed_names_.back());
        completed_names_.pop_back();
        return name;
    }

    std::vector<Scope> scopes_;
    std::set<std::string> builtins_;
    std::set<std::string> declared_;
    std::vector<Undo> log_;
    std::vector<std::string> name_acc_;
    std::vector<std::string> type_acc_;
    std::vector<std::string> completed_names_;
    std::string last_type_;
    std::string violated_name_;
    std::string violated_scope_;
};

template <>
struct SignalAction<full_sem::nota::Name_Begin> {
    static bool Feed(ContextView ctx) {
        ctx.stack.SemanticsAs<ClassSemantics>()->name_begin();
        return true;
    }
};

template <>
struct SignalAction<full_sem::nota::Name_Char> {
    static bool Feed(ContextView ctx) {
        ctx.stack.SemanticsAs<ClassSemantics>()->name_char(ctx.chars.ByteAt(ctx.chars.CurrentOffset() - 1));
        return true;
    }
};

template <>
struct SignalAction<full_sem::nota::Name_End> {
    static bool Feed(ContextView ctx) {
        ctx.stack.SemanticsAs<ClassSemantics>()->name_end();
        return true;
    }
};

template <>
struct SignalAction<full_sem::nota::Type_Begin> {
    static bool Feed(ContextView ctx) {
        ctx.stack.SemanticsAs<ClassSemantics>()->type_begin();
        return true;
    }
};

template <>
struct SignalAction<full_sem::nota::Type_Char> {
    static bool Feed(ContextView ctx) {
        return ctx.stack.SemanticsAs<ClassSemantics>()->type_char(ctx.chars.ByteAt(ctx.chars.CurrentOffset() - 1));
    }
};

template <>
struct SignalAction<full_sem::nota::Type_End> {
    static bool Feed(ContextView ctx) {
        return ctx.stack.SemanticsAs<ClassSemantics>()->type_end();
    }
};

template <>
struct SignalAction<full_sem::nota::Field_Declare> {
    static bool Feed(ContextView ctx) {
        return ctx.stack.SemanticsAs<ClassSemantics>()->field_declare();
    }
};

template <>
struct SignalAction<full_sem::nota::Field_Bind> {
    static bool Feed(ContextView ctx) {
        ctx.stack.SemanticsAs<ClassSemantics>()->field_bind();
        return true;
    }
};

template <>
struct SignalAction<full_sem::nota::Scope_Begin_Class> {
    static bool Feed(ContextView ctx) {
        return ctx.stack.SemanticsAs<ClassSemantics>()->scope_open();
    }
};

template <>
struct SignalAction<full_sem::nota::Scope_End_Class> {
    static bool Feed(ContextView ctx) {
        return ctx.stack.SemanticsAs<ClassSemantics>()->scope_close();
    }
};

} // namespace vera::parser3
