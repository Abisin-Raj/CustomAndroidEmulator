#include "clipboard_bridge.h"

namespace emulator::runtime {

ClipboardBridge& ClipboardBridge::Instance() {
    static ClipboardBridge instance;
    return instance;
}

void ClipboardBridge::SetHostClipboardText(const std::string& text) {
    std::lock_guard<std::mutex> lock(mutex_);
    host_text_ = text;
}

std::string ClipboardBridge::GetHostClipboardText() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return host_text_;
}

void ClipboardBridge::SetGuestClipboardText(const std::string& text) {
    std::lock_guard<std::mutex> lock(mutex_);
    guest_text_ = text;
}

std::string ClipboardBridge::GetGuestClipboardText() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return guest_text_;
}

bool ClipboardBridge::SyncToGuest() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (host_text_.empty() || host_text_ == guest_text_) return false;
    guest_text_ = host_text_;
    return true;
}

bool ClipboardBridge::SyncToHost() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (guest_text_.empty() || guest_text_ == host_text_) return false;
    host_text_ = guest_text_;
    return true;
}

} // namespace emulator::runtime
