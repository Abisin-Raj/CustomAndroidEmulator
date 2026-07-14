#pragma once

#include <string>

class KernelCommandLine {
public:
    KernelCommandLine() = default;
    ~KernelCommandLine() = default;

    // Takes an existing commandline from boot.img and adds/forces early tracing arguments
    static std::string build(const std::string& baseCmdline, bool enforceLogs = true);
};
