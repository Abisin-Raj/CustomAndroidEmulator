#pragma once
#include <string>

class QemuDetector {
public:
    /**
     * @brief Finds the path to the qemu-system-aarch64 executable.
     *        It checks the local settings, search in the environment PATH,
     *        and scans common folders on Windows.
     * @return Absolute path to the binary, or empty if not found.
     */
    static std::string detectQemuPath();
};
