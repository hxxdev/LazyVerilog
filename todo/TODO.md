### Implement following options in lazyverilog.toml:
#### Refer to /home/hxxdev/.local/share/nvim/site/pack/core/opt/lazyverilogpy

- lint.enable
- lint.module.enable
- lint.module.severity
- lint.module.one_module_per_file
- lint.module.stale_autoinst_diagnostic
- lint.statement.enable
- lint.statement.severity
- lint.statement.no_raw_always
- lint.statement.blocking_nonblocking_assignments
- lint.function.enable
- lint.function.severity
- lint.function.function_call_style
- rtltree.show_instance_name
- rtltree.show_file
- autowire.group_by_instance
- autowire.sort_by_name
- lint.naming.interface_pattern
- lint.naming.struct_pattern
- lint.naming.union_pattern
- lint.naming.enum_pattern
- lint.naming.parameter_pattern
- lint.naming.localparam_pattern
- lint.naming.check_module_filename
- lint.naming.check_package_filename
- format.tab_align
- format.align_punctuation
- format.statement.align_adaptive

### Replace option in source code
autoarg.indent_size -> format.indent_size

### Remove from lazyverilog.toml and source code:
- format.function_call.trailing_comma
- lint.design.enable
- lint.design.severity
- lint.design.max_file_size
- autoinst.indent_size
- format.indent_size
- autoinst.align_ports


### Add to lazyverilog.toml:
- format.trailing_newline

### Change source code: move these options to right sections which are written in lazyverilog.toml.
- lint.case_missing_default
- lint.functions_automatic
- lint.explicit_function_lifetime
- lint.explicit_task_lifetime
- lint.module_instantiation_style
- lint.latch_inference_detection
- lint.explicit_begin
- lint.register_naming
