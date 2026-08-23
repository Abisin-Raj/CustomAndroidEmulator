#pragma once
#include "log_sink.h"
#include <fstream>
#include <mutex>
#include <filesystem>

namespace emulator::logger::sinks {

class TimelineSink : public ILogSink {
public:
    explicit TimelineSink(const std::filesystem::path& file_path);
    ~TimelineSink() override;

    void Write(const models::LogEntry& entry) override;
    void Flush() override;

private:
    std::filesystem::path file_path_;
    std::ofstream stream_;
    std::mutex mutex_;
};

} // namespace emulator::logger::sinks
