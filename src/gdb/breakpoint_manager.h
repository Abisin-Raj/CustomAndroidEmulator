#pragma once
#include "register_snapshot.h"
#include <string>
#include <map>
#include <vector>
#include <optional>
#include <cstdint>
#include <functional>

namespace emulator::gdb {

struct BreakpointInfo {
    std::string name;
    uint64_t address{0};
    bool active{true};
    uint64_t hit_count{0};
    std::string description;
};

using BreakpointHitCallback = std::function<void(const BreakpointInfo& bp, const RegisterSnapshot& regs)>;

class BreakpointManager {
public:
    BreakpointManager() = default;

    void Add(const std::string& name, uint64_t address, const std::string& description = "");
    void Remove(const std::string& name);
    void RemoveByAddress(uint64_t address);
    void Clear();

    std::optional<BreakpointInfo> FindByAddress(uint64_t address) const;
    std::optional<BreakpointInfo> FindByName(const std::string& name) const;
    std::vector<BreakpointInfo> GetAll() const;

    void OnHit(uint64_t address, const RegisterSnapshot& regs);
    void SetHitCallback(BreakpointHitCallback cb);

private:
    std::map<uint64_t, BreakpointInfo> addr_map_;
    std::map<std::string, uint64_t> name_map_;
    BreakpointHitCallback hit_callback_;
};

} // namespace emulator::gdb
