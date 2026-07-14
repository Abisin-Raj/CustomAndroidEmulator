#pragma once
#include "kernel_analysis/elf_loader.h"
#include <string>
#include <vector>
#include <cstdint>

namespace kernel_analysis {

class SymbolResolver {
public:
    SymbolResolver() = default;
    ~SymbolResolver() = default;

    // Load symbols from vmlinux ELF64 binary
    bool loadElfSymbols(const std::string& elfPath);

    // Load symbols from a System.map text file
    bool loadSystemMapSymbols(const std::string& mapPath);

    // Resolve a virtual address to function_name+offset
    std::string resolveAddress(uint64_t address);

    // Look up a symbol by exact name and return its virtual address.
    // Returns 0 if the symbol is not found.
    uint64_t findSymbolAddress(const std::string& name) const;

    // Get number of loaded symbols
    size_t getSymbolCount() const { return m_symbols.size(); }

    // Clear all symbols
    void clear() { m_symbols.clear(); }

private:
    std::vector<SymbolEntry> m_symbols;

    // Sort symbols by address and remove duplicates
    void finalizeSymbols();
};

} // namespace kernel_analysis
