#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>

// Forward declarations from slang
namespace slang::syntax { class SyntaxTree; }
namespace slang::ast { class Compilation; }
#include <slang/diagnostics/Diagnostics.h>

/// Immutable snapshot of a single open document.
/// Handlers receive a shared_ptr<const DocumentState>; didChange atomically
/// swaps in a new instance. No per-document locking needed on the read path.
struct DocumentState {
    std::string uri;
    std::string text;
    std::shared_ptr<slang::syntax::SyntaxTree> tree;
    // Safe copy of parse diagnostics made at construction time (single-threaded).
    // Use this instead of tree->diagnostics() to avoid racing on slang's internal
    // SmallVector which can be corrupted by concurrent mimalloc TLS init.
    std::vector<slang::Diagnostic> parse_diagnostics;
    // Compilation is optional — only present when background_compilation=true
    std::optional<std::shared_ptr<slang::ast::Compilation>> compilation;
    std::string tree_filename{"buffer.sv"};
    bool compilation_dirty{false};

    DocumentState() = default;
    DocumentState(std::string uri, std::string text,
                  std::shared_ptr<slang::syntax::SyntaxTree> tree)
        : uri(std::move(uri)), text(std::move(text)), tree(std::move(tree)) {}
};
