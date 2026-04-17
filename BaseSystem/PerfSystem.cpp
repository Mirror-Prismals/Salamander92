#pragma once
#include "Host/PlatformInput.h"
#include "../Host.h"

namespace PerfSystemLogic {
    namespace {
        void loadPerfConfig(PerfContext& perf) {
            std::ifstream f("Host/perf.json");
            if (!f.is_open()) {
                std::cerr << "PerfSystem: could not open Host/perf.json" << std::endl;
                perf.enabled = false;
                perf.configLoaded = true;
                return;
            }
            json data;
            try {
                data = json::parse(f);
            } catch (...) {
                std::cerr << "PerfSystem: failed to parse Host/perf.json" << std::endl;
                perf.enabled = false;
                perf.configLoaded = true;
                return;
            }
            perf.enabled = data.value("enabled", false);
            perf.reportInterval = data.value("intervalSeconds", 1.0);
            perf.hitchThresholdMs = data.value("hitchThresholdMs", 16.0);
            perf.frameHitchThresholdMs = data.value(
                "frameHitchThresholdMs",
                std::max(24.0, perf.hitchThresholdMs * 2.0)
            );
            perf.frameHitchMinReportIntervalSeconds = std::max(
                0.0,
                data.value("frameHitchMinReportIntervalSeconds", 0.5)
            );
            perf.frameHitchTopSteps = std::max(1, data.value("frameHitchTopSteps", 6));
            perf.framePacingEnabled = data.value("framePacingEnabled", perf.framePacingEnabled);
            perf.framePacingReportIntervalSeconds = std::max(
                0.1,
                data.value(
                    "framePacingReportIntervalSeconds",
                    perf.framePacingReportIntervalSeconds
                )
            );
            perf.framePacingTopPeaks = std::max(
                0,
                data.value("framePacingTopPeaks", perf.framePacingTopPeaks)
            );
            perf.framePacingTopStepsPerPeak = std::max(
                1,
                data.value("framePacingTopStepsPerPeak", perf.framePacingTopStepsPerPeak)
            );
            perf.allowlist.clear();
            if (data.contains("allowlist") && data["allowlist"].is_array()) {
                for (const auto& entry : data["allowlist"]) {
                    if (entry.is_string()) {
                        perf.allowlist.insert(entry.get<std::string>());
                    }
                }
            }
            perf.configLoaded = true;
        }
    }

    void UpdatePerf(BaseSystem& baseSystem, std::vector<Entity>& prototypes, float dt, PlatformWindowHandle win) {
        (void)prototypes; (void)dt; (void)win;
        if (!baseSystem.perf) return;
        PerfContext& perf = *baseSystem.perf;
        if (!perf.configLoaded) {
            loadPerfConfig(perf);
        }
        if (!perf.enabled) return;

        double now = PlatformInput::GetTimeSeconds();
        if (perf.lastReportTime <= 0.0) {
            perf.lastReportTime = now;
            return;
        }
        double elapsed = now - perf.lastReportTime;
        if (elapsed < perf.reportInterval) return;

        double fps = elapsed > 0.0 ? static_cast<double>(perf.frameCount) / elapsed : 0.0;
        std::cout << "[Perf] " << perf.frameCount << " frames in "
                  << elapsed << "s (~" << fps << " fps)" << std::endl;

        std::vector<std::pair<std::string, double>> sortedTotals;
        sortedTotals.reserve(perf.totalsMs.size());
        for (const auto& [name, total] : perf.totalsMs) {
            sortedTotals.emplace_back(name, total);
        }
        std::sort(sortedTotals.begin(), sortedTotals.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });
        for (const auto& entry : sortedTotals) {
            int count = perf.counts.count(entry.first) ? perf.counts[entry.first] : 0;
            double avg = count > 0 ? entry.second / static_cast<double>(count) : 0.0;
            double maxMs = perf.maxMs.count(entry.first) ? perf.maxMs[entry.first] : 0.0;
            int hitches = perf.hitchCounts.count(entry.first) ? perf.hitchCounts[entry.first] : 0;
            std::cout << "[Perf] " << entry.first << ": total "
                      << entry.second << " ms, avg " << avg << " ms"
                      << ", max " << maxMs << " ms, hitches " << hitches << std::endl;
        }

        perf.totalsMs.clear();
        perf.maxMs.clear();
        perf.counts.clear();
        perf.hitchCounts.clear();
        perf.frameCount = 0;
        perf.lastReportTime = now;
    }
}
