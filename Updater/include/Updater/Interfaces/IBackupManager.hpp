// Copyright 2024 <github.com/razaqq>
#pragma once

#include "Updater/Models/UpdateResult.hpp"
#include "Updater/Models/UpdateConfiguration.hpp"

#include <filesystem>
#include <vector>
#include <string>


namespace PotatoAlert::Updater {

struct BackupInfo
{
    std::filesystem::path BackupPath;
    std::filesystem::path OriginalPath;
    std::chrono::system_clock::time_point CreationTime;
    std::uint64_t Size;
    std::string Checksum;
    bool IsCompressed = false;
};

class IBackupManager
{
public:
    virtual ~IBackupManager() = default;

    // Create backup of specified files/directories
    virtual UpdateResult CreateBackup(
        const std::vector<std::filesystem::path>& pathsToBackup,
        const std::filesystem::path& backupLocation,
        const UpdateConfiguration& config) = 0;
    
    // Restore from backup
    virtual UpdateResult RestoreBackup(const std::filesystem::path& backupLocation) = 0;
    
    // Verify backup integrity
    virtual UpdateResult VerifyBackup(const std::filesystem::path& backupLocation) = 0;
    
    // Remove backup
    virtual UpdateResult RemoveBackup(const std::filesystem::path& backupLocation) = 0;
    
    // List available backups
    virtual std::vector<BackupInfo> ListBackups(const std::filesystem::path& backupDirectory) = 0;
    
    // Cleanup old backups (keep only the most recent N)
    virtual UpdateResult CleanupOldBackups(const std::filesystem::path& backupDirectory, std::size_t keepCount) = 0;
    
    // Get backup size
    virtual std::uint64_t GetBackupSize(const std::filesystem::path& backupLocation) = 0;
    
    // Check if backup exists and is valid
    virtual bool IsValidBackup(const std::filesystem::path& backupLocation) = 0;
};

}  // namespace PotatoAlert::Updater
