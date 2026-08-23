#pragma once
#include <string>
#include <mutex>

namespace emulator::runtime {

class ClipboardBridge {
public:
    static ClipboardBridge& Instance();

    void SetHostClipboardText(const std::string& text);
    std::string GetHostClipboardText() const;

    void SetGuestClipboardText(const std::string& text);
    std::string GetGuestClipboardText() const;

    bool SyncToGuest();
    bool SyncToHost();

private:
    ClipboardBridge() = default;
    ~ClipboardBridge() = default;

    mutable std::mutex mutex_;
    std::string host_text_;
    std::string guest_text_;
};

} // namespace emulator::runtime
