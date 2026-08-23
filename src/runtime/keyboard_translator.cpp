#include "keyboard_translator.h"
#include <chrono>

namespace emulator::runtime {

static uint64_t GetTimestampUs() {
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(now).count();
}

uint16_t KeyboardTranslator::MapVkToLinuxKey(uint32_t host_vk) {
    switch (host_vk) {
        // Navigation & Controls
        case 0x1B: return static_cast<uint16_t>(emulator::input::KeyCode::KEY_BACK);       // VK_ESCAPE
        case 0x24: return static_cast<uint16_t>(emulator::input::KeyCode::KEY_HOMEPAGE);   // VK_HOME
        case 0x74: return static_cast<uint16_t>(emulator::input::KeyCode::KEY_HOMEPAGE);   // VK_F5
        case 0x76: return static_cast<uint16_t>(emulator::input::KeyCode::KEY_POWER);      // VK_F7
        case 0x7B: return static_cast<uint16_t>(emulator::input::KeyCode::KEY_POWER);      // VK_F12
        case 0x70: return static_cast<uint16_t>(emulator::input::KeyCode::KEY_VOLUMEDOWN); // VK_F1
        case 0x71: return static_cast<uint16_t>(emulator::input::KeyCode::KEY_VOLUMEUP);   // VK_F2

        // Standard Keys
        case 0x0D: return 28;  // KEY_ENTER
        case 0x08: return 14;  // KEY_BACKSPACE
        case 0x09: return 15;  // KEY_TAB
        case 0x20: return 57;  // KEY_SPACE
        case 0x25: return 105; // KEY_LEFT
        case 0x26: return 103; // KEY_UP
        case 0x27: return 106; // KEY_RIGHT
        case 0x28: return 108; // KEY_DOWN

        // Number row (0-9)
        case '0': return 11;
        case '1': return 2;
        case '2': return 3;
        case '3': return 4;
        case '4': return 5;
        case '5': return 6;
        case '6': return 7;
        case '7': return 8;
        case '8': return 9;
        case '9': return 10;

        // Letters (A-Z)
        case 'A': return 30;
        case 'B': return 48;
        case 'C': return 46;
        case 'D': return 32;
        case 'E': return 18;
        case 'F': return 33;
        case 'G': return 34;
        case 'H': return 35;
        case 'I': return 23;
        case 'J': return 36;
        case 'K': return 37;
        case 'L': return 38;
        case 'M': return 50;
        case 'N': return 49;
        case 'O': return 24;
        case 'P': return 25;
        case 'Q': return 16;
        case 'R': return 19;
        case 'S': return 31;
        case 'T': return 20;
        case 'U': return 22;
        case 'V': return 47;
        case 'W': return 17;
        case 'X': return 45;
        case 'Y': return 21;
        case 'Z': return 44;

        default: return 0;
    }
}

std::vector<emulator::input::InputEventRecord> KeyboardTranslator::OnKeyDown(uint32_t host_vk_code) {
    uint16_t linux_code = MapVkToLinuxKey(host_vk_code);
    if (linux_code == 0) return {};

    uint64_t ts = GetTimestampUs();
    std::vector<emulator::input::InputEventRecord> events;
    events.push_back({ts, static_cast<uint16_t>(emulator::input::EventType::EV_KEY), linux_code, 1, "EV_KEY", emulator::input::InputEventRecord::ResolveCodeName(1, linux_code), "", 0.0, "INJECTED"});
    events.push_back({ts, static_cast<uint16_t>(emulator::input::EventType::EV_SYN), static_cast<uint16_t>(emulator::input::SynCode::SYN_REPORT), 0, "EV_SYN", "SYN_REPORT", "", 0.0, "INJECTED"});
    return events;
}

std::vector<emulator::input::InputEventRecord> KeyboardTranslator::OnKeyUp(uint32_t host_vk_code) {
    uint16_t linux_code = MapVkToLinuxKey(host_vk_code);
    if (linux_code == 0) return {};

    uint64_t ts = GetTimestampUs();
    std::vector<emulator::input::InputEventRecord> events;
    events.push_back({ts, static_cast<uint16_t>(emulator::input::EventType::EV_KEY), linux_code, 0, "EV_KEY", emulator::input::InputEventRecord::ResolveCodeName(1, linux_code), "", 0.0, "INJECTED"});
    events.push_back({ts, static_cast<uint16_t>(emulator::input::EventType::EV_SYN), static_cast<uint16_t>(emulator::input::SynCode::SYN_REPORT), 0, "EV_SYN", "SYN_REPORT", "", 0.0, "INJECTED"});
    return events;
}

std::vector<emulator::input::InputEventRecord> KeyboardTranslator::OnTextInput(const std::string& text) {
    std::vector<emulator::input::InputEventRecord> all_events;
    for (char c : text) {
        uint32_t vk = static_cast<uint32_t>(toupper(c));
        auto down = OnKeyDown(vk);
        auto up = OnKeyUp(vk);
        all_events.insert(all_events.end(), down.begin(), down.end());
        all_events.insert(all_events.end(), up.begin(), up.end());
    }
    return all_events;
}

} // namespace emulator::runtime
