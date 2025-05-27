// Copyright 2024 <github.com/razaqq>
#pragma once

#include "Updater/Models/UpdateInfo.hpp"
#include "Updater/Models/UpdateResult.hpp"
#include "Updater/Models/UpdateConfiguration.hpp"
#include "IProgressReporter.hpp"

#include <filesystem>
#include <future>
#include <memory>


namespace PotatoAlert::Updater {

class IDownloader
{
public:
    virtual ~IDownloader() = default;

    virtual void SetProgressReporter(std::shared_ptr<IProgressReporter> reporter) = 0;
    
    // Check if update is available
    virtual std::future<std::pair<UpdateResult, std::optional<UpdateInfo>>> CheckForUpdates(
        const UpdateConfiguration& config) = 0;
    
    // Download update file
    virtual std::future<UpdateResult> DownloadUpdate(
        const UpdateInfo& updateInfo, 
        const std::filesystem::path& destination,
        const UpdateConfiguration& config) = 0;
    
    // Cancel ongoing operation
    virtual void Cancel() = 0;
    
    // Check if operation is in progress
    virtual bool IsDownloading() const = 0;
};

}  // namespace PotatoAlert::Updater
