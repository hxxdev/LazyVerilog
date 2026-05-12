#pragma once

#include "LibLsp/lsp/location_type.h"
#include "LibLsp/lsp/lsTextDocumentPositionParams.h"
#include "analyzer.hpp"
#include <optional>

std::optional<lsLocation> provide_definition(const Analyzer& analyzer,
                                             const lsTextDocumentPositionParams& params);
