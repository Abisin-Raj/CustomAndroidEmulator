#include "image_converter/transfer_list_parser.h"
#include <fstream>
#include <sstream>
#include <stdexcept>

// ---------------------------------------------------------------------------
// Range set parsing: "N,s0,e0,s1,e1,..." → vector of [start,end) pairs
// ---------------------------------------------------------------------------
std::vector<std::pair<uint64_t, uint64_t>>
TransferListParser::parseRangeSet(const std::string& rangeStr)
{
    std::vector<std::pair<uint64_t, uint64_t>> result;
    std::istringstream ss(rangeStr);
    std::string token;

    // First token is the count of following numbers
    if (!std::getline(ss, token, ',')) return result;
    int count = std::stoi(token);

    std::vector<uint64_t> nums;
    while (std::getline(ss, token, ',')) {
        nums.push_back(std::stoull(token));
    }

    if (static_cast<int>(nums.size()) != count) {
        throw std::runtime_error("Range set count mismatch: expected " +
                                 std::to_string(count) + " but got " +
                                 std::to_string(nums.size()));
    }
    if (count % 2 != 0) {
        throw std::runtime_error("Range set must have an even number of values.");
    }

    for (int i = 0; i < count; i += 2) {
        result.emplace_back(nums[i], nums[i + 1]);
    }
    return result;
}

// ---------------------------------------------------------------------------
// Transfer list file parser
// ---------------------------------------------------------------------------
TransferList TransferListParser::parse(const std::filesystem::path& path)
{
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("Cannot open transfer list: " + path.string());
    }

    TransferList tl;
    std::string line;

    // Line 1: version
    if (!std::getline(file, line)) throw std::runtime_error("Transfer list is empty.");
    // Trim CR
    if (!line.empty() && line.back() == '\r') line.pop_back();
    tl.version = std::stoi(line);

    // Line 2: total blocks
    if (!std::getline(file, line)) throw std::runtime_error("Missing total block count.");
    if (!line.empty() && line.back() == '\r') line.pop_back();
    tl.totalBlocks = std::stoull(line);

    // Version >= 2: two more header lines (max stash entries, max stash blocks)
    if (tl.version >= 2) {
        std::getline(file, line); // max simultaneous stash entries (ignored)
        std::getline(file, line); // max stash blocks needed
        if (!line.empty() && line.back() == '\r') line.pop_back();
        tl.stashBlocks = std::stoull(line);
    }

    // Parse commands
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        std::istringstream ls(line);
        std::string opStr;
        ls >> opStr;

        TransferCommand cmd;

        if (opStr == "new") {
            cmd.op = TransferCommand::Op::New;
            std::string ranges;
            ls >> ranges;
            cmd.ranges = parseRangeSet(ranges);
        } else if (opStr == "zero") {
            cmd.op = TransferCommand::Op::Zero;
            std::string ranges;
            ls >> ranges;
            cmd.ranges = parseRangeSet(ranges);
        } else if (opStr == "erase") {
            cmd.op = TransferCommand::Op::Erase;
            std::string ranges;
            ls >> ranges;
            cmd.ranges = parseRangeSet(ranges);
        } else if (opStr == "stash") {
            cmd.op = TransferCommand::Op::Stash;
            ls >> cmd.stashId;
            std::string ranges;
            ls >> ranges;
            cmd.ranges = parseRangeSet(ranges);
        } else if (opStr == "free") {
            cmd.op = TransferCommand::Op::Free;
            ls >> cmd.stashId;
        } else {
            cmd.op = TransferCommand::Op::Unknown;
        }

        tl.commands.push_back(std::move(cmd));
    }

    return tl;
}
