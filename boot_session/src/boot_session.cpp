#include "boot_session/boot_session.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <filesystem>

void BootSession::addEvent(const BootEvent& event) {
    m_timeline.push_back(event);
    switch (event.severity) {
        case EventSeverity::Info:
            m_infoCount++;
            break;
        case EventSeverity::Warning:
            m_warningCount++;
            break;
        case EventSeverity::Error:
            m_errorCount++;
            break;
        case EventSeverity::Critical:
            m_panicCount++;
            break;
    }
}

void BootSession::writeLogFile(const std::string& logsDir, const std::string& fullLogText) const {
    try {
        if (!std::filesystem::exists(logsDir)) {
            std::filesystem::create_directories(logsDir);
        }

        auto now = std::chrono::system_clock::now();
        auto timeT = std::chrono::system_clock::to_time_t(now);
        std::tm tm;
#if defined(_WIN32)
        localtime_s(&tm, &timeT);
#else
        localtime_r(&timeT, &tm);
#endif

        std::stringstream ss;
        // Clean name to be safe filename-friendly
        std::string cleanName = m_romName;
        for (char& c : cleanName) {
            if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
                c = '_';
            }
        }

        ss << "boot_" << cleanName << "_" 
           << std::put_time(&tm, "%Y-%m-%d_%H-%M-%S") << ".log";
        std::filesystem::path filePath = std::filesystem::path(logsDir) / ss.str();

        std::ofstream out(filePath);
        if (out) {
            out << "==================================================\n";
            out << "BOOT SESSION STATISTICS\n";
            out << "==================================================\n";
            out << generateStats() << "\n";
            out << "==================================================\n";
            out << "BOOT TIMELINE CHRONOLOGY\n";
            out << "==================================================\n";
            for (const auto& ev : m_timeline) {
                std::string sevStr;
                switch (ev.severity) {
                    case EventSeverity::Info:     sevStr = "INFO"; break;
                    case EventSeverity::Warning:  sevStr = "WARNING"; break;
                    case EventSeverity::Error:    sevStr = "ERROR"; break;
                    case EventSeverity::Critical: sevStr = "CRITICAL"; break;
                }
                out << "[" << std::fixed << std::setprecision(3) << ev.elapsedSeconds << "s] "
                    << "[" << sevStr << "] " << ev.details << " (Confidence: " 
                    << std::fixed << std::setprecision(0) << (ev.confidence * 100.0) << "%)\n";
                if (!ev.metadata.empty()) {
                    for (const auto& [k, v] : ev.metadata) {
                        out << "   - " << k << ": " << v << "\n";
                    }
                }
            }
            out << "==================================================\n";
            out << "RAW GUEST KERNEL CONSOLE OUTPUT\n";
            out << "==================================================\n";
            out << fullLogText;
            out.close();
        }
    } catch (...) {
        // Suppress and fallback
    }
}

std::string BootSession::generateStats() const {
    std::stringstream ss;
    ss << "ROM Name:          " << m_romName << "\n"
       << "Kernel Version:    " << m_kernelVersion << "\n"
       << "Boot Duration:     " << std::fixed << std::setprecision(2) << m_durationSeconds << " seconds\n"
       << "Timeline Events:   " << m_timeline.size() << "\n"
       << "Warnings Recorded: " << m_warningCount << "\n"
       << "Errors Recorded:   " << m_errorCount << "\n"
       << "Panics Recorded:   " << m_panicCount << "\n"
       << "Final Boot Result: " << m_finalResult;
    return ss.str();
}
