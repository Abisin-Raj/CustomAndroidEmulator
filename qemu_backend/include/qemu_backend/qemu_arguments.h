#pragma once
#include <string>
#include <vector>
#include <QStringList>

struct QemuConfigParameters {
    std::string cpu = "cortex-a53";
    std::string machine = "virt";
    std::string memory = "1024M";
    std::string kernelPath;
    std::string ramdiskPath;
    std::string cmdline;
    std::string extraArgs;
    bool nographic = true;
};

class QemuArguments {
public:
    /**
     * @brief Generates the command-line argument list for QEMU matching the configuration parameters.
     */
    static QStringList build(const QemuConfigParameters& params);
};
