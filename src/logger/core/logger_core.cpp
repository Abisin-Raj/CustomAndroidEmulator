#include "logger_core.h"

namespace emulator::logger::core {

LoggerCore::LoggerCore()
    : ring_buffer_(500) {}

LoggerCore::~LoggerCore() {
    Shutdown();
}

void LoggerCore::Initialize(const std::filesystem::path& session_dir) {
    Shutdown();

    session_dir_ = session_dir;
    std::filesystem::create_directories(session_dir_);

    // Initialize per-subsystem sinks
    std::vector<models::Subsystem> all_subsystems = {
        models::Subsystem::LAUNCHER,
        models::Subsystem::ROM,
        models::Subsystem::BOOT,
        models::Subsystem::DTB,
        models::Subsystem::QEMU,
        models::Subsystem::GDB,
        models::Subsystem::STORAGE,
        models::Subsystem::SMC,
        models::Subsystem::ANDROID,
        models::Subsystem::BINDER,
        models::Subsystem::DISPLAY,
        models::Subsystem::DIAGNOSTICS
    };


    for (auto subsys : all_subsystems) {
        auto log_file = session_dir_ / models::ToLogFileName(subsys);
        per_subsystem_sinks_[subsys] = std::make_shared<sinks::FileSink>(log_file);
    }

    // Initialize global timeline sink
    timeline_sink_ = std::make_shared<sinks::TimelineSink>(session_dir_ / "timeline.log");
    diagnostics_sink_ = per_subsystem_sinks_[models::Subsystem::DIAGNOSTICS];

    // Start background processing worker
    running_ = true;
    worker_thread_ = std::thread(&LoggerCore::ProcessQueue, this);
}

void LoggerCore::Shutdown() {
    if (!running_) return;

    running_ = false;
    queue_cv_.notify_all();

    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }

    Flush();
}

void LoggerCore::Log(models::LogEntry entry) {
    // Record into in-memory ring buffer immediately
    ring_buffer_.Push(entry);

    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        log_queue_.push(std::move(entry));
    }
    queue_cv_.notify_one();
}

std::vector<models::LogEntry> LoggerCore::GetRecentHistory(size_t count) const {
    return ring_buffer_.GetRecent(count);
}

void LoggerCore::Flush() {
    // Drain remaining queue synchronously
    std::unique_lock<std::mutex> lock(queue_mutex_);
    while (!log_queue_.empty()) {
        auto entry = std::move(log_queue_.front());
        log_queue_.pop();

        if (timeline_sink_) {
            timeline_sink_->Write(entry);
        }

        auto it = per_subsystem_sinks_.find(entry.subsystem);
        if (it != per_subsystem_sinks_.end() && it->second) {
            it->second->Write(entry);
        }
    }
    lock.unlock();

    for (auto& [subsys, sink] : per_subsystem_sinks_) {
        if (sink) sink->Flush();
    }
    if (timeline_sink_) {
        timeline_sink_->Flush();
    }
}

void LoggerCore::ProcessQueue() {
    while (running_) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        queue_cv_.wait(lock, [this] {
            return !log_queue_.empty() || !running_;
        });

        while (!log_queue_.empty()) {
            auto entry = std::move(log_queue_.front());
            log_queue_.pop();
            lock.unlock();

            if (timeline_sink_) {
                timeline_sink_->Write(entry);
            }

            auto it = per_subsystem_sinks_.find(entry.subsystem);
            if (it != per_subsystem_sinks_.end() && it->second) {
                it->second->Write(entry);
            }

            lock.lock();
        }
    }
}

} // namespace emulator::logger::core
