#pragma once
#include "config.hpp"
#include <string>

/// Format SystemVerilog source text.
/// Ported from lazyverilogpy/formatter.py (Verible token-annotator algorithm).
/// Returns the formatted source. Handles // verilog_format: off/on regions.
std::string format_source(const std::string& source, const FormatOptions& opts);
