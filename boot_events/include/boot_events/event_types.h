#pragma once

#include <string>
#include <vector>
#include <map>

enum class BootState {
    VM_STARTING,
    KERNEL_LOADING,
    KERNEL_RUNNING,
    USERSPACE_STARTING,
    SYSTEM_SERVICES,
    BOOT_COMPLETE,
    EMULATION_FAILED
};

enum class EventSeverity {
    Info,
    Warning,
    Error,
    Critical
};

enum class BootEventType {
    KernelVersion,
    CpuOnline,
    PsciInitialized,
    RamdiskMounted,
    InitStarted,
    ServiceStarted,
    PropertyChanged,
    KernelPanic,
    BootCompleted
};

struct BootEvent {
    BootEventType type;
    EventSeverity severity;
    std::string details;
    double confidence;
    double elapsedSeconds;
    std::map<std::string, std::string> metadata;
};
