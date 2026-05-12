#include "analyzer.hpp"
#include "syntax_index.hpp"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <memory>
#include <slang/diagnostics/DiagnosticEngine.h>
#include <slang/syntax/AllSyntax.h>
#include <slang/syntax/SyntaxTree.h>
#include <slang/syntax/SyntaxVisitor.h>
#include <slang/text/SourceManager.h>

std::shared_ptr<DocumentState> Analyzer::make_state(const std::string& uri,
                                                    const std::string& text) const {
    // Pass URI as name (display label) and stripped filesystem path as path
    // (used by SourceManager::assignText for include resolution relative to
    // the file's directory, not the server CWD).
    std::string path = uri;
    if (path.starts_with("file://"))
        path = path.substr(7);
    // Fresh SourceManager per document snapshot: avoids "path already assigned"
    // errors when the same file is re-parsed on didChange, and prevents the
    // static singleton from accumulating stale buffers across edits.
    auto sm = std::make_unique<slang::SourceManager>();
    auto tree = slang::syntax::SyntaxTree::fromText(std::string_view(text), *sm,
                                                    std::string_view(uri), std::string_view(path));
    auto state = std::make_shared<DocumentState>(uri, text, nullptr);
    state->source_manager = std::move(sm);
    state->tree = std::move(tree);
    // Format diagnostics immediately while the SyntaxTree arena is alive.
    // Do NOT copy slang::Diagnostic objects — their ConstantValue args can
    // contain internal pointers that are not safely copyable.
    if (state->tree) {
        const auto& diags = state->tree->diagnostics();
        auto& sm = state->tree->sourceManager();
        slang::DiagnosticEngine engine(sm);
        for (const auto& d : diags) {
            ParseDiagInfo info;
            try {
                auto loc = d.location.valid() ? sm.getFullyExpandedLoc(d.location) : d.location;
                if (loc.valid() && sm.isFileLoc(loc)) {
                    size_t ln = sm.getLineNumber(loc);
                    size_t col = sm.getColumnNumber(loc);
                    info.line = ln > 0 ? (int)ln - 1 : 0;
                    info.col = col > 0 ? (int)col - 1 : 0;
                }
            } catch (...) {
            }
            auto sev = slang::getDefaultSeverity(d.code);
            if (sev == slang::DiagnosticSeverity::Error || sev == slang::DiagnosticSeverity::Fatal)
                info.severity = 1;
            else if (sev == slang::DiagnosticSeverity::Warning)
                info.severity = 2;
            else
                info.severity = 3;
            try {
                info.message = engine.formatMessage(d);
            } catch (...) {
                info.message = "(diagnostic format error)";
            }
            state->parse_diagnostics.push_back(std::move(info));
        }
    }
    return state;
}

void Analyzer::open(const std::string& uri, const std::string& text) {
    auto state = make_state(uri, text);
    std::lock_guard<std::mutex> lock(map_mutex_);
    docs_[uri] = state;
}

void Analyzer::change(const std::string& uri, const std::string& text) {
    auto state = make_state(uri, text);
    std::lock_guard<std::mutex> lock(map_mutex_);
    docs_[uri] = state;
}

void Analyzer::close(const std::string& uri) {
    std::lock_guard<std::mutex> lock(map_mutex_);
    docs_.erase(uri);
}

std::shared_ptr<const DocumentState> Analyzer::get_state(const std::string& uri) const {
    std::lock_guard<std::mutex> lock(map_mutex_);
    auto it = docs_.find(uri);
    if (it == docs_.end())
        return nullptr;
    return it->second;
}

struct IdentifierSpan {
    std::string text;
    int start_col{0};
    int end_col{0};
};

// Extract identifier at (0-based line, 0-based col) from source text.
static std::optional<IdentifierSpan> extract_ident_span(std::string_view src, int line, int col) {
    int cur = 0;
    size_t pos = 0;
    while (pos < src.size() && cur < line) {
        if (src[pos] == '\n')
            ++cur;
        ++pos;
    }
    if (cur < line)
        return std::nullopt;

    size_t ls = pos;
    size_t le = src.find('\n', pos);
    if (le == std::string_view::npos)
        le = src.size();

    if (col < 0 || (size_t)col >= le - ls)
        return std::nullopt;
    size_t ip = ls + col;

    auto is_id = [](char c) { return std::isalnum((unsigned char)c) || c == '_' || c == '$'; };
    if (!is_id(src[ip]))
        return std::nullopt;

    size_t start = ip;
    while (start > ls && is_id(src[start - 1]))
        --start;
    size_t end = ip;
    while (end < le && is_id(src[end]))
        ++end;

    return IdentifierSpan{std::string(src.substr(start, end - start)), (int)(start - ls),
                          (int)(end - ls)};
}

static std::string extract_ident(std::string_view src, int line, int col) {
    auto span = extract_ident_span(src, line, col);
    return span ? span->text : std::string{};
}

static int to_lsp_line(int one_based_line) { return one_based_line > 0 ? one_based_line - 1 : 0; }

static std::string path_to_uri(const std::filesystem::path& path) {
    return "file://" + std::filesystem::absolute(path).lexically_normal().string();
}

static std::string file_name_to_uri(std::string_view file_name, const std::string& fallback_uri) {
    if (file_name.empty())
        return fallback_uri;

    std::string file(file_name);
    if (file.starts_with("file://"))
        return file;
    return path_to_uri(file);
}

struct IndexedFile {
    std::string uri;
    SyntaxIndex index;
};

static std::optional<IndexedFile> build_index_for_file(const std::filesystem::path& path) {
    slang::SourceManager sm;
    auto tree_or_error = slang::syntax::SyntaxTree::fromFile(path.string(), sm);
    if (!tree_or_error)
        return std::nullopt;

    return IndexedFile{path_to_uri(path), SyntaxIndex::build(**tree_or_error)};
}

static std::optional<Location>
find_module_definition(const SyntaxIndex& index, const std::string& uri, const std::string& name) {
    for (const auto& module : index.modules) {
        if (module.name == name) {
            const int line = to_lsp_line(module.line);
            return Location{uri, line, module.col, line, module.col + (int)module.name.size()};
        }
    }
    return std::nullopt;
}

static std::optional<Location> find_port_definition(const SyntaxIndex& index,
                                                    const std::string& uri,
                                                    const std::string& module_name,
                                                    const std::string& port_name) {
    for (const auto& module : index.modules) {
        if (module.name != module_name)
            continue;
        for (const auto& port : module.ports) {
            if (port.name == port_name) {
                const int line = to_lsp_line(port.line);
                return Location{uri, line, port.col, line, port.col + (int)port.name.size()};
            }
        }
    }
    return std::nullopt;
}

static Location location_from_token(const slang::SourceManager& sm, const std::string& uri,
                                    const slang::parsing::Token& token) {
    const int line = to_lsp_line((int)sm.getLineNumber(token.location()));
    const int col = (int)sm.getColumnNumber(token.location()) - 1;
    return Location{uri, line, col, line, col + (int)token.valueText().size()};
}

static Location location_from_token_actual_uri(const slang::SourceManager& sm,
                                               const std::string& fallback_uri,
                                               const slang::parsing::Token& token) {
    auto loc = location_from_token(
        sm, file_name_to_uri(sm.getFileName(token.location()), fallback_uri), token);
    return loc;
}

static std::optional<Location> find_macro_definition(const slang::syntax::SyntaxTree& tree,
                                                     const std::string& uri,
                                                     const std::string& name) {
    const auto& sm = tree.sourceManager();
    for (const auto* macro : tree.getDefinedMacros()) {
        if (macro && macro->name.valueText() == name)
            return location_from_token_actual_uri(sm, uri, macro->name);
    }
    return std::nullopt;
}

static std::optional<Location> find_macro_definition_in_file(const std::filesystem::path& path,
                                                             const std::string& name) {
    slang::SourceManager sm;
    auto tree_or_error = slang::syntax::SyntaxTree::fromFile(path.string(), sm);
    if (!tree_or_error)
        return std::nullopt;
    return find_macro_definition(**tree_or_error, path_to_uri(path), name);
}

static std::optional<Location>
find_subroutine_argument_definition(const slang::syntax::SyntaxTree& tree, const std::string& uri,
                                    const std::string& subroutine_name,
                                    const std::string& argument_name) {
    struct Visitor : public slang::syntax::SyntaxVisitor<Visitor> {
        const slang::SourceManager& sm;
        const std::string& uri;
        const std::string& subroutine_name;
        const std::string& argument_name;
        std::optional<Location> result;

        Visitor(const slang::SourceManager& sm, const std::string& uri,
                const std::string& subroutine_name, const std::string& argument_name)
            : sm(sm), uri(uri), subroutine_name(subroutine_name), argument_name(argument_name) {}

        void handle(const slang::syntax::FunctionDeclarationSyntax& node) {
            const auto* identifier =
                node.prototype->name->as_if<slang::syntax::IdentifierNameSyntax>();
            if (!identifier || identifier->identifier.valueText() != subroutine_name)
                return;

            if (!node.prototype->portList)
                return;

            for (const auto* port_base : node.prototype->portList->ports) {
                const auto* port =
                    port_base ? port_base->as_if<slang::syntax::FunctionPortSyntax>() : nullptr;
                if (!port)
                    continue;
                const auto& token = port->declarator->name;
                if (token.valueText() == argument_name) {
                    result = location_from_token_actual_uri(sm, uri, token);
                    return;
                }
            }
        }
    };

    Visitor visitor(tree.sourceManager(), uri, subroutine_name, argument_name);
    tree.root().visit(visitor);
    return visitor.result;
}

static std::optional<Location>
find_subroutine_argument_definition_in_file(const std::filesystem::path& path,
                                            const std::string& subroutine_name,
                                            const std::string& argument_name) {
    slang::SourceManager sm;
    auto tree_or_error = slang::syntax::SyntaxTree::fromFile(path.string(), sm);
    if (!tree_or_error)
        return std::nullopt;
    return find_subroutine_argument_definition(**tree_or_error, path_to_uri(path), subroutine_name,
                                               argument_name);
}

struct GenericDefinitionVisitor : public slang::syntax::SyntaxVisitor<GenericDefinitionVisitor> {
    const slang::SourceManager& sm;
    const std::string& uri;
    const std::string& name;
    const std::string& preferred_module;
    std::string current_module;
    std::optional<Location> first_result;
    std::optional<Location> scoped_result;

    GenericDefinitionVisitor(const slang::SourceManager& sm, const std::string& uri,
                             const std::string& name, const std::string& preferred_module)
        : sm(sm), uri(uri), name(name), preferred_module(preferred_module) {}

    std::optional<Location> result() const { return scoped_result ? scoped_result : first_result; }

    void maybe_set(slang::parsing::Token token, bool scope_sensitive = true) {
        if (!token || token.valueText() != name)
            return;

        auto loc = location_from_token_actual_uri(sm, uri, token);
        if (!first_result)
            first_result = loc;
        if (scope_sensitive && !preferred_module.empty() && current_module == preferred_module &&
            !scoped_result)
            scoped_result = loc;
    }

    void handle(const slang::syntax::ModuleDeclarationSyntax& node) {
        maybe_set(node.header->name, false);

        auto previous_module = current_module;
        current_module = std::string(node.header->name.valueText());
        visitDefault(node);
        current_module = std::move(previous_module);
    }

    void handle(const slang::syntax::ImplicitAnsiPortSyntax& node) {
        maybe_set(node.declarator->name);
    }

    void handle(const slang::syntax::ExplicitAnsiPortSyntax& node) { maybe_set(node.name); }

    void handle(const slang::syntax::DeclaratorSyntax& node) { maybe_set(node.name); }

    void handle(const slang::syntax::FunctionDeclarationSyntax& node) {
        if (const auto* identifier =
                node.prototype->name->as_if<slang::syntax::IdentifierNameSyntax>())
            maybe_set(identifier->identifier);
        visitDefault(node);
    }

    void handle(const slang::syntax::TypedefDeclarationSyntax& node) { maybe_set(node.name); }
};

static std::optional<Location> find_generic_definition(const slang::syntax::SyntaxTree& tree,
                                                       const std::string& uri,
                                                       const std::string& name,
                                                       const std::string& preferred_module) {
    GenericDefinitionVisitor visitor(tree.sourceManager(), uri, name, preferred_module);
    tree.root().visit(visitor);
    return visitor.result();
}

static std::optional<Location>
find_generic_definition_in_file(const std::filesystem::path& path, const std::string& name,
                                const std::string& preferred_module) {
    slang::SourceManager sm;
    auto tree_or_error = slang::syntax::SyntaxTree::fromFile(path.string(), sm);
    if (!tree_or_error)
        return std::nullopt;
    return find_generic_definition(**tree_or_error, path_to_uri(path), name, preferred_module);
}

static slang::SourceRange visible_range_for_token(const slang::SourceManager& sm,
                                                  const slang::parsing::Token& token) {
    if (sm.isMacroLoc(token.location()))
        return sm.getExpansionRange(token.location());
    return token.range();
}

static bool contains_position(const slang::SourceManager& sm, slang::SourceRange range, int line,
                              int col) {
    if (!range.start().valid() || !range.end().valid())
        return false;

    const int start_line = to_lsp_line((int)sm.getLineNumber(range.start()));
    const int start_col = (int)sm.getColumnNumber(range.start()) - 1;
    const int end_line = to_lsp_line((int)sm.getLineNumber(range.end()));
    const int end_col = (int)sm.getColumnNumber(range.end()) - 1;

    if (line < start_line || line > end_line)
        return false;
    if (line == start_line && col < start_col)
        return false;
    if (line == end_line && col >= end_col)
        return false;
    return true;
}

static bool token_contains_position(const slang::SourceManager& sm,
                                    const slang::parsing::Token& token, int line, int col) {
    return token && token.location().valid() &&
           contains_position(sm, visible_range_for_token(sm, token), line, col);
}

enum class DefinitionTargetKind {
    None,
    Instance,
    NamedPort,
    NamedArgument,
    Macro,
    Generic,
};

struct DefinitionTarget {
    DefinitionTargetKind kind{DefinitionTargetKind::None};
    std::string name;
    std::string module_name;
    std::string subroutine_name;
    std::string scope_module;
};

struct DefinitionTargetVisitor : public slang::syntax::SyntaxVisitor<DefinitionTargetVisitor> {
    const slang::SourceManager& sm;
    int line;
    int col;
    DefinitionTarget target;
    std::string current_module;

    DefinitionTargetVisitor(const slang::SourceManager& sm, int line, int col)
        : sm(sm), line(line), col(col) {}

    bool found() const { return target.kind != DefinitionTargetKind::None; }

    void handle(const slang::syntax::ModuleDeclarationSyntax& node) {
        if (token_contains_position(sm, node.header->name, line, col)) {
            target.kind = DefinitionTargetKind::Generic;
            target.name = std::string(node.header->name.valueText());
            target.scope_module = current_module;
            return;
        }

        auto previous_module = current_module;
        current_module = std::string(node.header->name.valueText());
        visitDefault(node);
        current_module = std::move(previous_module);
    }

    void handle(const slang::syntax::HierarchyInstantiationSyntax& node) {
        if (token_contains_position(sm, node.type, line, col)) {
            target.kind = DefinitionTargetKind::Generic;
            target.name = std::string(node.type.valueText());
            target.scope_module = current_module;
            return;
        }

        const std::string module_name(node.type.valueText());
        for (const auto* instance : node.instances) {
            if (!instance)
                continue;
            if (instance->decl && token_contains_position(sm, instance->decl->name, line, col)) {
                target.kind = DefinitionTargetKind::Instance;
                target.name = std::string(instance->decl->name.valueText());
                target.module_name = module_name;
                target.scope_module = current_module;
                return;
            }

            for (const auto* connection : instance->connections) {
                if (!connection)
                    continue;
                const auto* named = connection->as_if<slang::syntax::NamedPortConnectionSyntax>();
                if (named && token_contains_position(sm, named->name, line, col)) {
                    target.kind = DefinitionTargetKind::NamedPort;
                    target.name = std::string(named->name.valueText());
                    target.module_name = module_name;
                    target.scope_module = current_module;
                    return;
                }
            }
        }
        visitDefault(node);
    }

    void handle(const slang::syntax::InvocationExpressionSyntax& node) {
        const auto* callee = node.left->as_if<slang::syntax::IdentifierNameSyntax>();
        if (!callee || !node.arguments) {
            visitDefault(node);
            return;
        }

        const std::string subroutine_name(callee->identifier.valueText());
        for (const auto* argument : node.arguments->parameters) {
            if (!argument)
                continue;
            const auto* named = argument->as_if<slang::syntax::NamedArgumentSyntax>();
            if (!named)
                continue;
            if (token_contains_position(sm, named->name, line, col)) {
                target.kind = DefinitionTargetKind::NamedArgument;
                target.name = std::string(named->name.valueText());
                target.subroutine_name = subroutine_name;
                target.scope_module = current_module;
                return;
            }
        }

        visitDefault(node);
    }

    void handle(const slang::syntax::NamedTypeSyntax& node) {
        const auto* identifier = node.name->as_if<slang::syntax::IdentifierNameSyntax>();
        if (identifier && token_contains_position(sm, identifier->identifier, line, col)) {
            target.kind = DefinitionTargetKind::Generic;
            target.name = std::string(identifier->identifier.valueText());
            target.scope_module = current_module;
            return;
        }
        visitDefault(node);
    }

    void visitToken(slang::parsing::Token token) {
        if (found() || !token || !token.location().valid())
            return;

        if (sm.isMacroLoc(token.location())) {
            if (!contains_position(sm, sm.getExpansionRange(token.location()), line, col))
                return;

            auto macro_name = sm.getMacroName(token.location());
            if (!macro_name.empty()) {
                target.kind = DefinitionTargetKind::Macro;
                target.name = std::string(macro_name);
                target.scope_module = current_module;
            }
            return;
        }

        if (token.kind == slang::parsing::TokenKind::Identifier &&
            token_contains_position(sm, token, line, col)) {
            target.kind = DefinitionTargetKind::Generic;
            target.name = std::string(token.valueText());
            target.scope_module = current_module;
        }
    }
};

static DefinitionTarget definition_target_at(const slang::syntax::SyntaxTree& tree, int line,
                                             int col) {
    DefinitionTargetVisitor visitor(tree.sourceManager(), line, col);
    tree.root().visit(visitor);
    return visitor.target;
}

std::optional<SymbolInfo> Analyzer::symbol_at(const std::string& uri, int line, int col) const {
    auto state = get_state(uri);
    if (!state || !state->tree)
        return std::nullopt;

    auto ident = extract_ident(state->text, line, col);
    if (ident.empty())
        return std::nullopt;

    auto idx = SyntaxIndex::build(*state->tree, state->text);

    for (const auto& m : idx.modules) {
        if (m.name == ident)
            return SymbolInfo{ident, "module", "", m.line, 0};
        for (const auto& p : m.ports)
            if (p.name == ident)
                return SymbolInfo{ident, "port", p.direction, p.line, 0};
    }
    for (const auto& inst : idx.instances)
        if (inst.instance_name == ident)
            return SymbolInfo{ident, "instance", inst.module_name, inst.line, 0};

    return SymbolInfo{ident, "unknown", "", line, col};
}

std::optional<Location> Analyzer::definition_of(const std::string& uri, int line, int col) const {
    auto state = get_state(uri);
    if (!state || !state->tree)
        return std::nullopt;

    auto target = definition_target_at(*state->tree, line, col);
    if (target.kind == DefinitionTargetKind::None || target.name.empty())
        return std::nullopt;

    auto idx = SyntaxIndex::build(*state->tree);
    std::vector<std::string> extra_files;
    {
        std::lock_guard<std::mutex> lock(map_mutex_);
        extra_files = extra_files_;
    }

    if (target.kind == DefinitionTargetKind::Macro) {
        if (auto loc = find_macro_definition(*state->tree, uri, target.name))
            return loc;
        for (const auto& path : extra_files) {
            if (auto loc = find_macro_definition_in_file(path, target.name))
                return loc;
        }
        return std::nullopt;
    }

    if (target.kind == DefinitionTargetKind::NamedPort) {
        if (auto loc = find_port_definition(idx, uri, target.module_name, target.name))
            return loc;

        for (const auto& path : extra_files) {
            auto indexed = build_index_for_file(path);
            if (!indexed)
                continue;
            if (auto loc = find_port_definition(indexed->index, indexed->uri, target.module_name,
                                                target.name))
                return loc;
        }
        return std::nullopt;
    }

    if (target.kind == DefinitionTargetKind::NamedArgument) {
        if (auto loc = find_subroutine_argument_definition(*state->tree, uri,
                                                           target.subroutine_name, target.name))
            return loc;

        for (const auto& path : extra_files) {
            if (auto loc = find_subroutine_argument_definition_in_file(path, target.subroutine_name,
                                                                       target.name))
                return loc;
        }
        return std::nullopt;
    }

    if (target.kind == DefinitionTargetKind::Instance) {
        if (auto loc = find_module_definition(idx, uri, target.module_name))
            return loc;
        for (const auto& path : extra_files) {
            auto indexed = build_index_for_file(path);
            if (!indexed)
                continue;
            if (auto loc = find_module_definition(indexed->index, indexed->uri, target.module_name))
                return loc;
        }
        return std::nullopt;
    }

    if (auto loc = find_generic_definition(*state->tree, uri, target.name, target.scope_module))
        return loc;
    for (const auto& path : extra_files) {
        if (auto loc = find_generic_definition_in_file(path, target.name, target.scope_module))
            return loc;
    }

    return std::nullopt;
}

std::vector<std::pair<int, int>> Analyzer::find_occurrences(const std::string& uri,
                                                            const std::string& name) const {
    auto state = get_state(uri);
    if (!state || name.empty())
        return {};

    std::string_view src = state->text;
    auto is_id = [](char c) { return std::isalnum((unsigned char)c) || c == '_' || c == '$'; };

    // Build line-start offsets
    std::vector<size_t> ls;
    ls.push_back(0);
    for (size_t i = 0; i < src.size(); ++i)
        if (src[i] == '\n')
            ls.push_back(i + 1);

    std::vector<std::pair<int, int>> result;
    size_t pos = 0;
    while (pos < src.size()) {
        auto found = src.find(name, pos);
        if (found == std::string_view::npos)
            break;

        bool before_ok = (found == 0) || !is_id(src[found - 1]);
        bool after_ok = (found + name.size() >= src.size()) || !is_id(src[found + name.size()]);

        if (before_ok && after_ok) {
            auto it = std::upper_bound(ls.begin(), ls.end(), found);
            int line = (int)(it - ls.begin()) - 1; // 0-based
            int col = (int)(found - ls[(size_t)line]);
            result.push_back({line, col});
        }
        pos = found + 1;
    }
    return result;
}

void Analyzer::set_extra_files(const std::vector<std::string>& paths) {
    std::lock_guard<std::mutex> lock(map_mutex_);
    extra_files_ = paths;
}

void Analyzer::refresh_if_stale(const std::string& /*uri*/) {
    // TODO: implement mtime check in US-011
}
