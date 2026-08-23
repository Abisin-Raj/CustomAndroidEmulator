#pragma once
#include <string>
#include <map>
#include <vector>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace emulator::logger::models {

struct PhaseDuration {
    std::string phase_name;
    uint64_t duration_ms{0};
    bool completed{false};
};

struct SyscallStats {
    uint32_t nr{0};
    std::string name;
    uint64_t count{0};
    double total_duration_ms{0.0};
    double max_duration_ms{0.0};
};

struct Metrics {
    uint64_t boot_time_ms{0};
    uint64_t kernel_time_ms{0};
    uint64_t first_stage_ms{0};
    uint64_t second_stage_ms{0};
    uint64_t zygote_ms{0};
    uint64_t surfaceflinger_ms{0};
    uint64_t syscalls{0};
    uint64_t binder_transactions{0};
    uint64_t smc_calls{0};
    uint64_t smc_failures{0};
    uint64_t smc_retry_loops{0};
    double avg_smc_latency_us{0.0};
    uint64_t ufs_reads{0};
    uint64_t total_errors{0};
    uint64_t total_warnings{0};
    uint64_t total_gdb_hits{0};
    double peak_memory_mb{0.0};
    double avg_cpu_usage_pct{0.0};

    std::map<std::string, uint64_t> top_smc_opcodes;
    std::map<std::string, PhaseDuration> phase_map;
    std::map<uint32_t, SyscallStats> syscall_stats;

    nlohmann::json ToJson() const {
        nlohmann::json j;
        j["boot_time_ms"] = boot_time_ms;
        j["kernel_time_ms"] = kernel_time_ms;
        j["first_stage_ms"] = first_stage_ms;
        j["second_stage_ms"] = second_stage_ms;
        j["zygote_ms"] = zygote_ms;
        j["surfaceflinger_ms"] = surfaceflinger_ms;
        j["syscalls"] = syscalls;
        j["binder_transactions"] = binder_transactions;
        j["smc_calls"] = smc_calls;
        j["smc_failures"] = smc_failures;
        j["smc_retry_loops"] = smc_retry_loops;
        j["avg_smc_latency_us"] = avg_smc_latency_us;
        j["top_smc_opcodes"] = top_smc_opcodes;
        j["ufs_reads"] = ufs_reads;
        j["total_errors"] = total_errors;
        j["total_warnings"] = total_warnings;
        j["total_gdb_hits"] = total_gdb_hits;
        j["peak_memory_mb"] = peak_memory_mb;
        j["avg_cpu_usage_pct"] = avg_cpu_usage_pct;


        nlohmann::json phases = nlohmann::json::object();
        for (const auto& [name, p] : phase_map) {
            phases[name] = {
                {"duration_ms", p.duration_ms},
                {"completed", p.completed}
            };
        }
        j["phases"] = phases;

        nlohmann::json syscall_obj = nlohmann::json::object();
        for (const auto& [nr, s] : syscall_stats) {
            syscall_obj[std::to_string(nr)] = {
                {"name", s.name},
                {"count", s.count},
                {"total_duration_ms", s.total_duration_ms},
                {"max_duration_ms", s.max_duration_ms}
            };
        }
        j["syscall_stats"] = syscall_obj;

        return j;
    }
};

} // namespace emulator::logger::models
