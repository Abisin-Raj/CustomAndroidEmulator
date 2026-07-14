#pragma once

#include <memory>
#include <string>
#include "detector_registry.h"
#include "boot_events/event_dispatcher.h"
#include "boot_session/boot_session.h"
#include "kernel_analysis/symbol_resolver.h"

class BootPipelineManager {
public:
    explicit BootPipelineManager(EventDispatcher* dispatcher);
    ~BootPipelineManager() = default;

    // Registers all default boot detectors automatically
    void initializeDefaultDetectors();

    // Load kernel symbols from ROM folder or reference libraries
    bool loadSymbols(const std::string& romDir);

    // Process a single guest log line with its exact elapsed timestamp, returning annotated output
    std::string processLogLine(const std::string& line, double elapsedSeconds);

    // Getters
    const DetectorContext& getContext() const { return m_context; }
    const BootSession& getSession() const { return m_session; }
    BootSession& getSession() { return m_session; }
    EventDispatcher* getDispatcher() const { return m_dispatcher; }
    kernel_analysis::SymbolResolver& getResolver() { return m_resolver; }
    const kernel_analysis::SymbolResolver& getResolver() const { return m_resolver; }

    void setRomName(const std::string& name);

private:
    void transitionState(BootState newState);

    DetectorContext m_context;
    DetectorRegistry m_registry;
    EventDispatcher* m_dispatcher = nullptr;
    BootSession m_session;
    kernel_analysis::SymbolResolver m_resolver;
};
