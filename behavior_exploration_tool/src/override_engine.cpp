#include "behavior_exploration_tool/override_engine.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <spdlog/spdlog.h>
#include <filesystem>
#include <set>
#include <algorithm>

namespace bet {

using json = nlohmann::json;

bool OverrideEngine::loadFromFile(const std::string& filepath) {
    clear();
    if (!std::filesystem::exists(filepath)) {
        spdlog::info("[BET] OverrideEngine: Overrides file does not exist: {}", filepath);
        return true; // Empty overrides configuration is valid (disabled by default)
    }

    try {
        std::ifstream in(filepath);
        if (!in.is_open()) {
            spdlog::error("[BET] OverrideEngine: Failed to open overrides file: {}", filepath);
            return false;
        }

        json jRoot;
        in >> jRoot;
        in.close();

        if (!jRoot.is_object()) {
            spdlog::error("[BET] OverrideEngine: Invalid root JSON type (expected object)");
            return false;
        }

        bool enabled = jRoot.value("enabled", true);
        if (!enabled) {
            spdlog::info("[BET] OverrideEngine: Overrides are globally disabled in config");
            return true; 
        }

        std::vector<OverrideRule> loadedRules;
        std::set<std::string> ruleIds;

        if (jRoot.count("rules") && jRoot["rules"].is_array()) {
            for (const auto& jRule : jRoot["rules"]) {
                OverrideRule rule;
                rule.id = jRule.value("id", "");
                rule.enabled = jRule.value("enabled", true);
                rule.scope = jRule.value("scope", "");
                rule.romName = jRule.value("romName", "");
                rule.kernelVersion = jRule.value("kernelVersion", "");
                rule.bootId = jRule.value("bootId", 0);
                rule.description = jRule.value("description", "");
                rule.category = jRule.value("category", "");
                rule.name = jRule.value("name", "");
                rule.action = jRule.value("action", "");
                
                // Parse overrides dict
                if (jRule.count("overrides") && jRule["overrides"].is_object()) {
                    for (auto it = jRule["overrides"].begin(); it != jRule["overrides"].end(); ++it) {
                        if (it.value().is_number()) {
                            rule.overrides[it.key()] = it.value().get<uint64_t>();
                        } else if (it.value().is_string()) {
                            std::string hexStr = it.value().get<std::string>();
                            try {
                                rule.overrides[it.key()] = std::stoull(hexStr, nullptr, 16);
                            } catch (...) {
                                spdlog::error("[BET] OverrideEngine: Failed to parse hex string '{}' for register '{}'", hexStr, it.key());
                                return false;
                            }
                        }
                    }
                }

                // Validation
                std::string validationError;
                if (!validateRule(rule, validationError)) {
                    spdlog::error("[BET] OverrideEngine: Rule '{}' failed validation: {}", rule.id, validationError);
                    return false;
                }

                if (ruleIds.count(rule.id)) {
                    spdlog::error("[BET] OverrideEngine: Duplicate rule ID detected: '{}'", rule.id);
                    return false;
                }

                ruleIds.insert(rule.id);
                if (rule.enabled) {
                    loadedRules.push_back(rule);
                }
            }
        }

        m_rules = loadedRules;
        spdlog::info("[BET] OverrideEngine: Successfully loaded and validated {} rules", m_rules.size());
        return true;

    } catch (const std::exception& e) {
        spdlog::error("[BET] OverrideEngine: Exception loading rules: {}", e.what());
        return false;
    }
}

bool OverrideEngine::validateRule(const OverrideRule& rule, std::string& outError) const {
    if (rule.id.empty()) {
        outError = "Rule ID is empty";
        return false;
    }
    if (rule.category.empty() || rule.name.empty()) {
        outError = "Rule category or name is empty";
        return false;
    }
    if (rule.action != "override_registers") {
        outError = "Invalid action '" + rule.action + "' (only 'override_registers' is currently supported)";
        return false;
    }
    
    // Check scope
    if (rule.scope != "global" && rule.scope != "rom" && rule.scope != "kernel" && rule.scope != "boot-id") {
        outError = "Invalid scope '" + rule.scope + "' (must be global, rom, kernel, or boot-id)";
        return false;
    }

    if (rule.scope == "rom" && rule.romName.empty()) {
        outError = "romName cannot be empty for rom scope";
        return false;
    }
    if (rule.scope == "kernel" && rule.kernelVersion.empty()) {
        outError = "kernelVersion cannot be empty for kernel scope";
        return false;
    }
    if (rule.scope == "boot-id" && rule.bootId <= 0) {
        outError = "bootId must be greater than 0 for boot-id scope";
        return false;
    }

    // Check register keys
    for (const auto& pair : rule.overrides) {
        std::string reg = pair.first;
        // Make sure it contains valid register name (e.g. starting with x, w, pc, sp, etc. and ending with numbers)
        if (reg.empty()) {
            outError = "Register name cannot be empty";
            return false;
        }
        std::transform(reg.begin(), reg.end(), reg.begin(), ::tolower);
        bool valid = (reg == "pc" || reg == "sp" || reg == "lr" || 
                      (reg[0] == 'x' && std::all_of(reg.begin() + 1, reg.end(), ::isdigit)) ||
                      (reg[0] == 'w' && std::all_of(reg.begin() + 1, reg.end(), ::isdigit)));
        if (!valid) {
            outError = "Invalid register name target: '" + pair.first + "'";
            return false;
        }
    }

    return true;
}

bool OverrideEngine::findOverride(
    const std::string& category,
    const std::string& name,
    const std::string& romName,
    const std::string& kernelVersion,
    int bootId,
    std::string& outRuleId,
    std::string& outDesc,
    std::map<std::string, uint64_t>& outOverrides) const {
    
    int bestPriority = 0;
    const OverrideRule* bestMatch = nullptr;

    for (const auto& rule : m_rules) {
        if (!rule.enabled) continue;
        if (rule.category != category) continue;
        if (rule.name != name) continue;

        int priority = 0;
        if (rule.scope == "boot-id") {
            if (rule.bootId == bootId) {
                priority = 4;
            }
        } else if (rule.scope == "kernel") {
            if (rule.kernelVersion == kernelVersion) {
                priority = 3;
            }
        } else if (rule.scope == "rom") {
            if (rule.romName == romName) {
                priority = 2;
            }
        } else if (rule.scope == "global") {
            priority = 1;
        }

        if (priority > bestPriority) {
            bestPriority = priority;
            bestMatch = &rule;
        }
    }

    if (bestMatch != nullptr) {
        outRuleId = bestMatch->id;
        outDesc = bestMatch->description;
        outOverrides = bestMatch->overrides;
        return true;
    }

    return false;
}

} // namespace bet
