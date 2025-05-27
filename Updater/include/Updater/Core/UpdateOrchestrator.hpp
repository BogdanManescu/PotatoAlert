// Copyright 2024 <github.com/razaqq>
#pragma once

#include "Updater/Models/UpdateResult.hpp"
#include "Updater/Models/UpdateInfo.hpp"
#include "Updater/Models/UpdateConfiguration.hpp"
#include "Updater/Models/ProgressInfo.hpp"
#include "Updater/Models/UpdateError.hpp"

#include "Updater/Interfaces/IDownloader.hpp"
#include "Updater/Interfaces/IFileManager.hpp"
#include "Updater/Interfaces/IBackupManager.hpp"
#include "Updater/Interfaces/IProcessManager.hpp"
#include "Updater/Interfaces/IUpdateVerifier.hpp"
#include "Updater/Interfaces/IProgressReporter.hpp"
#include "Updater/Interfaces/IPlatformManager.hpp"

#include <functional>
#include <future>
#include <memory>
#include <atomic>


namespace PotatoAlert::Updater {

class UpdateOrchestrator
{
public:
    using ProgressCallback = std::function<void(const ProgressInfo&)>;
    using ErrorCallback = std::function<void(const UpdateError&)>;

    UpdateOrchestrator(
        std::shared_ptr<IPlatformManager> platformManager,
        std::shared_ptr<IDownloader> downloader,
        std::shared_ptr<IFileManager> fileManager,
        std::shared_ptr<IBackupManager> backupManager,
        std::shared_ptr<IProcessManager> processManager,
        std::shared_ptr<IUpdateVerifier> verifier,
        std::shared_ptr<IProgressReporter> progressReporter
    );

    ~UpdateOrchestrator();

    // Configuration
    void SetConfiguration(const UpdateConfiguration& config);
    const UpdateConfiguration& GetConfiguration() const;

    // Callbacks
    void SetProgressCallback(ProgressCallback callback);
    void SetErrorCallback(ErrorCallback callback);

    // Main update operations
    std::future<std::pair<UpdateResult, std::optional<UpdateInfo>>> CheckForUpdates();
    std::future<UpdateResult> PerformUpdate(const UpdateInfo& updateInfo);
    std::future<UpdateResult> RollbackUpdate();

    // Individual steps (for advanced usage)
    std::future<UpdateResult> DownloadUpdate(const UpdateInfo& updateInfo);
    std::future<UpdateResult> VerifyUpdate(const UpdateInfo& updateInfo);
    std::future<UpdateResult> CreateBackup();
    std::future<UpdateResult> InstallUpdate(const UpdateInfo& updateInfo);
    std::future<UpdateResult> Finalize();

    // Control operations
    void Cancel();
    void Pause();   // For downloads
    void Resume();  // For downloads

    // State queries
    UpdateState GetCurrentState() const;
    bool IsOperationInProgress() const;
    bool IsCancellationRequested() const;
    std::optional<UpdateError> GetLastError() const;

    // Utility methods
    bool IsUpdateAvailable() const;
    std::filesystem::path GetUpdateCacheDirectory() const;
    void CleanupCache();

private:
    // Internal state management
    void SetState(UpdateState state, const std::string& message = "");
    void ReportError(const UpdateError& error);
    void ReportProgress(const ProgressInfo& progress);

    // Update workflow steps
    UpdateResult PrepareForUpdate();
    UpdateResult ValidateConfiguration();
    UpdateResult CheckSystemRequirements();
    UpdateResult StopRunningApplications();
    UpdateResult PerformInstallation(const UpdateInfo& updateInfo);
    UpdateResult RestartApplications();
    UpdateResult Cleanup();

    // Member variables
    std::shared_ptr<IPlatformManager> m_platformManager;
    std::shared_ptr<IDownloader> m_downloader;
    std::shared_ptr<IFileManager> m_fileManager;
    std::shared_ptr<IBackupManager> m_backupManager;
    std::shared_ptr<IProcessManager> m_processManager;
    std::shared_ptr<IUpdateVerifier> m_verifier;
    std::shared_ptr<IProgressReporter> m_progressReporter;

    UpdateConfiguration m_config;
    
    std::atomic<UpdateState> m_currentState{UpdateState::Idle};
    std::atomic<bool> m_cancellationRequested{false};
    std::atomic<bool> m_operationInProgress{false};
    
    ProgressCallback m_progressCallback;
    ErrorCallback m_errorCallback;
    
    std::optional<UpdateError> m_lastError;
    std::optional<UpdateInfo> m_currentUpdateInfo;
    std::filesystem::path m_backupPath;
    
    mutable std::mutex m_stateMutex;
};

}  // namespace PotatoAlert::Updater
