#pragma once
#include <filesystem>
#include <string>
#include <vector>

struct DesignConfig {
    std::string vcode;           // .f filelist path
    std::vector<std::string> define; // preprocessor defines
};

struct PerfConfig {
    bool background_compilation{false};
    int  nice_value{10};
    bool log_timing{false};
};

struct InlayHintConfig {
    bool enable{true};
};

struct StatementOptions {
    bool align{false};
    bool align_adaptive{false};
    int  lhs_min_width{1};
    bool wrap_end_else_clauses{false};
};

struct PortDeclarationOptions {
    bool align{true};
    int  section1_min_width{10};
    int  section2_min_width{20};
    int  section3_min_width{20};
    int  section4_min_width{30};
    int  section5_min_width{30};
};

struct VarDeclarationOptions {
    bool align{false};
    int  section1_min_width{0};
    int  section2_min_width{30};
    int  section3_min_width{30};
    int  section4_min_width{0};
};

struct InstanceOptions {
    bool align{false};
    int  port_indent_level{1};
    int  instance_port_name_width{1};
    int  instance_port_between_paren_width{0};
    bool align_instance_port_adaptive{false};
};

struct PortOptions {
    bool non_ansi_port_per_line_enabled{false};
    int  non_ansi_port_per_line{1};
    bool non_ansi_port_max_line_length_enabled{false};
    int  non_ansi_port_max_line_length{80};
};

struct FormatOptions {
    // Mirrors Python FormatOptions (formatter.py)
    int  indent_size{2};                  // Python: indent_size (was indent_width)
    bool compact_indexing_and_selections{true};
    std::string keyword_case{"preserve"}; // "preserve" | "lower" | "upper"
    int  blank_lines_between_items{1};
    int  default_indent_level_inside_module_block{1};
    bool tab_align{false};
    bool align_punctuation{false};
    bool enable_format_on_save{false};
    bool safe_mode{false};
    bool trailing_newline{true};

    StatementOptions       statement;
    PortDeclarationOptions port_declaration;
    VarDeclarationOptions  var_declaration;
    InstanceOptions        instance;
    PortOptions            port;
};

struct LintConfig {
    // All rules default disabled; activated via [lint.*] in lazyverilog.toml
    bool case_missing_default{false};
    bool functions_automatic{false};
    bool explicit_function_lifetime{false};
    bool explicit_task_lifetime{false};
    bool module_instantiation_style{false};
    bool latch_inference_detection{false};
    bool explicit_begin{false};
    bool register_naming{false};
};

struct AutoinstOptions {
    bool align_ports{false};
};

struct AutoargOptions {
    bool autoarg_on_save{false};
};

struct AutowireOptions {};

struct AutoFuncOptions {};

struct Config {
    DesignConfig    design;
    PerfConfig      perf;
    InlayHintConfig inlay_hint;
    FormatOptions   format;
    LintConfig      lint;
    AutoinstOptions autoinst;
    AutoargOptions  autoarg;
    AutowireOptions autowire;
    AutoFuncOptions autofunc;
};

/// Load lazyverilog.toml from root directory. Returns defaults if not found.
/// Unknown keys are silently ignored.
Config load_config(const std::filesystem::path& root);

/// Walk up from `start` (file or directory) looking for lazyverilog.toml.
/// Returns the directory containing it, or empty path if not found.
std::filesystem::path find_config_root(const std::filesystem::path& start);
