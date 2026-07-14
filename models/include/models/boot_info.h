#pragma once
#include <string>
#include <cstdint>

struct BootImgInfo {
    bool isValid = false;
    uint32_t headerVersion = 0;
    uint32_t pageSize = 0;
    uint64_t kernelSize = 0;
    uint64_t ramdiskSize = 0;
    std::string osVersion = "Unknown";
    std::string osPatchLevel = "Unknown";
    std::string cmdline;
    std::string kernelRelease = "Unknown";

    // Helper to align block offsets
    static uint64_t align(uint64_t size, uint64_t page_size) {
        if (page_size == 0) return 0;
        return ((size + page_size - 1) / page_size) * page_size;
    }

    uint64_t getKernelOffset() const {
        return pageSize;
    }

    uint64_t getRamdiskOffset() const {
        return getKernelOffset() + align(kernelSize, pageSize);
    }
};
