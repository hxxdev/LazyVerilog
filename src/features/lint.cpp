#include "lint.hpp"
#include <slang/syntax/AllSyntax.h>
#include <slang/syntax/SyntaxVisitor.h>
#include <slang/text/SourceManager.h>

using namespace slang;
using namespace slang::syntax;

static ParseDiagInfo make_diag(SourceManager& sm, SourceLocation loc,
                               int sev, std::string msg) {
    ParseDiagInfo d;
    d.severity = sev;
    d.message  = std::move(msg);
    if (loc.valid()) {
        size_t ln = sm.getLineNumber(loc);
        size_t co = sm.getColumnNumber(loc);
        d.line = ln > 0 ? (int)ln - 1 : 0;
        d.col  = co > 0 ? (int)co - 1 : 0;
    }
    return d;
}

/// Recursively check if a syntax subtree contains an if-without-else (latch risk).
static bool has_latch_risk(const SyntaxNode& node) {
    if (const auto* cond = node.as_if<ConditionalStatementSyntax>())
        if (!cond->elseClause)
            return true;
    for (uint32_t i = 0; i < node.getChildCount(); ++i) {
        const SyntaxNode* child = node.childNode(i);
        if (child && has_latch_risk(*child))
            return true;
    }
    return false;
}

struct LintVisitor : public SyntaxVisitor<LintVisitor> {
    const LintConfig&          cfg;
    SourceManager&             sm;
    std::vector<ParseDiagInfo> diags;

    LintVisitor(const LintConfig& c, SourceManager& s) : cfg(c), sm(s) {}

    // ── case_missing_default ──────────────────────────────────────────────
    void handle(const CaseStatementSyntax& node) {
        if (cfg.case_missing_default && !node.uniqueOrPriority.valid()) {
            bool has_default = false;
            for (uint32_t i = 0; i < node.items.size() && !has_default; ++i)
                if (node.items[i]->as_if<DefaultCaseItemSyntax>())
                    has_default = true;
            if (!has_default)
                diags.push_back(make_diag(sm, node.caseKeyword.location(), 2,
                    "[statement] case statement missing default item"));
        }
        visitDefault(node);
    }

    // ── functions_automatic / explicit_function_lifetime / explicit_task_lifetime
    void handle(const FunctionDeclarationSyntax& node) {
        bool is_task   = (node.kind == SyntaxKind::TaskDeclaration);
        auto& proto    = *node.prototype;
        bool  has_life = proto.lifetime.valid() && !proto.lifetime.rawText().empty();

        if (!is_task) {
            if (cfg.functions_automatic) {
                bool is_auto = has_life && proto.lifetime.rawText() == "automatic";
                if (!is_auto)
                    diags.push_back(make_diag(sm, proto.keyword.location(), 2,
                        "[function] function declaration should use 'automatic' lifetime"));
            } else if (cfg.explicit_function_lifetime && !has_life) {
                diags.push_back(make_diag(sm, proto.keyword.location(), 2,
                    "[function] function declaration missing explicit lifetime (automatic/static)"));
            }
        } else if (cfg.explicit_task_lifetime && !has_life) {
            diags.push_back(make_diag(sm, proto.keyword.location(), 2,
                "[function] task declaration missing explicit lifetime (automatic/static)"));
        }
        visitDefault(node);
    }

    // ── latch_inference_detection ─────────────────────────────────────────
    void handle(const ProceduralBlockSyntax& node) {
        if (cfg.latch_inference_detection && node.kind == SyntaxKind::AlwaysCombBlock) {
            if (has_latch_risk(*node.statement))
                diags.push_back(make_diag(sm, node.keyword.location(), 2,
                    "[statement] always_comb block may infer a latch (incomplete if)"));
        }
        visitDefault(node);
    }

    // ── module_instantiation_style (bool: true → require named connections) ─
    void handle(const HierarchyInstantiationSyntax& node) {
        if (cfg.module_instantiation_style) {
            for (uint32_t i = 0; i < node.instances.size(); ++i) {
                const auto* inst = node.instances[i];
                if (!inst) continue;
                bool positional = false;
                for (uint32_t j = 0; j < inst->connections.size() && !positional; ++j) {
                    if (const auto* conn = inst->connections[j])
                        if (conn->as_if<OrderedPortConnectionSyntax>())
                            positional = true;
                }
                if (positional)
                    diags.push_back(make_diag(sm, node.type.location(), 2,
                        "[module] instance uses positional port connections; named connections preferred"));
            }
        }
        visitDefault(node);
    }
};

std::vector<ParseDiagInfo> run_lint(const DocumentState& state, const LintConfig& config) {
    if (!state.tree)
        return {};
    // Fast path: skip if no rules are enabled
    if (!config.case_missing_default && !config.functions_automatic &&
        !config.explicit_function_lifetime && !config.explicit_task_lifetime &&
        !config.module_instantiation_style && !config.latch_inference_detection &&
        !config.explicit_begin && !config.register_naming)
        return {};

    auto& sm = state.tree->sourceManager();
    LintVisitor v(config, sm);
    state.tree->root().visit(v);
    return std::move(v.diags);
}
