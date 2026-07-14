#include "kernel_analysis/elf_loader.h"
#include "core/logger.h"
#include <fstream>
#include <cstring>

namespace kernel_analysis {

bool ElfLoader::loadSymbols(const std::string& elfPath, std::vector<SymbolEntry>& outSymbols) {
    std::ifstream fs(elfPath, std::ios::binary);
    if (!fs) {
        LOG_PARSER_ERROR("ElfLoader: Failed to open ELF file: {}", elfPath);
        return false;
    }

    // Read ELF64 Header
    Elf64_Ehdr ehdr;
    if (!fs.read(reinterpret_cast<char*>(&ehdr), sizeof(ehdr))) {
        LOG_PARSER_ERROR("ElfLoader: Failed to read ELF header from: {}", elfPath);
        return false;
    }

    // Validate ELF magic: 0x7F, 'E', 'L', 'F'
    if (ehdr.e_ident[0] != 0x7F || ehdr.e_ident[1] != 'E' ||
        ehdr.e_ident[2] != 'L' || ehdr.e_ident[3] != 'F') {
        LOG_PARSER_ERROR("ElfLoader: Invalid ELF magic in: {}", elfPath);
        return false;
    }

    // Validate 64-bit ELF class
    if (ehdr.e_ident[4] != 2) { // 2 = ELFCLASS64
        LOG_PARSER_ERROR("ElfLoader: Only ELF64 is supported: {}", elfPath);
        return false;
    }

    LOG_PARSER_INFO("ElfLoader: Parsing ELF section headers (count: {}, offset: {})", ehdr.e_shnum, ehdr.e_shoff);

    // Read all section headers
    std::vector<Elf64_Shdr> shdrs(ehdr.e_shnum);
    fs.seekg(ehdr.e_shoff);
    if (!fs.read(reinterpret_cast<char*>(shdrs.data()), ehdr.e_shnum * sizeof(Elf64_Shdr))) {
        LOG_PARSER_ERROR("ElfLoader: Failed to read ELF section headers");
        return false;
    }

    // Find the symbol table (SHT_SYMTAB) and its linked string table
    int symtabIdx = -1;
    int strtabIdx = -1;

    for (int i = 0; i < ehdr.e_shnum; ++i) {
        if (shdrs[i].sh_type == SHT_SYMTAB) {
            symtabIdx = i;
            strtabIdx = shdrs[i].sh_link;
            break;
        }
    }

    if (symtabIdx == -1 || strtabIdx == -1 || strtabIdx >= ehdr.e_shnum) {
        LOG_PARSER_WARN("ElfLoader: No symbol table (SYMTAB) found in: {}", elfPath);
        return false;
    }

    const auto& symShdr = shdrs[symtabIdx];
    const auto& strShdr = shdrs[strtabIdx];

    // Read string table into memory
    std::vector<char> strtab(strShdr.sh_size);
    fs.seekg(strShdr.sh_offset);
    if (!fs.read(strtab.data(), strShdr.sh_size)) {
        LOG_PARSER_ERROR("ElfLoader: Failed to read string table");
        return false;
    }

    // Read symbol table entries
    uint64_t numSyms = symShdr.sh_size / sizeof(Elf64_Sym);
    std::vector<Elf64_Sym> syms(numSyms);
    fs.seekg(symShdr.sh_offset);
    if (!fs.read(reinterpret_cast<char*>(syms.data()), symShdr.sh_size)) {
        LOG_PARSER_ERROR("ElfLoader: Failed to read symbol table entries");
        return false;
    }

    LOG_PARSER_INFO("ElfLoader: Extracting {} symbols from ELF table", numSyms);

    outSymbols.reserve(outSymbols.size() + numSyms);

    for (const auto& sym : syms) {
        if (sym.st_name >= strtab.size() || sym.st_name == 0) {
            continue; // Skip symbols without valid names
        }

        std::string name = &strtab[sym.st_name];
        if (name.empty()) {
            continue;
        }

        SymbolEntry entry;
        entry.name = name;
        entry.address = sym.st_value;
        entry.size = sym.st_size;

        // Classify symbol type roughly
        unsigned char typeInfo = sym.st_info & 0xF;
        if (typeInfo == 2) { // STT_FUNC
            entry.type = 'T'; // Text / Function symbol
        } else if (typeInfo == 1) { // STT_OBJECT
            entry.type = 'D'; // Data object
        } else {
            entry.type = 't';
        }

        outSymbols.push_back(entry);
    }

    return true;
}

} // namespace kernel_analysis
