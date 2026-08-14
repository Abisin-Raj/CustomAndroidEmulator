#include "kernel_runtime/kernel_commandline.h"
#include <sstream>
#include <vector>
#include <cctype>

std::string KernelCommandLine::build(const std::string& baseCmdline, bool enforceLogs) {
    if (!enforceLogs || baseCmdline.find("earlycon=msm_geni_serial") != std::string::npos || baseCmdline.find("console=ttyMSM") != std::string::npos) {
        return baseCmdline;
    }

    std::string result = baseCmdline;
    std::vector<std::string> tokens;
    std::stringstream ss(baseCmdline);
    std::string token;
    bool hasConsole = false;
    bool hasEarlycon = false;
    bool hasDevkmsg = false;
    bool hasNokaslr = false;

    while (ss >> token) {
        if (token.rfind("console=", 0) == 0) {
            tokens.push_back("console=ttyAMA0");
            hasConsole = true;
        } else if (token.rfind("earlycon=", 0) == 0) {
            tokens.push_back("earlycon=pl011,0x09000000");
            hasEarlycon = true;
        } else if (token.rfind("printk.devkmsg=", 0) == 0) {
            tokens.push_back("printk.devkmsg=on");
            hasDevkmsg = true;
        } else if (token == "nokaslr") {
            tokens.push_back("nokaslr");
            hasNokaslr = true;
        } else {
            tokens.push_back(token);
        }
    }

    if (!hasConsole) tokens.push_back("console=ttyAMA0");
    if (!hasEarlycon) tokens.push_back("earlycon=pl011,0x09000000");
    if (!hasDevkmsg) tokens.push_back("printk.devkmsg=on");
    if (!hasNokaslr) tokens.push_back("nokaslr");

    std::stringstream out;
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (i > 0) out << " ";
        out << tokens[i];
    }
    return out.str();
}
