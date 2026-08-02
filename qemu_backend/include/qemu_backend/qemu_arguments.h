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
    std::string dtbPath;
    std::string cmdline;
    std::string extraArgs;
    bool nographic      = true;
    // When true, appends -d unimp,guest_errors -D qemu_trace.log to observe
    // guest accesses to unmapped/unimplemented MMIO regions.
    bool enableMmioTrace = false;
    // When true, appends -dumpdtb qemu_generated.dtb so QEMU writes its
    // auto-generated DTB to disk for inspection and CPU reg comparison.
    bool enableDtbDump   = false;
};

class QemuArguments {
public:
    /**
     * @brief Generates the command-line argument list for QEMU matching the configuration parameters.
     */
    static QStringList build(const QemuConfigParameters& params);
};
