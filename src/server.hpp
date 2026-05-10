#pragma once
#include "analyzer.hpp"
#include "config.hpp"
#include <memory>
#include <string>

// Forward declarations to avoid pulling in LspCpp headers here
class RemoteEndPoint;

class LazyVerilogServer {
public:
    explicit LazyVerilogServer();
    ~LazyVerilogServer();

    /// Block until server exits (stdin closed or exit notification received).
    void run();

private:
    void register_handlers();

    Config   config_;
    Analyzer analyzer_;

    struct Impl;
    std::unique_ptr<Impl> impl_;
};
