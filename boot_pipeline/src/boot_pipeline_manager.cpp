#include "boot_pipeline/boot_pipeline_manager.h"
#include "boot_pipeline/kernel_detector.h"
#include "boot_pipeline/init_detector.h"
#include "boot_pipeline/panic_detector.h"
#include "core/logger.h"

BootPipelineManager::BootPipelineManager(EventDispatcher* dispatcher)
    : m_dispatcher(dispatcher) {
    m_context.currentState = BootState::VM_STARTING;
}

void BootPipelineManager::initializeDefaultDetectors() {
    m_registry.registerDetector(std::make_unique<KernelDetector>());
    m_registry.registerDetector(std::make_unique<InitDetector>());
    m_registry.registerDetector(std::make_unique<PanicDetector>());
}

void BootPipelineManager::setRomName(const std::string& name) {
    m_session.setRomName(name);
}

void BootPipelineManager::processLogLine(const std::string& line, double elapsedSeconds) {
    m_context.elapsedSeconds = elapsedSeconds;
    m_session.setDuration(elapsedSeconds);

    for (const auto& detector : m_registry.getDetectors()) {
        auto events = detector->processLine(line, m_context);
        for (const auto& ev : events) {
            // Record event inside active session
            m_session.addEvent(ev);
            
            // Dispatch signal notifications
            if (m_dispatcher) {
                m_dispatcher->dispatchEvent(ev);
            }

            // Route state machines checkpoints mapping
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
