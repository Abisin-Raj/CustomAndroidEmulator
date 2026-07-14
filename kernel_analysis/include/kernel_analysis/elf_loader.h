#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <map>

namespace kernel_analysis {

struct SymbolEntry {
    std::string name;
    uint64_t address;
    uint64_t size;
    char type; // 'T' = text, 'D' = data, etc.
};

class ElfLoader {
public:
    ElfLoader() = default;
    ~ElfLoader() = default;

    // Load symbols from an ELF64 file (e.g. vmlinux)
    bool loadSymbols(const std::string& elfPath, std::vector<SymbolEntry>& outSymbols);

private:
    // Manual definitions of standard ELF64 headers for cross-platform compatibility
    struct Elf64_Ehdr {
        unsigned char e_ident[16];
        uint16_t      e_type;
        uint16_t      e_machine;
        uint32_t      e_version;
        uint64_t      e_entry;
        uint64_t      e_phoff;
        uint64_t      e_shoff;
        uint32_t      e_flags;
        uint16_t      e_ehsize;
        uint16_t      e_phentsize;
        uint16_t      e_phnum;
        uint16_t      e_shentsize;
        uint16_t      e_shnum;
        uint16_t      e_shstrndx;
    };

    struct Elf64_Shdr {
        uint32_t sh_name;
        uint32_t sh_type;
        uint64_t sh_flags;
        uint64_t sh_addr;
        uint64_t sh_offset;
        uint64_t sh_size;
        uint32_t sh_link;
        uint32_t sh_info;
        uint64_t sh_addralign;
        uint64_t sh_entsize;
    };

    struct Elf64_Sym {
        uint32_t      st_name;
        unsigned char st_info;
        unsigned char st_other;
        uint16_t      st_shndx;
        uint64_t      st_value;
        uint64_t      st_size;
    };

    static constexpr uint32_t SHT_SYMTAB = 2;
    static constexpr uint32_t SHT_STRTAB = 3;
};

} // namespace kernel_analysis
