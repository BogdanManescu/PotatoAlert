// Copyright 2024 <github.com/razaqq>
#pragma once

#include "UpdateResult.hpp"
#include "Updater/Interfaces/IPlatformManager.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>


namespace PotatoAlert::Updater {

class UpdateConfiguration
{
public:    // Update sources
    std::string UpdateUrl = "https://github.com/razaqq/PotatoAlert/releases/latest/download/{}";
    std::string VersionCheckUrl = "https://api.github.com/repos/razaqq/PotatoAlert/releases/latest";
    UpdateChannel Channel = UpdateChannel::Stable;
    Platform Platform = Platform::Windows;  // Default platform
    
    // Download settings
    std::optional<std::size_t> MaxDownloadSpeedBytesPerSecond;  // No limit if not set
    bool ResumeSupport = true;
    std::size_t MaxRetries = 3;
    std::chrono::seconds NetworkTimeout{30};
    std::chrono::seconds DownloadTimeout{300};
    
    // Security settings
    bool RequireSignatureVerification = true;
    std::vector<std::string> TrustedCertificateFingerprints;
    bool AllowInsecureConnections = false;
    bool VerifyChecksums = true;
    
    // Backup settings
    std::optional<std::filesystem::path> BackupLocation;  // Uses temp directory if not set
    std::size_t MaxBackups = 3;
    bool CompressBackups = false;
    bool VerifyBackups = true;
    
    // Installation settings
    bool RestartAfterUpdate = true;
    std::string RestartArguments;
    bool SilentMode = false;
    bool AutomaticUpdates = false;
    std::chrono::hours UpdateCheckInterval{24};  // Check every 24 hours
    
    // Process management
    bool RequireElevation = true;
    std::chrono::seconds ProcessWaitTimeout{30};
    bool ForceKillProcesses = false;
    
    // Application-specific settings
    std::string ApplicationName = "PotatoAlert";
    std::string UpdaterBinaryName = "PotatoUpdater.exe";
    std::string MainBinaryName = "PotatoAlert.exe";
    std::vector<std::string> FilesToBackup;  // If empty, backup everything
    std::vector<std::string> FilesToIgnore{".trash", ".backup", ".log"};

    [[nodiscard]] std::filesystem::path GetBackupDirectory() const
    {
        if (BackupLocation.has_value())
            return BackupLocation.value();
        
        return std::filesystem::temp_directory_path() / (ApplicationName + "Backup");
    }

    [[nodiscard]] std::string GetUpdateArchiveUrl() const
    {
        const std::string archiveFile = GetArchiveFileName();
        return std::vformat(UpdateUrl, std::make_format_args(archiveFile));
    }

    [[nodiscard]] std::string GetArchiveFileName() const
    {
#ifdef WIN32
        return ApplicationName + ".zip";
#else
        return ApplicationName + "_linux.zip";
#endif
    }

    [[nodiscard]] bool IsValid() const
    {
        return !UpdateUrl.empty() 
            && !VersionCheckUrl.empty() 
            && !ApplicationName.empty()
            && NetworkTimeout.count() > 0
            && DownloadTimeout.count() > 0
            && MaxRetries > 0;
    }
};

}  // namespace PotatoAlert::Updater
