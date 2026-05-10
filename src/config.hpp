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

struct FormatOptions {
    // Mirror Python's FormatOptions dataclass fields
    bool indent_module_body{true};
    bool align_port_declarations{false};
    int  indent_width{4};
    bool use_spaces{true};
    bool trailing_newline{true};
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
