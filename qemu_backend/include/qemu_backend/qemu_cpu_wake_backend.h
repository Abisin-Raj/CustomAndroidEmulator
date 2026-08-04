#pragma once
#include "qemu_backend/cpu_wake_backend.h"
#include "qemu_backend/gdb_rsp_client.h"

namespace qemu_backend {

class QemuCpuWakeBackend : public ICpuWakeBackend {
public:
    explicit QemuCpuWakeBackend(GdbRspClient* gdb);
    ~QemuCpuWakeBackend() override = default;

    bool wakeCpu(uint64_t mpidr, uint64_t entryPa, uint64_t contextId) override;

private:
    GdbRspClient* m_gdb;
};

} // namespace qemu_backend
