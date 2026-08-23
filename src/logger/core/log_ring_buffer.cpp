#include "log_ring_buffer.h"
#include <algorithm>

namespace emulator::logger::core {

LogRingBuffer::LogRingBuffer(size_t capacity)
    : capacity_(capacity) {}

void LogRingBuffer::Push(const models::LogEntry& entry) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (buffer_.size() >= capacity_) {
        buffer_.pop_front();
    }
    buffer_.push_back(entry);
}

std::vector<models::LogEntry> LogRingBuffer::GetRecent(size_t count) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<models::LogEntry> result;
    if (buffer_.empty()) return result;

    size_t n = std::min(count, buffer_.size());
    auto start_it = buffer_.end() - n;
    result.assign(start_it, buffer_.end());
    return result;
}

std::vector<models::LogEntry> LogRingBuffer::GetAll() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::vector<models::LogEntry>(buffer_.begin(), buffer_.end());
}

void LogRingBuffer::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    buffer_.clear();
}

size_t LogRingBuffer::Size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return buffer_.size();
}

} // namespace emulator::logger::core
