// Copyright 2024 <github.com/razaqq>

#include "Updater/Core/UpdateOrchestrator.hpp"

using namespace PotatoAlert::Updater;

UpdateOrchestrator::UpdateOrchestrator(
    std::shared_ptr<IPlatformManager> platformManager,
    std::shared_ptr<IDownloader> downloader,
    std::shared_ptr<IFileManager> fileManager,
    std::shared_ptr<IBackupManager> backupManager,
    std::shared_ptr<IProcessManager> processManager,
    std::shared_ptr<IUpdateVerifier> verifier,
    std::shared_ptr<IProgressReporter> progressReporter
)
    : m_platformManager(std::move(platformManager))
    , m_downloader(std::move(downloader))
    , m_fileManager(std::move(fileManager))
    , m_backupManager(std::move(backupManager))
    , m_processManager(std::move(processManager))
    , m_verifier(std::move(verifier))
    , m_progressReporter(std::move(progressReporter))
{
    // Set up the downloader with our progress reporter
    if (m_downloader)
    {
        m_downloader->SetProgressReporter(m_progressReporter);
    }
}

UpdateOrchestrator::~UpdateOrchestrator()
{
    Cancel();
}

void UpdateOrchestrator::SetConfiguration(const UpdateConfiguration& config)
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    m_config = config;
}

const UpdateConfiguration& UpdateOrchestrator::GetConfiguration() const
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_config;
}

void UpdateOrchestrator::SetProgressCallback(ProgressCallback callback)
{
    m_progressCallback = std::move(callback);
    
    if (m_progressReporter)
    {
        m_progressReporter->SetProgressCallback([this](const ProgressInfo& progress)
        {
            if (m_progressCallback)
            {
                m_progressCallback(progress);
            }
        });
    }
}

void UpdateOrchestrator::SetErrorCallback(ErrorCallback callback)
{
    m_errorCallback = std::move(callback);
}

std::future<std::pair<UpdateResult, std::optional<UpdateInfo>>> UpdateOrchestrator::CheckForUpdates()
{
    return std::async(std::launch::async, [this]() -> std::pair<UpdateResult, std::optional<UpdateInfo>>
    {
        if (m_operationInProgress.exchange(true))
        {
            return {UpdateResult::InvalidConfiguration, std::nullopt};
        }
        
        SetState(UpdateState::CheckingForUpdates, "Checking for updates...");
        
        try
        {
            auto result = ValidateConfiguration();
            if (result != UpdateResult::Success)
            {
                m_operationInProgress = false;
                SetState(UpdateState::Failed, "Configuration validation failed");
                return {result, std::nullopt};
            }
            
            if (!m_downloader)
            {
                m_operationInProgress = false;
                SetState(UpdateState::Failed, "No downloader available");
                return {UpdateResult::InvalidConfiguration, std::nullopt};
            }
            
            auto updateResult = m_downloader->CheckForUpdates(m_config).get();
            
            if (updateResult.first == UpdateResult::Success && updateResult.second.has_value())
            {
                SetState(UpdateState::UpdateAvailable, "Update available: " + updateResult.second->VersionString);
                m_currentUpdateInfo = updateResult.second;
            }
            else if (updateResult.first == UpdateResult::Success)
            {
                SetState(UpdateState::Complete, "Application is up to date");
            }
            else
            {
                SetState(UpdateState::Failed, "Failed to check for updates");
            }
            
            m_operationInProgress = false;
            return updateResult;
        }
        catch (...)
        {
            m_operationInProgress = false;
            SetState(UpdateState::Failed, "Exception occurred while checking for updates");
            ReportError(UpdateError(UpdateResult::UnknownError, "Exception in CheckForUpdates", std::current_exception()));
            return {UpdateResult::UnknownError, std::nullopt};
        }
    });
}

std::future<UpdateResult> UpdateOrchestrator::PerformUpdate(const UpdateInfo& updateInfo)
{
    return std::async(std::launch::async, [this, updateInfo]() -> UpdateResult
    {
        if (m_operationInProgress.exchange(true))
        {
            return UpdateResult::InvalidConfiguration;
        }
        
        try
        {
            m_currentUpdateInfo = updateInfo;
            m_cancellationRequested = false;
            
            // Validate configuration and system requirements
            auto result = PrepareForUpdate();
            if (result != UpdateResult::Success)
            {
                m_operationInProgress = false;
                return result;
            }
            
            // Download the update
            result = DownloadUpdate(updateInfo).get();
            if (result != UpdateResult::Success || m_cancellationRequested)
            {
                m_operationInProgress = false;
                return m_cancellationRequested ? UpdateResult::Cancelled : result;
            }
            
            // Verify the downloaded update
            result = VerifyUpdate(updateInfo).get();
            if (result != UpdateResult::Success || m_cancellationRequested)
            {
                m_operationInProgress = false;
                return m_cancellationRequested ? UpdateResult::Cancelled : result;
            }
            
            // Create backup
            result = CreateBackup().get();
            if (result != UpdateResult::Success || m_cancellationRequested)
            {
                m_operationInProgress = false;
                return m_cancellationRequested ? UpdateResult::Cancelled : result;
            }
            
            // Stop running applications
            result = StopRunningApplications();
            if (result != UpdateResult::Success || m_cancellationRequested)
            {
                m_operationInProgress = false;
                return m_cancellationRequested ? UpdateResult::Cancelled : result;
            }
            
            // Install the update
            result = InstallUpdate(updateInfo).get();
            if (result != UpdateResult::Success || m_cancellationRequested)
            {
                // Attempt rollback on installation failure
                auto rollbackResult = RollbackUpdate().get();
                m_operationInProgress = false;
                return rollbackResult == UpdateResult::Success ? UpdateResult::RollbackSucceeded : UpdateResult::RollbackFailed;
            }
            
            // Finalize
            result = Finalize().get();
            if (result != UpdateResult::Success)
            {
                m_operationInProgress = false;
                return result;
            }
            
            // Restart applications if configured
            if (m_config.RestartAfterUpdate)
            {
                RestartApplications();
            }
            
            // Cleanup
            Cleanup();
            
            SetState(UpdateState::Complete, "Update completed successfully");
            m_operationInProgress = false;
            return UpdateResult::Success;
        }
        catch (...)
        {
            m_operationInProgress = false;
            SetState(UpdateState::Failed, "Exception occurred during update");
            ReportError(UpdateError(UpdateResult::UnknownError, "Exception in PerformUpdate", std::current_exception()));
            
            // Attempt rollback
            try
            {
                RollbackUpdate().get();
                return UpdateResult::RollbackSucceeded;
            }
            catch (...)
            {
                return UpdateResult::RollbackFailed;
            }
        }
    });
}

std::future<UpdateResult> UpdateOrchestrator::DownloadUpdate(const UpdateInfo& updateInfo)
{
    return std::async(std::launch::async, [this, updateInfo]() -> UpdateResult
    {
        if (!m_downloader)
        {
            return UpdateResult::InvalidConfiguration;
        }
        
        SetState(UpdateState::Downloading, "Downloading update...");
        
        const auto cacheDir = GetUpdateCacheDirectory();
        const auto downloadPath = cacheDir / updateInfo.Version.ToString() / std::filesystem::path(updateInfo.DownloadUrl).filename();
        
        // Create cache directory
        if (m_fileManager && !m_fileManager->DirectoryExists(downloadPath.parent_path()))
        {
            auto result = m_fileManager->CreateDirectory(downloadPath.parent_path());
            if (result != UpdateResult::Success)
            {
                return result;
            }
        }
        
        return m_downloader->DownloadUpdate(updateInfo, downloadPath, m_config).get();
    });
}

void UpdateOrchestrator::Cancel()
{
    m_cancellationRequested = true;
    
    if (m_downloader)
    {
        m_downloader->Cancel();
    }
}

UpdateState UpdateOrchestrator::GetCurrentState() const
{
    return m_currentState.load();
}

bool UpdateOrchestrator::IsOperationInProgress() const
{
    return m_operationInProgress.load();
}

bool UpdateOrchestrator::IsCancellationRequested() const
{
    return m_cancellationRequested.load();
}

std::filesystem::path UpdateOrchestrator::GetUpdateCacheDirectory() const
{
    if (m_platformManager)
    {
        return m_platformManager->GetTempDirectory() / "PotatoAlertUpdates";
    }
    
    return std::filesystem::temp_directory_path() / "PotatoAlertUpdates";
}

void UpdateOrchestrator::SetState(UpdateState state, const std::string& message)
{
    m_currentState = state;
    
    if (m_progressReporter)
    {
        m_progressReporter->ReportState(state, message);
    }
}

void UpdateOrchestrator::ReportError(const UpdateError& error)
{
    m_lastError = error;
    
    if (m_errorCallback)
    {
        m_errorCallback(error);
    }
}

UpdateResult UpdateOrchestrator::PrepareForUpdate()
{
    auto result = ValidateConfiguration();
    if (result != UpdateResult::Success)
        return result;
    
    result = CheckSystemRequirements();
    if (result != UpdateResult::Success)
        return result;
    
    return UpdateResult::Success;
}

UpdateResult UpdateOrchestrator::ValidateConfiguration()
{
    if (!m_config.IsValid())
    {
        ReportError(UpdateError(UpdateResult::InvalidConfiguration, "Invalid update configuration"));
        return UpdateResult::InvalidConfiguration;
    }
    
    return UpdateResult::Success;
}

UpdateResult UpdateOrchestrator::CheckSystemRequirements()
{
    // Check if we have enough disk space
    if (m_platformManager && m_currentUpdateInfo.has_value())
    {
        const auto availableSpace = m_platformManager->GetAvailableDiskSpace(m_platformManager->GetApplicationDirectory());
        const auto requiredSpace = m_currentUpdateInfo->FileSize * 3; // File + backup + extraction space
        
        if (availableSpace < requiredSpace)
        {
            ReportError(UpdateError(UpdateResult::DiskSpaceError, "Insufficient disk space for update"));
            return UpdateResult::DiskSpaceError;
        }
    }
    
    // Check privileges
    if (m_processManager && m_config.RequireElevation && !m_processManager->IsElevated())
    {
        ReportError(UpdateError(UpdateResult::InsufficientPrivileges, "Administrator privileges required"));
        return UpdateResult::InsufficientPrivileges;
    }
    
    return UpdateResult::Success;
}

// Placeholder implementations for remaining methods
std::future<UpdateResult> UpdateOrchestrator::VerifyUpdate(const UpdateInfo& updateInfo)
{
    return std::async(std::launch::async, [this]() { 
        SetState(UpdateState::Verifying, "Verifying update...");
        return UpdateResult::Success; 
    });
}

std::future<UpdateResult> UpdateOrchestrator::CreateBackup()
{
    return std::async(std::launch::async, [this]() { 
        SetState(UpdateState::BackingUp, "Creating backup...");
        return UpdateResult::Success; 
    });
}

std::future<UpdateResult> UpdateOrchestrator::InstallUpdate(const UpdateInfo& updateInfo)
{
    return std::async(std::launch::async, [this]() { 
        SetState(UpdateState::Installing, "Installing update...");
        return UpdateResult::Success; 
    });
}

std::future<UpdateResult> UpdateOrchestrator::Finalize()
{
    return std::async(std::launch::async, [this]() { 
        SetState(UpdateState::Finalizing, "Finalizing update...");
        return UpdateResult::Success; 
    });
}

std::future<UpdateResult> UpdateOrchestrator::RollbackUpdate()
{
    return std::async(std::launch::async, [this]() { 
        SetState(UpdateState::RollingBack, "Rolling back update...");
        return UpdateResult::Success; 
    });
}

UpdateResult UpdateOrchestrator::StopRunningApplications() { return UpdateResult::Success; }
UpdateResult UpdateOrchestrator::RestartApplications() { return UpdateResult::Success; }
UpdateResult UpdateOrchestrator::Cleanup() { return UpdateResult::Success; }
void UpdateOrchestrator::Pause() {}
void UpdateOrchestrator::Resume() {}
bool UpdateOrchestrator::IsUpdateAvailable() const { return m_currentUpdateInfo.has_value(); }
void UpdateOrchestrator::CleanupCache() {}
std::optional<UpdateError> UpdateOrchestrator::GetLastError() const { return m_lastError; }
