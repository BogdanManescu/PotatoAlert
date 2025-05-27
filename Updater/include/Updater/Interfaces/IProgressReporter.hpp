// Copyright 2024 <github.com/razaqq>
#pragma once

#include "Updater/Models/ProgressInfo.hpp"

#include <functional>


namespace PotatoAlert::Updater {

class IProgressReporter
{
public:
    virtual ~IProgressReporter() = default;

    using ProgressCallback = std::function<void(const ProgressInfo&)>;

    virtual void SetProgressCallback(ProgressCallback callback) = 0;
    virtual void ReportProgress(const ProgressInfo& progress) = 0;
    virtual void ReportState(UpdateState state, const std::string& message = "") = 0;
    virtual void ReportDownloadProgress(std::uint64_t bytesDownloaded, std::uint64_t totalBytes, double speed = 0.0) = 0;
    virtual void ReportPercentComplete(int percent, const std::string& message = "") = 0;
};

}  // namespace PotatoAlert::Updater
