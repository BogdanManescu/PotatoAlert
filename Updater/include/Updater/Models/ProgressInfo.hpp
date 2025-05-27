// Copyright 2024 <github.com/razaqq>
#pragma once

#include "UpdateResult.hpp"

#include <chrono>
#include <cstdint>
#include <string>


namespace PotatoAlert::Updater {

class ProgressInfo
{
public:    UpdateState CurrentState = UpdateState::Idle;
    std::string StatusMessage;
    
    // Additional fields used by implementation
    std::string Stage;
    std::string Message;
    double Progress = 0.0;
    
    // Download progress
    std::optional<std::uint64_t> BytesDownloaded;
    std::optional<std::uint64_t> TotalBytes;
    std::optional<double> DownloadSpeedBytesPerSecond;
    
    // Overall progress
    std::optional<int> PercentComplete;
    
    // Time estimates
    std::optional<std::chrono::seconds> EstimatedTimeRemaining;
    std::chrono::system_clock::time_point Timestamp;

    ProgressInfo() : Timestamp(std::chrono::system_clock::now()) {}

    ProgressInfo(UpdateState state, std::string message)
        : CurrentState(state)
        , StatusMessage(std::move(message))
        , Timestamp(std::chrono::system_clock::now())
    {
    }

    [[nodiscard]] double GetProgressPercent() const
    {
        if (PercentComplete.has_value())
            return PercentComplete.value();
        
        if (BytesDownloaded.has_value() && TotalBytes.has_value() && TotalBytes.value() > 0)
            return (static_cast<double>(BytesDownloaded.value()) / TotalBytes.value()) * 100.0;
        
        return 0.0;
    }

    [[nodiscard]] std::string GetSpeedString() const
    {
        if (!DownloadSpeedBytesPerSecond.has_value())
            return "";
        
        const double speed = DownloadSpeedBytesPerSecond.value();
        if (speed >= 1024 * 1024)
            return std::to_string(speed / (1024 * 1024)) + " MB/s";
        else if (speed >= 1024)
            return std::to_string(speed / 1024) + " KB/s";
        else
            return std::to_string(speed) + " B/s";
    }
};

}  // namespace PotatoAlert::Updater
