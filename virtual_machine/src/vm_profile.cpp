#include "virtual_machine/vm_profile.h"
#include "core/config_manager.h"

VmProfile VmProfile::loadByName(const std::string& name) {
    auto data = core::ConfigManager::getProfile(name);
    
    VmProfile p;
    p.name = data.name;
    p.cpu = data.cpu;
    p.machine = data.machine;
    p.memory = data.memory;
    p.nographic = data.nographic;
    return p;
}
