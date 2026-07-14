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

void BootSession::writeJsonSummary(const std::string& logsDir) const {
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

        std::stringstream fileSS;
        std::string cleanName = m_romName;
        for (char& c : cleanName) {
            if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
                c = '_';
            }
        }

        char timeStr[64];
        std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%dT%H:%M:%S", &tm);
        
        char fileTimeStr[64];
        std::strftime(fileTimeStr, sizeof(fileTimeStr), "%Y-%m-%d_%H-%M-%S", &tm);

        fileSS << "boot_" << cleanName << "_" << fileTimeStr << ".json";
        std::filesystem::path filePath = std::filesystem::path(logsDir) / fileSS.str();

        // Safe JSON string escaping for panic reason
        std::string escapedPanic = m_panicReason;
        std::string safePanic;
        for (char c : escapedPanic) {
            if (c == '"') safePanic += "\\\"";
            else if (c == '\\') safePanic += "\\\\";
            else if (c == '\n') safePanic += "\\n";
            else if (c == '\r') safePanic += "\\r";
            else if (c == '\t') safePanic += "\\t";
            else safePanic += c;
        }

        std::ofstream out(filePath);
        if (out) {
            out << "{\n"
                << "  \"rom\": \"" << m_romName << "\",\n"
                << "  \"timestamp\": \"" << timeStr << "\",\n"
                << "  \"result\": \"" << m_finalResult << "\",\n"
                << "  \"last_state\": \"" << m_lastState << "\",\n"
                << "  \"duration_ms\": " << static_cast<long long>(m_durationSeconds * 1000.0) << ",\n"
                << "  \"panic_reason\": \"" << safePanic << "\"\n"
                << "}\n";
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
