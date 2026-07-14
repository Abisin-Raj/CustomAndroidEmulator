#pragma once

#include <memory>
#include <string>
#include "detector_registry.h"
#include "boot_events/event_dispatcher.h"
#include "boot_session/boot_session.h"

class BootPipelineManager {
public:
    explicit BootPipelineManager(EventDispatcher* dispatcher);
    ~BootPipelineManager() = default;

    // Registers all default boot detectors automatically
    void initializeDefaultDetectors();

    // Process a single guest log line with its exact elapsed timestamp
    void processLogLine(const std::string& line, double elapsedSeconds);

    // Getters
    const DetectorContext& getContext() const { return m_context; }
    const BootSession& getSession() const { return m_session; }
    BootSession& getSession() { return m_session; }
    EventDispatcher* getDispatcher() const { return m_dispatcher; }

    void setRomName(const std::string& name);

private:
    void transitionState(BootState newState);

    DetectorContext m_context;
    DetectorRegistry m_registry;
    EventDispatcher* m_dispatcher = nullptr;
    BootSession m_session;
};
