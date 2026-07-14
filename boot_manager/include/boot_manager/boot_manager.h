#pragma once
#include "models/boot_report.h"
#include <string>
#include <filesystem>

class BootManager {
public:
    /**
     * @brief Performs boot validation, creates/updates boot_config.json,
     *        and outputs a readiness validation checklist report.
     */
    static BootValidationReport validateAndPrepareBoot(const std::string& romName, const std::filesystem::path& romDir);
};
