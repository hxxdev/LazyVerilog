#include "syntax_index.hpp"
#include <algorithm>
#include <slang/syntax/SyntaxTree.h>
#include <slang/syntax/AllSyntax.h>

using namespace slang;
using namespace slang::syntax;

static std::string tok_str(const slang::parsing::Token& t) {
    return std::string(t.rawText());
}

// Compute 1-based line number by searching for token rawText in source text.
// search_from is updated to just after the found position (for sequential scan).
static int tok_line(const slang::parsing::Token& t,
                    std::string_view src,
                    const std::vector<size_t>& line_starts,
                    size_t& search_from) {
    auto raw = t.rawText();
    if (raw.empty() || src.empty()) return 0;
    auto pos = src.find(raw, search_from);
    if (pos == std::string_view::npos)
        pos = src.find(raw, 0);  // fallback: search from beginning
    if (pos == std::string_view::npos) return 0;
    search_from = pos + raw.size();
    auto it = std::upper_bound(line_starts.begin(), line_starts.end(), pos);
    return (int)(it - line_starts.begin());
}

static void extract_ansi_ports(const AnsiPortListSyntax& pl,
                                std::vector<PortEntry>& ports,
                                std::string_view src,
                                const std::vector<size_t>& ls,
                                size_t& sf) {
    for (size_t i = 0; i < pl.ports.size(); ++i) {
        const auto* mp = pl.ports[i];
        if (!mp) continue;

        if (auto* p = mp->as_if<ImplicitAnsiPortSyntax>()) {
            PortEntry pe;
            pe.name = tok_str(p->declarator->name);
            pe.line = tok_line(p->declarator->name, src, ls, sf);
            if (auto* vh = p->header->as_if<VariablePortHeaderSyntax>())
                pe.direction = tok_str(vh->direction);
            else if (auto* nh = p->header->as_if<NetPortHeaderSyntax>())
                pe.direction = tok_str(nh->direction);
            ports.push_back(std::move(pe));
        } else if (auto* p = mp->as_if<ExplicitAnsiPortSyntax>()) {
            PortEntry pe;
            pe.name      = tok_str(p->name);
            pe.direction = tok_str(p->direction);
            pe.line      = tok_line(p->name, src, ls, sf);
            ports.push_back(std::move(pe));
        }
    }
}

static void extract_instances(const SyntaxList<MemberSyntax>& members,
                               std::vector<InstanceEntry>& out,
                               std::string_view src,
                               const std::vector<size_t>& ls,
                               size_t& sf) {
    for (size_t i = 0; i < members.size(); ++i) {
        const auto* m = members[i];
        if (!m) continue;
        if (auto* hi = m->as_if<HierarchyInstantiationSyntax>()) {
            std::string mod = tok_str(hi->type);
            for (size_t j = 0; j < hi->instances.size(); ++j) {
                const auto* inst = hi->instances[j];
                if (!inst) continue;
                InstanceEntry ie;
                ie.module_name = mod;
                if (inst->decl) {
                    ie.instance_name = tok_str(inst->decl->name);
                    ie.line          = tok_line(inst->decl->name, src, ls, sf);
                }
                // Extract named port connections
                for (size_t k = 0; k < inst->connections.size(); ++k) {
                    const auto* pc = inst->connections[k];
                    if (!pc) continue;
                    if (auto* npc = pc->as_if<NamedPortConnectionSyntax>()) {
                        NamedPortConn c;
                        c.port_name = tok_str(npc->name);
                        int ln = tok_line(npc->name, src, ls, sf);
                        c.line = ln;
                        // compute col from line start
                        if (ln > 0 && ln <= (int)ls.size()) {
                            auto raw = npc->name.rawText();
                            auto pos = src.find(raw, (ln >= 1 ? ls[ln-1] : 0));
                            if (pos != std::string_view::npos)
                                c.col = (int)(pos - ls[ln > 0 ? ln-1 : 0]);
                        }
                        ie.connections.push_back(std::move(c));
                    }
                }
                out.push_back(std::move(ie));
            }
        }
    }
}

static void process_module(const ModuleDeclarationSyntax& mod,
                            SyntaxIndex& idx,
                            std::string_view src,
                            const std::vector<size_t>& ls,
                            size_t& sf) {
    ModuleEntry me;
    me.name = tok_str(mod.header->name);
    me.line = tok_line(mod.header->name, src, ls, sf);

    if (mod.header->ports) {
        if (auto* apl = mod.header->ports->as_if<AnsiPortListSyntax>())
            extract_ansi_ports(*apl, me.ports, src, ls, sf);
    }

    extract_instances(mod.members, idx.instances, src, ls, sf);
    idx.modules.push_back(std::move(me));
}

static std::vector<size_t> build_line_starts(std::string_view src) {
    std::vector<size_t> ls;
    ls.push_back(0);
    for (size_t i = 0; i < src.size(); ++i)
        if (src[i] == '\n') ls.push_back(i + 1);
    return ls;
}

SyntaxIndex SyntaxIndex::build(const slang::syntax::SyntaxTree& tree,
                                std::string_view source) {
    SyntaxIndex idx;
    auto ls = build_line_starts(source);
    size_t sf = 0;  // sequential search cursor

    const auto& root = tree.root();

    if (const auto* cu = root.as_if<CompilationUnitSyntax>()) {
        for (size_t i = 0; i < cu->members.size(); ++i) {
            const auto* m = cu->members[i];
            if (!m) continue;
            if (auto* mod = m->as_if<ModuleDeclarationSyntax>())
                process_module(*mod, idx, source, ls, sf);
        }
    } else if (auto* mod = root.as_if<ModuleDeclarationSyntax>()) {
        process_module(*mod, idx, source, ls, sf);
    }

    return idx;
}
