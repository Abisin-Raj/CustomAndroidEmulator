#include "behavior_exploration_tool/bet_system.h"
#include <QProcess>
#include <QDateTime>
#include <QDir>
#include <spdlog/spdlog.h>
#include <fstream>
#include <sstream>
#include <chrono>

namespace bet {

BetSystem& BetSystem::instance() {
    static BetSystem inst;
    return inst;
}

void BetSystem::initialize(const std::string& dbDirectory) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized) return;

    m_dbDir = dbDirectory;
    QDir().mkpath(QString::fromStdString(m_dbDir));
    
    std::string dbFile = (QDir(QString::fromStdString(m_dbDir)).filePath("bet_experiments.json")).toStdString();
    
    m_database = std::make_unique<ExperimentDatabase>(dbFile);
    m_recorder = std::make_unique<EventRecorder>();
    m_timeline = std::make_unique<BootTimeline>();
    m_analyzer = std::make_unique<ProgressAnalyzer>();
    m_comparator = std::make_unique<ComparisonEngine>();
    m_blockerDetector = std::make_unique<BlockerDetector>();
    m_reportGenerator = std::make_unique<ReportGenerator>();
    
    std::string overridesFile = (QDir(QString::fromStdString(m_dbDir)).filePath("bet_overrides.json")).toStdString();
    m_overrideEngine = std::make_unique<OverrideEngine>();
    m_overrideEngine->loadFromFile(overridesFile);

    if (m_database->load()) {
        spdlog::info("[BET] BetSystem initialized successfully. DB located at: {}", dbFile);
    } else {
        spdlog::error("[BET] BetSystem: Failed to load database file: {}", dbFile);
    }
    m_initialized = true;
}

std::string BetSystem::getGitCommitHash() const {
    QProcess process;
    process.start("git", QStringList() << "rev-parse" << "--short" << "HEAD");
    if (process.waitForFinished(1000)) {
        QString output = process.readAllStandardOutput().trimmed();
        if (!output.isEmpty()) {
            return output.toStdString();
        }
    }
    return "unknown";
}

std::string BetSystem::getIsoTimestamp() const {
    return QDateTime::currentDateTime().toString(Qt::ISODate).toStdString();
}

bool BetSystem::isOverridesEnabled() const {
    // Emergency switch: if environment variable BET_DISABLE_OVERRIDES=1, disable all overrides.
    QByteArray env = qgetenv("BET_DISABLE_OVERRIDES");
    if (env == "1") {
        return false;
    }
    return true;
}

bool BetSystem::checkOverride(const std::string& category, const std::string& name, 
                              std::string& outRuleId, std::string& outDesc, 
                              std::map<std::string, uint64_t>& outOverrides) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized || !isOverridesEnabled() || !m_overrideEngine) {
        return false;
    }

    return m_overrideEngine->findOverride(category, name, m_activeRomName, m_activeKernelVersion, m_activeBootId, outRuleId, outDesc, outOverrides);
}

void BetSystem::startSession(const std::string& romName, const std::string& kernelVersion, const std::string& emulatorVersion) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) {
        spdlog::error("[BET] BetSystem: startSession called before initialize!");
        return;
    }

    m_recorder->clear();
    m_timeline->clear();
    m_analyzer->clear();
    m_blockerDetector->clear();
    m_latestReportMarkdown.clear();

    m_activeRomName = romName;
    m_activeKernelVersion = kernelVersion;
    m_activeEmulatorVersion = emulatorVersion;
    m_activeBootId = m_database->getNextBootId();

    m_sessionStartTime = static_cast<double>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count()
    ) / 1000.0;

    BetEvent startEvent;
    startEvent.bootId = m_activeBootId;
    startEvent.elapsedSeconds = 0.0;
    startEvent.timestamp = getIsoTimestamp();
    startEvent.type = BetEventType::BootStart;
    startEvent.category = "System";
    startEvent.name = "boot_start";
    startEvent.details = "VM startup triggered for ROM: " + romName;
    
    m_recorder->recordEvent(startEvent);
    spdlog::info("[BET] Started boot session #{} for ROM '{}'", m_activeBootId, romName);
}

void BetSystem::registerIntercept(const std::string& category, const std::string& name, uint64_t pc, uint64_t lr, uint64_t sp, 
                                 const std::vector<uint64_t>& before, const std::vector<uint64_t>& dispatcher, const std::vector<uint64_t>& after, 
                                 bool handled, const std::string& overrideRuleId,
                                 const std::map<std::string, std::string>& metadata) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) return;

    double now = static_cast<double>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count()
    ) / 1000.0;
    double elapsed = now - m_sessionStartTime;

    BetEvent ev;
    ev.bootId = m_activeBootId;
    ev.elapsedSeconds = elapsed;
    ev.timestamp = getIsoTimestamp();
    ev.type = BetEventType::GuestIntercept;
    ev.category = category;
    ev.name = name;
    ev.pc = pc;
    ev.lr = lr;
    ev.sp = sp;
    ev.registersBefore = before;
    ev.registersDispatcher = dispatcher;
    ev.registersAfter = after;
    ev.handled = handled;
    ev.overrideRuleId = overrideRuleId;
    ev.metadata = metadata;
    
    if (!overrideRuleId.empty()) {
        ev.metadata["override_applied"] = "true";
        ev.metadata["override_rule_id"] = overrideRuleId;
    }
    
    m_recorder->recordEvent(ev);
}

void BetSystem::pushLogLine(const std::string& logLine) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) return;

    double now = static_cast<double>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count()
    ) / 1000.0;
    double elapsed = now - m_sessionStartTime;

    BetEvent ev;
    ev.bootId = m_activeBootId;
    ev.elapsedSeconds = elapsed;
    ev.timestamp = getIsoTimestamp();
    ev.type = BetEventType::SerialLogLine;
    ev.category = "SERIAL";
    ev.name = "serial_out";
    ev.details = logLine;

    m_recorder->recordEvent(ev);
}

void BetSystem::endSession(const std::string& outcomeMessage) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized || m_activeBootId == 0) return;

    double now = static_cast<double>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count()
    ) / 1000.0;
    double elapsed = now - m_sessionStartTime;

    BetEvent endEvent;
    endEvent.bootId = m_activeBootId;
    endEvent.elapsedSeconds = elapsed;
    endEvent.timestamp = getIsoTimestamp();
    endEvent.type = BetEventType::BootEnd;
    endEvent.category = "System";
    endEvent.name = "boot_end";
    endEvent.details = outcomeMessage;
    
    m_recorder->recordEvent(endEvent);

    // Build timeline and analysis
    const auto& events = m_recorder->getEvents();
    m_timeline->buildTimeline(events);
    m_analyzer->analyzeProgress(events);
    m_blockerDetector->detectBlockers(events);

    // Pull previous baseline run from DB
    ExperimentRun prevRun;
    bool hasBaseline = m_database->getLatestRunForRom(m_activeRomName, prevRun);
    
    ComparisonReport comp;
    if (hasBaseline) {
        comp = m_comparator->compareRuns(m_analyzer->getReport(), prevRun.progress);
    } else {
        comp.hasPreviousRun = false;
    }

    // Generate markdown report
    m_latestReportMarkdown = m_reportGenerator->generateMarkdownReport(
        m_activeBootId,
        m_activeRomName,
        getGitCommitHash(),
        m_activeEmulatorVersion,
        m_analyzer->getReport(),
        m_blockerDetector->getObservations(),
        comp
    );

    // Define export filenames
    QDir dbDir(QString::fromStdString(m_dbDir));
    QString bootFolder = QString("boot_%1").arg(m_activeBootId);
    dbDir.mkdir(bootFolder);
    QDir runDir(dbDir.filePath(bootFolder));

    std::string eventsFile = runDir.filePath("events_trace.json").toStdString();
    m_recorder->saveToJsonFile(eventsFile);

    std::string reportFile = runDir.filePath("report.md").toStdString();
    std::ofstream rOut(reportFile);
    if (rOut.is_open()) {
        rOut << m_latestReportMarkdown;
        rOut.close();
    }

    std::string timelineFile = runDir.filePath("timeline.txt").toStdString();
    std::ofstream tOut(timelineFile);
    if (tOut.is_open()) {
        tOut << m_timeline->generateTimelineText();
        tOut.close();
    }

    // Create database run record
    ExperimentRun run;
    run.bootId = m_activeBootId;
    run.timestamp = getIsoTimestamp();
    run.gitCommit = getGitCommitHash();
    run.emulatorVersion = m_activeEmulatorVersion;
    run.romName = m_activeRomName;
    run.kernelVersion = m_activeKernelVersion;
    run.progress = m_analyzer->getReport();
    run.timeline = m_timeline->getTimelineEvents();
    run.reportMarkdown = m_latestReportMarkdown;
    run.eventsFilePath = eventsFile;
    run.serialLogFilePath = ""; // Optional if we want to save full raw console log

    m_database->addRun(run);
    m_database->save();

    spdlog::info("[BET] Session #{} ended. Report saved to: {}", m_activeBootId, reportFile);
    m_activeBootId = 0;
}

} // namespace bet
