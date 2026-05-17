#pragma once
#include "config.hpp"
#include <memory>
#include <stdexcept>
#include <string>

namespace slang::syntax { class SyntaxTree; }

class SafeModeError : public std::runtime_error {
  public:
    explicit SafeModeError(const std::string& message) : std::runtime_error(message) {}
};

/// Format SystemVerilog source text.
/// When @p tree is provided (e.g. from Analyzer), it is used for CST-based
/// module port expansion and line classification without a redundant re-parse.
/// Pass nullptr (default) to let format_source build its own tree.
std::string format_source(
    const std::string& source,
    const FormatOptions& opts,
    std::shared_ptr<const slang::syntax::SyntaxTree> tree = nullptr);
