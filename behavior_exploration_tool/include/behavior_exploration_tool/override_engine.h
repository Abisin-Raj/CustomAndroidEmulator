#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>

namespace bet {

struct OverrideRule {
    std::string id;
    bool enabled = true;
    std::string scope; // "global", "rom", "kernel", "boot-id"
    std::string romName;
    std::string kernelVersion;
    int bootId = 0;
    std::string description;
    
    std::string category; // e.g. "SMC"
    std::string name;     // e.g. "0xC2000601"
    
    std::string action;   // "override_registers"
    std::map<std::string, uint64_t> overrides; // register name -> value
};

class OverrideEngine {
public:
    OverrideEngine() = default;
    ~OverrideEngine() = default;

    // Load rules from a json file, runs schema validation.
    // Returns false if validation fails (e.g. duplicate IDs, invalid scope/action/registers).
    bool loadFromFile(const std::string& filepath);

    // Look up matching override rule by priority (boot-id > kernel > rom > global).
    // Returns true if a match is found and populated.
    bool findOverride(
        const std::string& category,
        const std::string& name,
        const std::string& romName,
        const std::string& kernelVersion,
        int bootId,
        std::string& outRuleId,
        std::string& outDesc,
        std::map<std::string, uint64_t>& outOverrides) const;

    const std::vector<OverrideRule>& getRules() const { return m_rules; }
    void clear() { m_rules.clear(); }

private:
    // Schema validation helpers. Returns false if rule is invalid.
    bool validateRule(const OverrideRule& rule, std::string& outError) const;

    std::vector<OverrideRule> m_rules;
};

} // namespace bet
