// Copyright 2024 <github.com/razaqq>
#pragma once

#include "Updater/Interfaces/IBackupManager.hpp"

#include <filesystem>
#include <memory>
#include <mutex>
#include <vector>


namespace PotatoAlert::Updater {

class BackupManager : public IBackupManager
{
public:
    BackupManager();
    ~BackupManager() override = default;

    // IBackupManager implementation
    void SetProgressReporter(std::shared_ptr<IProgressReporter> reporter) override;

    std::future<UpdateResult> CreateBackup(
        const std::filesystem::path& sourceDir,
        const std::filesystem::path& backupDir,
        const BackupConfiguration& config) override;

    std::future<UpdateResult> RestoreBackup(
        const std::filesystem::path& backupDir,
        const std::filesystem::path& targetDir) override;

    std::future<UpdateResult> ValidateBackup(
        const std::filesystem::path& backupDir) override;

    std::future<UpdateResult> DeleteBackup(
        const std::filesystem::path& backupDir) override;

    std::future<UpdateResult> CleanupOldBackups(
        const std::filesystem::path& backupRootDir,
        size_t maxBackups) override;

    std::vector<BackupInfo> ListBackups(
        const std::filesystem::path& backupRootDir) const override;

    UpdateResult GetLastError() const override;

private:
    struct BackupEntry
    {
        std::filesystem::path SourcePath;
        std::filesystem::path BackupPath;
        size_t FileSize;
        std::filesystem::file_time_type LastWriteTime;
        std::string Checksum;
    };

    UpdateResult CreateBackupInternal(
        const std::filesystem::path& sourceDir,
        const std::filesystem::path& backupDir,
        const BackupConfiguration& config);

    UpdateResult RestoreBackupInternal(
        const std::filesystem::path& backupDir,
        const std::filesystem::path& targetDir);

    UpdateResult ValidateBackupInternal(const std::filesystem::path& backupDir);
    UpdateResult DeleteBackupInternal(const std::filesystem::path& backupDir);
    
    UpdateResult CleanupOldBackupsInternal(
        const std::filesystem::path& backupRootDir,
        size_t maxBackups);

    UpdateResult CreateBackupManifest(
        const std::filesystem::path& backupDir,
        const std::vector<BackupEntry>& entries);

    std::optional<std::vector<BackupEntry>> ReadBackupManifest(
        const std::filesystem::path& backupDir) const;

    std::string CalculateFileChecksum(const std::filesystem::path& filePath) const;
    
    void ReportProgress(const std::string& operation, double progress);

    std::shared_ptr<IProgressReporter> m_progressReporter;
    mutable std::mutex m_mutex;
    mutable UpdateResult m_lastError{UpdateResult::Success};
};

}  // namespace PotatoAlert::Updater
