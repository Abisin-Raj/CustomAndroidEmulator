#pragma once
#include "../models/log_entry.h"
#include "../models/subsystem.h"
#include "../sinks/log_sink.h"
#include "../sinks/file_sink.h"
#include "../sinks/timeline_sink.h"
#include "log_ring_buffer.h"
#include <map>
#include <vector>
#include <memory>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <filesystem>

namespace emulator::logger::core {

class LoggerCore {
public:
    LoggerCore();
    ~LoggerCore();

    void Initialize(const std::filesystem::path& session_dir);
    void Shutdown();

    void Log(models::LogEntry entry);
    
    std::vector<models::LogEntry> GetRecentHistory(size_t count = 100) const;
    void Flush();

private:
    void ProcessQueue();

    std::filesystem::path session_dir_;
    std::map<models::Subsystem, std::shared_ptr<sinks::FileSink>> per_subsystem_sinks_;
    std::shared_ptr<sinks::TimelineSink> timeline_sink_;
    std::shared_ptr<sinks::FileSink> diagnostics_sink_;
    
    LogRingBuffer ring_buffer_;

    std::queue<models::LogEntry> log_queue_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::atomic<bool> running_{false};
    std::thread worker_thread_;
};

} // namespace emulator::logger::core
