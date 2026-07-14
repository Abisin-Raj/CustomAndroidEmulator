#include "boot_pipeline/boot_pipeline_manager.h"
#include "boot_pipeline/kernel_detector.h"
#include "boot_pipeline/init_detector.h"
#include "boot_pipeline/panic_detector.h"
#include "core/logger.h"
#include <filesystem>
#include <regex>

BootPipelineManager::BootPipelineManager(EventDispatcher* dispatcher)
    : m_dispatcher(dispatcher) {
    m_context.currentState = BootState::VM_STARTING;
}

void BootPipelineManager::initializeDefaultDetectors() {
    m_registry.registerDetector(std::make_unique<KernelDetector>());
    m_registry.registerDetector(std::make_unique<InitDetector>());
    m_registry.registerDetector(std::make_unique<PanicDetector>());
}

bool BootPipelineManager::loadSymbols(const std::string& romDir) {
    m_resolver.clear();
    std::filesystem::path dir(romDir);
    
    // 1. Check for System.map in ROM directory
    std::filesystem::path systemMapPath = dir / "System.map";
    if (std::filesystem::exists(systemMapPath)) {
        LOG_PARSER_INFO("BootPipelineManager: Found System.map in ROM dir: {}", systemMapPath.string());
        return m_resolver.loadSystemMapSymbols(systemMapPath.string());
    }

    // 2. Check for vmlinux ELF binary in ROM directory
    std::filesystem::path vmlinuxPath = dir / "vmlinux";
    if (std::filesystem::exists(vmlinuxPath)) {
        LOG_PARSER_INFO("BootPipelineManager: Found vmlinux ELF in ROM dir: {}", vmlinuxPath.string());
        return m_resolver.loadElfSymbols(vmlinuxPath.string());
    }

    // 3. Fallback: check project reference library for a matching map file (e.g. System.map)
    std::filesystem::path refDir = std::filesystem::current_path() / "reference" / "system_maps";
    if (std::filesystem::exists(refDir)) {
        for (const auto& entry : std::filesystem::directory_iterator(refDir)) {
            if (entry.path().extension() == ".map" || entry.path().filename().string().find("System.map") != std::string::npos) {
                LOG_PARSER_INFO("BootPipelineManager: Loading fallback symbol file from reference library: {}", entry.path().string());
                return m_resolver.loadSystemMapSymbols(entry.path().string());
            }
        }
    }

    LOG_PARSER_WARN("BootPipelineManager: No symbol files (System.map / vmlinux) found for address resolution.");
    return false;
}

void BootPipelineManager::setRomName(const std::string& name) {
    m_session.setRomName(name);
}

std::string BootPipelineManager::processLogLine(const std::string& line, double elapsedSeconds) {
    m_context.elapsedSeconds = elapsedSeconds;
    m_session.setDuration(elapsedSeconds);

    // Annotate addresses in the line if symbols are loaded
    std::string annotated = line;
    if (m_resolver.getSymbolCount() > 0) {
        std::regex addrRegex("(ffffff[0-9a-fA-F]{10}|ffff0000[0-9a-fA-F]{8})");
        std::smatch match;
        std::string::const_iterator searchStart(annotated.cbegin());
        std::string newAnnotated = "";
        size_t lastPos = 0;
        
        while (std::regex_search(searchStart, annotated.cend(), match, addrRegex)) {
            size_t matchPos = std::distance(annotated.cbegin(), match[0].first);
            newAnnotated += annotated.substr(lastPos, matchPos - lastPos);
            newAnnotated += match[0].str();
            
            try {
                uint64_t addr = std::stoull(match[0].str(), nullptr, 16);
                std::string resolved = m_resolver.resolveAddress(addr);
                if (resolved != match[0].str() && resolved.find("0x") != 0) {
                    newAnnotated += " [" + resolved + "]";
                }
            } catch (...) {
                // Ignore parse errors on transient hex tokens
            }
            
            lastPos = matchPos + match[0].length();
            searchStart = match[0].second;
        }
        newAnnotated += annotated.substr(lastPos);
        annotated = newAnnotated;
    }

    for (const auto& detector : m_registry.getDetectors()) {
        auto events = detector->processLine(annotated, m_context);
        for (const auto& ev : events) {
            m_session.addEvent(ev);
            
            if (m_dispatcher) {
                m_dispatcher->dispatchEvent(ev);
            }

            if (ev.type == BootEventType::KernelVersion) {
                m_context.kernelVersion = ev.details;
                m_session.setKernelVersion(ev.details);
                if (m_context.currentState < BootState::KERNEL_RUNNING) {
                    transitionState(BootState::KERNEL_RUNNING);
                }
            } else if (ev.type == BootEventType::InitStarted) {
                m_context.initDetected = true;
                if (m_context.currentState < BootState::USERSPACE_STARTING) {
                    transitionState(BootState::USERSPACE_STARTING);
                }
            } else if (ev.type == BootEventType::ServiceStarted) {
                if (m_context.currentState < BootState::SYSTEM_SERVICES) {
                    transitionState(BootState::SYSTEM_SERVICES);
                }
            } else if (ev.type == BootEventType::BootCompleted) {
                transitionState(BootState::BOOT_COMPLETE);
                m_session.setFinalResult("Success");
            } else if (ev.type == BootEventType::KernelPanic) {
                m_context.panicDetected = true;
                transitionState(BootState::EMULATION_FAILED);
                m_session.setFinalResult("Failed");
            }
        }
    }
    return annotated;
}

void BootPipelineManager::transitionState(BootState newState) {
    if (newState != m_context.currentState) {
        m_context.currentState = newState;
        if (m_dispatcher) {
            m_dispatcher->dispatchStateChange(newState);
        }
        
        std::string stateStr;
        switch (newState) {
            case BootState::VM_STARTING:        stateStr = "VM_STARTING"; break;
            case BootState::KERNEL_LOADING:     stateStr = "KERNEL_LOADING"; break;
            case BootState::KERNEL_RUNNING:     stateStr = "KERNEL_RUNNING"; break;
            case BootState::USERSPACE_STARTING: stateStr = "USERSPACE_STARTING"; break;
            case BootState::SYSTEM_SERVICES:    stateStr = "SYSTEM_SERVICES"; break;
            case BootState::BOOT_COMPLETE:      stateStr = "BOOT_COMPLETE"; break;
            case BootState::EMULATION_FAILED:   stateStr = "EMULATION_FAILED"; break;
        }
        LOG_BOOT_INFO("Boot state transitioned to: {}", stateStr);
    }
}
