#include "kernel_analysis/symbol_resolver.h"
#include "core/logger.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>

namespace kernel_analysis {

bool SymbolResolver::loadElfSymbols(const std::string& elfPath) {
    ElfLoader loader;
    bool success = loader.loadSymbols(elfPath, m_symbols);
    if (success) {
        finalizeSymbols();
    }
    return success;
}

bool SymbolResolver::loadSystemMapSymbols(const std::string& mapPath) {
    std::ifstream fs(mapPath);
    if (!fs) {
        LOG_PARSER_ERROR("SymbolResolver: Failed to open System.map file: {}", mapPath);
        return false;
    }

    std::string line;
    size_t count = 0;
    while (std::getline(fs, line)) {
        if (line.empty()) continue;
        
        // Strip trailing carriage return for Windows compatibility
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) continue;

        std::stringstream ss(line);
        std::string addrStr, typeStr, name;
        if (ss >> addrStr >> typeStr >> name) {
            try {
                uint64_t addr = std::stoull(addrStr, nullptr, 16);
                SymbolEntry entry;
                entry.address = addr;
                entry.name = name;
                entry.type = typeStr.empty() ? '?' : typeStr[0];
                entry.size = 0; // System.map doesn't record size explicitly in general formats
                m_symbols.push_back(entry);
                count++;
            } catch (...) {
                // Ignore parse errors on comments or headers
                continue;
            }
        }
    }

    LOG_PARSER_INFO("SymbolResolver: Parsed {} symbols from System.map: {}", count, mapPath);
    if (count > 0) {
        finalizeSymbols();
        return true;
    }
    return false;
}

void SymbolResolver::finalizeSymbols() {
    // Sort symbols by address
    std::sort(m_symbols.begin(), m_symbols.end(), [](const SymbolEntry& a, const SymbolEntry& b) {
        return a.address < b.address;
    });

    // Remove duplicates
    auto last = std::unique(m_symbols.begin(), m_symbols.end(), [](const SymbolEntry& a, const SymbolEntry& b) {
        return a.address == b.address && a.name == b.name;
    });
    m_symbols.erase(last, m_symbols.end());

    LOG_PARSER_INFO("SymbolResolver: Finalized symbol table with {} entries", m_symbols.size());
}

std::string SymbolResolver::resolveAddress(uint64_t address) const {
    if (m_symbols.empty()) {
        std::stringstream ss;
        ss << "0x" << std::hex << address;
        return ss.str();
    }

    // Binary search for the first symbol whose address is strictly greater than the target address
    auto it = std::upper_bound(m_symbols.begin(), m_symbols.end(), address,
        [](uint64_t addr, const SymbolEntry& sym) {
            return addr < sym.address;
        });

    // If it points to the beginning, the address is smaller than the first symbol address
    if (it == m_symbols.begin()) {
        std::stringstream ss;
        ss << "0x" << std::hex << address;
        return ss.str();
    }

    // Decrement to get the closest preceding symbol
    --it;

    uint64_t offset = address - it->address;

    // Check size limit if it was populated from ELF
    if (it->size > 0 && offset >= it->size) {
        std::stringstream ss;
        ss << "0x" << std::hex << address;
        return ss.str();
    }

    // Format output as name+offset
    std::stringstream ss;
    ss << it->name << "+0x" << std::hex << offset;
    return ss.str();
}

uint64_t SymbolResolver::findSymbolAddress(const std::string& name) const {
    // Linear scan by name — called once at startup, O(n) is acceptable.
    for (const auto& sym : m_symbols) {
        if (sym.name == name) return sym.address;
    }
    return 0;
}

} // namespace kernel_analysis
