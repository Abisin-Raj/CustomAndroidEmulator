#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

/// A single block-range command from an Android transfer.list file.
struct TransferCommand {
    enum class Op { New, Zero, Erase, Stash, Free, Unknown };

    Op op = Op::Unknown;

    /// Inclusive [start, end) block pairs. Each pair means blocks [first, second).
    std::vector<std::pair<uint64_t, uint64_t>> ranges;

    /// For Stash / Free commands, the stash label.
    std::string stashId;
};

/// Parsed representation of an Android *.transfer.list file.
struct TransferList {
    int      version     = 0;   ///< Transfer list version (1–4).
    uint64_t totalBlocks = 0;   ///< Total number of 4 096-byte blocks in the output image.
    uint64_t stashBlocks = 0;   ///< Maximum stash blocks required.
    std::vector<TransferCommand> commands;
};

/// Parses Android OTA transfer.list files (versions 1–4).
class TransferListParser {
public:
    /// Parse @p path and return the structured transfer list.
    /// Throws std::runtime_error on malformed input.
    static TransferList parse(const std::filesystem::path& path);

    /// Convert a range-set string ("4,0,10,20,30") to a list of [start,end) pairs.
    static std::vector<std::pair<uint64_t, uint64_t>> parseRangeSet(const std::string& rangeStr);
};
