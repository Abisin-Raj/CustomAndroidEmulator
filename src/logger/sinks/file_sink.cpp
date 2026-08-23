#include "file_sink.h"
#include <iomanip>
#include <sstream>

namespace emulator::logger::sinks {

FileSink::FileSink(const std::filesystem::path& file_path)
    : file_path_(file_path) {
    if (file_path_.has_parent_path()) {
        std::filesystem::create_directories(file_path_.parent_path());
    }
    stream_.open(file_path_, std::ios::out | std::ios::app);
}

FileSink::~FileSink() {
    Flush();
    if (stream_.is_open()) {
        stream_.close();
    }
}

void FileSink::Write(const models::LogEntry& entry) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!stream_.is_open()) return;

    auto time_t_val = std::chrono::system_clock::to_time_t(entry.timestamp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        entry.timestamp.time_since_epoch()) % 1000;

    std::tm tm_buf{};
#if defined(_WIN32) || defined(_WIN64)
    localtime_s(&tm_buf, &time_t_val);
#else
    localtime_r(&time_t_val, &tm_buf);
#endif

    stream_ << std::put_time(&tm_buf, "%H:%M:%S")
            << '.' << std::setfill('0') << std::setw(3) << ms.count()
            << std::setfill(' ')
            << " [" << models::ToString(entry.subsystem) << "] "
            << std::left << std::setw(5) << models::ToString(entry.level)
            << " " << entry.message;


    if (entry.pc.has_value()) {
        stream_ << " (PC=0x" << std::hex << entry.pc.value() << std::dec << ")";
    }
    if (entry.duration_ms.has_value()) {
        stream_ << " [+" << std::fixed << std::setprecision(2) << entry.duration_ms.value() << "ms]";
    }
    stream_ << "\n";
}

void FileSink::Flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stream_.is_open()) {
        stream_.flush();
    }
}

} // namespace emulator::logger::sinks
