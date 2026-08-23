#pragma once
#include "../models/log_entry.h"
#include <vector>
#include <mutex>
#include <deque>

namespace emulator::logger::core {

class LogRingBuffer {
public:
    explicit LogRingBuffer(size_t capacity = 500);

    void Push(const models::LogEntry& entry);
    std::vector<models::LogEntry> GetRecent(size_t count = 100) const;
    std::vector<models::LogEntry> GetAll() const;
    void Clear();
    size_t Size() const;

private:
    size_t capacity_;
    mutable std::mutex mutex_;
    std::deque<models::LogEntry> buffer_;
};

} // namespace emulator::logger::core
