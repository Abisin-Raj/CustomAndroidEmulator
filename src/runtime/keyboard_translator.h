#pragma once
#include "src/input/input_event_record.h"
#include <vector>
#include <cstdint>
#include <string>

namespace emulator::runtime {

class KeyboardTranslator {
public:
    KeyboardTranslator() = default;

    std::vector<emulator::input::InputEventRecord> OnKeyDown(uint32_t host_vk_code);
    std::vector<emulator::input::InputEventRecord> OnKeyUp(uint32_t host_vk_code);
    std::vector<emulator::input::InputEventRecord> OnTextInput(const std::string& text);

    static uint16_t MapVkToLinuxKey(uint32_t host_vk_code);
};

} // namespace emulator::runtime
