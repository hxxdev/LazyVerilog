#include "definition.hpp"

std::optional<lsLocation> provide_definition(const Analyzer& analyzer,
                                             const lsTextDocumentPositionParams& params) {
    const auto& uri = params.textDocument.uri.raw_uri_;
    const int line = params.position.line;
    const int character = params.position.character;

    auto src_range = analyzer.definition_of(uri, line, character);
    if (!src_range)
        return std::nullopt;

    lsLocation result;
    result.uri.raw_uri_ = src_range->uri.empty() ? uri : src_range->uri;
    result.range.start = lsPosition(src_range->line, src_range->col);
    result.range.end = lsPosition(src_range->end_line, src_range->end_col);
    return result;
}
