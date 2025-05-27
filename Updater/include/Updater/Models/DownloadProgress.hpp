// Copyright 2024 <github.com/razaqq>
#pragma once

#include <chrono>
#include <cstdint>


namespace PotatoAlert::Updater {

class DownloadProgress
{
public:
    std::uint64_t BytesReceived = 0;
    std::uint64_t TotalBytes = 0;
    double SpeedBytesPerSecond = 0.0;
    std::chrono::steady_clock::time_point StartTime;
    std::chrono::steady_clock::time_point LastUpdateTime;
    
    DownloadProgress()
        : StartTime(std::chrono::steady_clock::now())
        , LastUpdateTime(StartTime)
    {
    }

    void Update(std::uint64_t bytesReceived, std::uint64_t totalBytes)
    {
        const auto now = std::chrono::steady_clock::now();
        const auto timeDiff = std::chrono::duration_cast<std::chrono::milliseconds>(now - LastUpdateTime);
        
        if (timeDiff.count() > 0)
        {
            const std::uint64_t bytesDiff = bytesReceived - BytesReceived;
            SpeedBytesPerSecond = static_cast<double>(bytesDiff) / (timeDiff.count() / 1000.0);
        }
        
        BytesReceived = bytesReceived;
        TotalBytes = totalBytes;
        LastUpdateTime = now;
    }

    [[nodiscard]] double GetProgressPercent() const
    {
        if (TotalBytes == 0)
            return 0.0;
        
        return (static_cast<double>(BytesReceived) / TotalBytes) * 100.0;
    }

    [[nodiscard]] std::chrono::seconds GetEstimatedTimeRemaining() const
    {
        if (SpeedBytesPerSecond <= 0.0 || BytesReceived >= TotalBytes)
            return std::chrono::seconds{0};
        
        const std::uint64_t remainingBytes = TotalBytes - BytesReceived;
        const double remainingSeconds = remainingBytes / SpeedBytesPerSecond;
        
        return std::chrono::seconds{static_cast<std::int64_t>(remainingSeconds)};
    }

    [[nodiscard]] std::chrono::seconds GetElapsedTime() const
    {
        const auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::seconds>(now - StartTime);
    }
};

}  // namespace PotatoAlert::Updater
