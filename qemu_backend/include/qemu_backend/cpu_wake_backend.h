#pragma once
#include <cstdint>

namespace qemu_backend {

class ICpuWakeBackend {
public:
    virtual ~ICpuWakeBackend() = default;

    // Locates target vCPU, programs architectural state, and invokes QEMU's native CPU wake path.
    // Returns true if and only if the CPU wake operation succeeded.
    virtual bool wakeCpu(uint64_t mpidr, uint64_t entryPa, uint64_t contextId) = 0;
};

} // namespace qemu_backend
