#pragma once
#include "../models/log_entry.h"
#include <memory>

namespace emulator::logger::sinks {

class ILogSink {
public:
    virtual ~ILogSink() = default;
    virtual void Write(const models::LogEntry& entry) = 0;
    virtual void Flush() = 0;
};

using LogSinkPtr = std::shared_ptr<ILogSink>;

} // namespace emulator::logger::sinks
