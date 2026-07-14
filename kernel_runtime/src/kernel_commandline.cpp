#include "kernel_runtime/kernel_commandline.h"
#include <sstream>
#include <vector>
#include <cctype>

std::string KernelCommandLine::build(const std::string& baseCmdline, bool enforceLogs) {
    std::string result = baseCmdline;

    if (enforceLogs) {
        std::vector<std::string> tokens;
        std::stringstream ss(baseCmdline);
        std::string token;
        bool hasConsole = false;
        bool hasEarlycon = false;
        bool hasDevkmsg = false;

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
            } else {
                tokens.push_back(token);
            }
        }

        if (!hasConsole) tokens.push_back("console=ttyAMA0");
        if (!hasEarlycon) tokens.push_back("earlycon=pl011,0x09000000");
        if (!hasDevkmsg) tokens.push_back("printk.devkmsg=on");

        std::stringstream out;
        for (size_t i = 0; i < tokens.size(); ++i) {
            if (i > 0) out << " ";
            out << tokens[i];
        }
        result = out.str();
    }

    return result;
}
