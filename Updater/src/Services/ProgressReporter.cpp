// Copyright 2024 <github.com/razaqq>

#include "Updater/Services/ProgressReporter.hpp"

using namespace PotatoAlert::Updater;

ProgressReporter::ProgressReporter()
    : m_lastCallbackTime(std::chrono::steady_clock::now())
{
}

void ProgressReporter::SetProgressCallback(ProgressCallback callback)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_callback = std::move(callback);
}

void ProgressReporter::ReportProgress(const ProgressInfo& progress)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_currentProgress = progress;
    CallCallback(progress);
}

void ProgressReporter::ReportState(UpdateState state, const std::string& message)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_currentProgress.CurrentState = state;
    m_currentProgress.StatusMessage = message;
    m_currentProgress.Timestamp = std::chrono::system_clock::now();
    
    CallCallback(m_currentProgress);
}

void ProgressReporter::ReportDownloadProgress(std::uint64_t bytesDownloaded, std::uint64_t totalBytes, double speed)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_currentProgress.BytesDownloaded = bytesDownloaded;
    m_currentProgress.TotalBytes = totalBytes;
    m_currentProgress.DownloadSpeedBytesPerSecond = speed;
    m_currentProgress.Timestamp = std::chrono::system_clock::now();
    
    // Calculate ETA if we have speed information
    if (speed > 0.0 && totalBytes > bytesDownloaded)
    {
        const double remainingBytes = static_cast<double>(totalBytes - bytesDownloaded);
        const double eta = remainingBytes / speed;
        m_currentProgress.EstimatedTimeRemaining = std::chrono::seconds(static_cast<std::int64_t>(eta));
    }
    
    CallCallback(m_currentProgress);
}

void ProgressReporter::ReportPercentComplete(int percent, const std::string& message)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_currentProgress.PercentComplete = percent;
    if (!message.empty())
    {
        m_currentProgress.StatusMessage = message;
    }
    m_currentProgress.Timestamp = std::chrono::system_clock::now();
    
    CallCallback(m_currentProgress);
}

ProgressInfo ProgressReporter::GetCurrentProgress() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentProgress;
}

void ProgressReporter::SetThrottleInterval(std::chrono::milliseconds interval)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_throttleInterval = interval;
}

void ProgressReporter::CallCallback(const ProgressInfo& progress)
{
    if (!m_callback)
        return;
    
    if (ShouldThrottle())
        return;
    
    m_lastCallbackTime = std::chrono::steady_clock::now();
    
    // Call callback without holding the lock to prevent deadlocks
    auto callback = m_callback;
    m_mutex.unlock();
    callback(progress);
    m_mutex.lock();
}

bool ProgressReporter::ShouldThrottle() const
{
    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastCallbackTime);
    return elapsed < m_throttleInterval;
}
