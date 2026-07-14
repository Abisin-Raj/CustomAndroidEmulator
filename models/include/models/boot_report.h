#pragma once
#include <string>
#include <vector>

struct BootValidationReport {
    std::string status = "Not Bootable"; // "Ready", "Warning", "Error", "Not Bootable"
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
    std::vector<std::string> recommendations;
    std::string vmProfile = "generic_arm64";
};
