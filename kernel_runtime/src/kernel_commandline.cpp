#include "kernel_runtime/kernel_commandline.h"
#include <sstream>
#include <cctype>

std::string KernelCommandLine::build(const std::string& baseCmdline, bool enforceLogs) {
    std::string result = baseCmdline;

    if (enforceLogs) {
        // Trim leading and trailing spaces
        while (!result.empty() && std::isspace(static_cast<unsigned char>(result.front()))) {
            result.erase(0, 1);
        }
        while (!result.empty() && std::isspace(static_cast<unsigned char>(result.back()))) {
            result.pop_back();
        }

        auto appendArgIfMissing = [&result](const std::string& key, const std::string& fullArg) {
            if (result.find(key) == std::string::npos) {
                if (!result.empty()) {
                    result += " ";
                }
                result += fullArg;
            }
        };

        appendArgIfMissing("console=", "console=ttyAMA0");
        appendArgIfMissing("earlycon=", "earlycon=pl011,0x09000000");
        appendArgIfMissing("printk.devkmsg=", "printk.devkmsg=on");
    }

    return result;
}
