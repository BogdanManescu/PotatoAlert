// Copyright 2024 <github.com/razaqq>
#pragma once

#include "Updater/Interfaces/IProgressReporter.hpp"

#include <mutex>


namespace PotatoAlert::Updater {

class ProgressReporter : public IProgressReporter
{
public:
    ProgressReporter();
    ~ProgressReporter() override = default;

    // IProgressReporter implementation
    void SetProgressCallback(ProgressCallback callback) override;
    void ReportProgress(const ProgressInfo& progress) override;
    void ReportState(UpdateState state, const std::string& message = "") override;
    void ReportDownloadProgress(std::uint64_t bytesDownloaded, std::uint64_t totalBytes, double speed = 0.0) override;
    void ReportPercentComplete(int percent, const std::string& message = "") override;

    // Additional functionality
    ProgressInfo GetCurrentProgress() const;
    void SetThrottleInterval(std::chrono::milliseconds interval);

private:
    void CallCallback(const ProgressInfo& progress);
    bool ShouldThrottle() const;

    mutable std::mutex m_mutex;
    ProgressCallback m_callback;
    ProgressInfo m_currentProgress;
    std::chrono::milliseconds m_throttleInterval{100}; // Limit callbacks to once per 100ms
    std::chrono::steady_clock::time_point m_lastCallbackTime;
};

}  // namespace PotatoAlert::Updater
