#include <catch2/catch_test_macros.hpp>
#include "config.hpp"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

static fs::path make_temp_toml(const std::string& content) {
    auto dir = fs::temp_directory_path() / "lv_test_config";
    fs::create_directories(dir);
    auto p = dir / "lazyverilog.toml";
    std::ofstream f(p);
    f << content;
    return dir;
}

TEST_CASE("config: missing file returns defaults", "[config]") {
    auto dir = fs::temp_directory_path() / "lv_no_such_dir_xyz123";
    fs::remove_all(dir);
    Config cfg = load_config(dir);
    CHECK(cfg.design.vcode.empty());
    CHECK(cfg.design.define.empty());
    CHECK(cfg.perf.background_compilation == false);
    CHECK(cfg.perf.nice_value == 10);
    CHECK(cfg.perf.log_timing == false);
    CHECK(cfg.inlay_hint.enable == true);
    CHECK(cfg.format.indent_module_body == true);
    CHECK(cfg.format.indent_width == 4);
    CHECK(cfg.format.use_spaces == true);
    CHECK(cfg.format.trailing_newline == true);
    CHECK(cfg.lint.case_missing_default == false);
    CHECK(cfg.lint.register_naming == false);
    CHECK(cfg.autoinst.align_ports == false);
    CHECK(cfg.autoarg.autoarg_on_save == false);
}

TEST_CASE("config: unknown TOML keys silently ignored", "[config]") {
    auto dir = make_temp_toml(R"(
[unknown_section]
some_key = "value"
another_key = 42

[design]
vcode = "test.f"
unknown_design_key = true
)");
    Config cfg;
    REQUIRE_NOTHROW(cfg = load_config(dir));
    CHECK(cfg.design.vcode == "test.f");
}

TEST_CASE("config: parse all sections correctly", "[config]") {
    auto dir = make_temp_toml(R"(
[design]
vcode = "my.f"
define = ["RTL_SIM", "FAST_MODEL"]

[perf]
background_compilation = true
nice_value = 15
log_timing = true

[inlay_hint]
enable = false

[format]
indent_module_body = false
align_port_declarations = true
indent_width = 2
use_spaces = false
trailing_newline = false

[lint]
case_missing_default = true
functions_automatic = true
explicit_function_lifetime = true
explicit_task_lifetime = true
module_instantiation_style = true
latch_inference_detection = true
explicit_begin = true
register_naming = true

[autoinst]
align_ports = true

[autoarg]
autoarg_on_save = true
)");
    Config cfg = load_config(dir);

    CHECK(cfg.design.vcode == "my.f");
    REQUIRE(cfg.design.define.size() == 2);
    CHECK(cfg.design.define[0] == "RTL_SIM");
    CHECK(cfg.design.define[1] == "FAST_MODEL");

    CHECK(cfg.perf.background_compilation == true);
    CHECK(cfg.perf.nice_value == 15);
    CHECK(cfg.perf.log_timing == true);

    CHECK(cfg.inlay_hint.enable == false);

    CHECK(cfg.format.indent_module_body == false);
    CHECK(cfg.format.align_port_declarations == true);
    CHECK(cfg.format.indent_width == 2);
    CHECK(cfg.format.use_spaces == false);
    CHECK(cfg.format.trailing_newline == false);

    CHECK(cfg.lint.case_missing_default == true);
    CHECK(cfg.lint.functions_automatic == true);
    CHECK(cfg.lint.explicit_function_lifetime == true);
    CHECK(cfg.lint.explicit_task_lifetime == true);
    CHECK(cfg.lint.module_instantiation_style == true);
    CHECK(cfg.lint.latch_inference_detection == true);
    CHECK(cfg.lint.explicit_begin == true);
    CHECK(cfg.lint.register_naming == true);

    CHECK(cfg.autoinst.align_ports == true);
    CHECK(cfg.autoarg.autoarg_on_save == true);
}

TEST_CASE("config: malformed TOML returns defaults", "[config]") {
    auto dir = make_temp_toml("this is not valid toml @@@ !!!");
    Config cfg;
    REQUIRE_NOTHROW(cfg = load_config(dir));
    // Defaults preserved on parse error
    CHECK(cfg.perf.background_compilation == false);
    CHECK(cfg.format.indent_width == 4);
}
