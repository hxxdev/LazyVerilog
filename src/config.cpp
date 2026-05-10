#include "config.hpp"
#include <toml++/toml.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>

Config load_config(const std::filesystem::path& root) {
    Config cfg{};
    auto toml_path = root / "lazyverilog.toml";
    if (!std::filesystem::exists(toml_path)) {
        return cfg;
    }
    try {
        auto tbl = toml::parse_file(toml_path.string());

        // [design]
        if (auto d = tbl["design"].as_table()) {
            if (auto v = (*d)["vcode"].value<std::string>()) cfg.design.vcode = *v;
            if (auto arr = (*d)["define"].as_array()) {
                arr->for_each([&](auto&& el) {
                    if constexpr (toml::is_string<std::remove_cvref_t<decltype(el)>>) {
                        cfg.design.define.push_back(*el);
                    }
                });
            }
        }

        // [perf]
        if (auto p = tbl["perf"].as_table()) {
            if (auto v = (*p)["background_compilation"].value<bool>()) cfg.perf.background_compilation = *v;
            if (auto v = (*p)["nice_value"].value<int64_t>()) cfg.perf.nice_value = static_cast<int>(*v);
            if (auto v = (*p)["log_timing"].value<bool>()) cfg.perf.log_timing = *v;
        }

        // [inlay_hint]
        if (auto ih = tbl["inlay_hint"].as_table()) {
            if (auto v = (*ih)["enable"].value<bool>()) cfg.inlay_hint.enable = *v;
        }

        // [format]
        if (auto f = tbl["format"].as_table()) {
            if (auto v = (*f)["indent_module_body"].value<bool>()) cfg.format.indent_module_body = *v;
            if (auto v = (*f)["align_port_declarations"].value<bool>()) cfg.format.align_port_declarations = *v;
            if (auto v = (*f)["indent_width"].value<int64_t>()) cfg.format.indent_width = static_cast<int>(*v);
            if (auto v = (*f)["use_spaces"].value<bool>()) cfg.format.use_spaces = *v;
            if (auto v = (*f)["trailing_newline"].value<bool>()) cfg.format.trailing_newline = *v;
        }

        // [lint.*] — each subtable key is a rule name
        if (auto lint = tbl["lint"].as_table()) {
            lint->for_each([&](const toml::key& k, auto&& val) {
                // Only process boolean values; skip tables, arrays, etc.
                if constexpr (toml::is_boolean<std::remove_cvref_t<decltype(val)>>) {
                    bool enabled = *val;
                    auto key = std::string(k.str());
                    if (key == "case_missing_default") cfg.lint.case_missing_default = enabled;
                    else if (key == "functions_automatic") cfg.lint.functions_automatic = enabled;
                    else if (key == "explicit_function_lifetime") cfg.lint.explicit_function_lifetime = enabled;
                    else if (key == "explicit_task_lifetime") cfg.lint.explicit_task_lifetime = enabled;
                    else if (key == "module_instantiation_style") cfg.lint.module_instantiation_style = enabled;
                    else if (key == "latch_inference_detection") cfg.lint.latch_inference_detection = enabled;
                    else if (key == "explicit_begin") cfg.lint.explicit_begin = enabled;
                    else if (key == "register_naming") cfg.lint.register_naming = enabled;
                    // unknown lint keys silently ignored
                }
            });
        }

        // [autoinst]
        if (auto ai = tbl["autoinst"].as_table()) {
            if (auto v = (*ai)["align_ports"].value<bool>()) cfg.autoinst.align_ports = *v;
        }

        // [autoarg]
        if (auto aa = tbl["autoarg"].as_table()) {
            if (auto v = (*aa)["autoarg_on_save"].value<bool>()) cfg.autoarg.autoarg_on_save = *v;
        }

        // Unknown top-level keys silently ignored (toml++ doesn't error on them)

    } catch (const toml::parse_error& e) {
        // Parse error: return defaults (log to stderr)
        std::cerr << "[lazyverilog] config parse error: " << e.what() << "\n";
    } catch (...) {
        std::cerr << "[lazyverilog] config load error\n";
    }
    return cfg;
}
