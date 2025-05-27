// Copyright 2024 <github.com/razaqq>

#include "Updater/Services/BackupManager.hpp"
#include "Updater/Models/ProgressInfo.hpp"
#include "Updater/Interfaces/IProgressReporter.hpp"

#include "Core/Log.hpp"
#include "Core/Sha256.hpp"
#include "Core/Json.hpp"

#include <future>
#include <thread>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>


using namespace PotatoAlert::Updater;
using namespace PotatoAlert::Core;


BackupManager::BackupManager() = default;

void BackupManager::SetProgressReporter(std::shared_ptr<IProgressReporter> reporter)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_progressReporter = reporter;
}

UpdateResult BackupManager::CreateBackup(
    const std::vector<std::filesystem::path>& pathsToBackup,
    const std::filesystem::path& backupLocation,
    const UpdateConfiguration& config)
{
    // Convert UpdateConfiguration to BackupConfiguration
    BackupConfiguration backupConfig = BackupConfiguration::CreateDefault();
    // You could map fields from config if needed
    
    // For now, backup all paths to a single location
    // In a real implementation, you might want to backup each path separately
    if (pathsToBackup.empty()) {
        LOG_WARN("No paths specified for backup");
        return UpdateResult::Success;
    }
    
    // Use the first path as the source directory for simplicity
    // A more sophisticated implementation would handle multiple paths
    return CreateBackupInternal(pathsToBackup[0], backupLocation, backupConfig);
}

UpdateResult BackupManager::RestoreBackup(
    const std::filesystem::path& backupLocation)
{
    // For restore, we need to determine the target directory
    // In a real implementation, this might be stored in the backup metadata
    // For now, assume we're restoring to the current directory
    auto targetDir = std::filesystem::current_path();
    return RestoreBackupInternal(backupLocation, targetDir);
}

UpdateResult BackupManager::VerifyBackup(const std::filesystem::path& backupLocation)
{
    return ValidateBackupInternal(backupLocation);
}

UpdateResult BackupManager::RemoveBackup(const std::filesystem::path& backupLocation)
{
    return DeleteBackupInternal(backupLocation);
}

UpdateResult BackupManager::CleanupOldBackups(
    const std::filesystem::path& backupDirectory,
    std::size_t keepCount)
{
    return CleanupOldBackupsInternal(backupDirectory, keepCount);
}

std::vector<BackupInfo> BackupManager::ListBackups(const std::filesystem::path& backupRootDir) const
{
    std::vector<BackupInfo> backups;
    
    try
    {
        std::error_code ec;
        if (!std::filesystem::exists(backupRootDir, ec) || ec)
        {
            return backups;
        }

        for (const auto& entry : std::filesystem::directory_iterator(backupRootDir, ec))
        {
            if (ec)
            {
                LOG_ERROR("Error iterating backup directory: {}", ec.message());
                break;
            }

            if (entry.is_directory(ec) && !ec)
            {
                BackupInfo info;
                info.BackupPath = entry.path();
                info.BackupName = entry.path().filename().string();
                
                // Try to get creation time from manifest
                auto manifestPath = entry.path() / "backup_manifest.json";
                if (std::filesystem::exists(manifestPath, ec) && !ec)
                {
                    info.CreationTime = std::filesystem::last_write_time(manifestPath, ec);
                    if (ec)
                    {
                        info.CreationTime = std::filesystem::file_time_type{};
                    }
                }
                else
                {
                    info.CreationTime = entry.last_write_time(ec);
                    if (ec)
                    {
                        info.CreationTime = std::filesystem::file_time_type{};
                    }
                }

                // Calculate backup size
                info.BackupSize = 0;
                for (const auto& file : std::filesystem::recursive_directory_iterator(entry.path(), ec))
                {
                    if (ec)
                        break;
                    if (file.is_regular_file(ec) && !ec)
                    {
                        info.BackupSize += file.file_size(ec);
                        if (ec)
                            break;
                    }
                }

                backups.push_back(info);
            }
        }

        // Sort by creation time (newest first)
        std::sort(backups.begin(), backups.end(), 
            [](const BackupInfo& a, const BackupInfo& b) {
                return a.CreationTime > b.CreationTime;
            });
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        LOG_ERROR("Filesystem error listing backups: {}", e.what());
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lastError = UpdateResult::FileSystemError;
    }

    return backups;
}

UpdateResult BackupManager::GetLastError() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastError;
}

UpdateResult BackupManager::CreateBackupInternal(
    const std::filesystem::path& sourceDir,
    const std::filesystem::path& backupDir,
    const BackupConfiguration& config)
{
    try
    {
        ReportProgress("Starting backup", 0.0);

        // Validate source directory
        std::error_code ec;
        if (!std::filesystem::exists(sourceDir, ec) || ec)
        {
            LOG_ERROR("Source directory does not exist: {}", sourceDir.string());
            return UpdateResult::FileSystemError;
        }

        // Create backup directory
        std::filesystem::create_directories(backupDir, ec);
        if (ec)
        {
            LOG_ERROR("Failed to create backup directory: {}", ec.message());
            return UpdateResult::FileSystemError;
        }

        // Collect all files to backup
        std::vector<std::filesystem::path> filesToBackup;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(sourceDir, ec))
        {
            if (ec)
            {
                LOG_ERROR("Error iterating source directory: {}", ec.message());
                return UpdateResult::FileSystemError;
            }

            if (entry.is_regular_file(ec) && !ec)
            {
                // Apply filters if configured
                bool shouldInclude = true;
                std::string extension = entry.path().extension().string();
                std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

                if (!config.IncludePatterns.empty())
                {
                    shouldInclude = false;
                    for (const auto& pattern : config.IncludePatterns)
                    {
                        if (extension == pattern || entry.path().filename().string().find(pattern) != std::string::npos)
                        {
                            shouldInclude = true;
                            break;
                        }
                    }
                }

                for (const auto& pattern : config.ExcludePatterns)
                {
                    if (extension == pattern || entry.path().filename().string().find(pattern) != std::string::npos)
                    {
                        shouldInclude = false;
                        break;
                    }
                }

                if (shouldInclude)
                {
                    filesToBackup.push_back(entry.path());
                }
            }
        }

        if (filesToBackup.empty())
        {
            LOG_WARN("No files found to backup in: {}", sourceDir.string());
            return UpdateResult::Success;
        }

        // Backup files and create manifest entries
        std::vector<BackupEntry> manifestEntries;
        size_t processedFiles = 0;

        for (const auto& sourceFile : filesToBackup)
        {
            auto relativePath = std::filesystem::relative(sourceFile, sourceDir);
            auto backupFile = backupDir / relativePath;

            // Create directory structure in backup
            std::filesystem::create_directories(backupFile.parent_path(), ec);
            if (ec)
            {
                LOG_ERROR("Failed to create backup subdirectory: {}", ec.message());
                return UpdateResult::FileSystemError;
            }

            // Copy file
            std::filesystem::copy_file(sourceFile, backupFile, 
                std::filesystem::copy_options::overwrite_existing, ec);
            if (ec)
            {
                LOG_ERROR("Failed to copy file {} to {}: {}", 
                    sourceFile.string(), backupFile.string(), ec.message());
                return UpdateResult::FileSystemError;
            }

            // Create manifest entry
            BackupEntry entry;
            entry.SourcePath = relativePath;
            entry.BackupPath = backupFile;
            entry.FileSize = std::filesystem::file_size(sourceFile, ec);
            entry.LastWriteTime = std::filesystem::last_write_time(sourceFile, ec);
            
            if (config.CalculateChecksums)
            {
                entry.Checksum = CalculateFileChecksum(sourceFile);
            }

            manifestEntries.push_back(entry);

            processedFiles++;
            double progress = static_cast<double>(processedFiles) / filesToBackup.size() * 0.9; // Reserve 10% for manifest
            ReportProgress(fmt::format("Backed up {} files", processedFiles), progress);
        }

        // Create backup manifest
        auto manifestResult = CreateBackupManifest(backupDir, manifestEntries);
        if (manifestResult != UpdateResult::Success)
        {
            return manifestResult;
        }

        ReportProgress("Backup completed", 1.0);
        LOG_INFO("Successfully created backup: {} ({} files)", backupDir.string(), filesToBackup.size());
        return UpdateResult::Success;
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        LOG_ERROR("Filesystem error creating backup: {}", e.what());
        return UpdateResult::FileSystemError;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Exception creating backup: {}", e.what());
        return UpdateResult::InternalError;
    }
}

UpdateResult BackupManager::RestoreBackupInternal(
    const std::filesystem::path& backupDir,
    const std::filesystem::path& targetDir)
{
    try
    {
        ReportProgress("Starting restore", 0.0);

        // Validate backup directory and manifest
        auto manifestEntries = ReadBackupManifest(backupDir);
        if (!manifestEntries)
        {
            LOG_ERROR("Failed to read backup manifest from: {}", backupDir.string());
            return UpdateResult::BackupCorrupted;
        }

        if (manifestEntries->empty())
        {
            LOG_WARN("Backup manifest is empty: {}", backupDir.string());
            return UpdateResult::Success;
        }

        // Create target directory
        std::error_code ec;
        std::filesystem::create_directories(targetDir, ec);
        if (ec)
        {
            LOG_ERROR("Failed to create target directory: {}", ec.message());
            return UpdateResult::FileSystemError;
        }

        // Restore files
        size_t processedFiles = 0;
        for (const auto& entry : *manifestEntries)
        {
            auto targetFile = targetDir / entry.SourcePath;
            
            // Create directory structure
            std::filesystem::create_directories(targetFile.parent_path(), ec);
            if (ec)
            {
                LOG_ERROR("Failed to create target subdirectory: {}", ec.message());
                return UpdateResult::FileSystemError;
            }

            // Copy file from backup
            std::filesystem::copy_file(entry.BackupPath, targetFile,
                std::filesystem::copy_options::overwrite_existing, ec);
            if (ec)
            {
                LOG_ERROR("Failed to restore file {} to {}: {}",
                    entry.BackupPath.string(), targetFile.string(), ec.message());
                return UpdateResult::FileSystemError;
            }

            // Restore file timestamp
            std::filesystem::last_write_time(targetFile, entry.LastWriteTime, ec);
            if (ec)
            {
                LOG_WARN("Failed to restore timestamp for {}: {}", targetFile.string(), ec.message());
                // Don't fail the restore for timestamp issues
            }

            processedFiles++;
            double progress = static_cast<double>(processedFiles) / manifestEntries->size();
            ReportProgress(fmt::format("Restored {} files", processedFiles), progress);
        }

        ReportProgress("Restore completed", 1.0);
        LOG_INFO("Successfully restored backup: {} ({} files)", backupDir.string(), manifestEntries->size());
        return UpdateResult::Success;
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        LOG_ERROR("Filesystem error restoring backup: {}", e.what());
        return UpdateResult::FileSystemError;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Exception restoring backup: {}", e.what());
        return UpdateResult::InternalError;
    }
}

UpdateResult BackupManager::ValidateBackupInternal(const std::filesystem::path& backupDir)
{
    try
    {
        ReportProgress("Validating backup", 0.0);

        auto manifestEntries = ReadBackupManifest(backupDir);
        if (!manifestEntries)
        {
            LOG_ERROR("Failed to read backup manifest for validation: {}", backupDir.string());
            return UpdateResult::BackupCorrupted;
        }

        size_t processedFiles = 0;
        for (const auto& entry : *manifestEntries)
        {
            // Check if backup file exists
            std::error_code ec;
            if (!std::filesystem::exists(entry.BackupPath, ec) || ec)
            {
                LOG_ERROR("Backup file missing: {}", entry.BackupPath.string());
                return UpdateResult::BackupCorrupted;
            }

            // Check file size
            auto actualSize = std::filesystem::file_size(entry.BackupPath, ec);
            if (ec || actualSize != entry.FileSize)
            {
                LOG_ERROR("Backup file size mismatch: {} (expected: {}, actual: {})",
                    entry.BackupPath.string(), entry.FileSize, actualSize);
                return UpdateResult::BackupCorrupted;
            }

            // Check checksum if available
            if (!entry.Checksum.empty())
            {
                auto actualChecksum = CalculateFileChecksum(entry.BackupPath);
                if (actualChecksum != entry.Checksum)
                {
                    LOG_ERROR("Backup file checksum mismatch: {}", entry.BackupPath.string());
                    return UpdateResult::BackupCorrupted;
                }
            }

            processedFiles++;
            double progress = static_cast<double>(processedFiles) / manifestEntries->size();
            ReportProgress(fmt::format("Validated {} files", processedFiles), progress);
        }

        ReportProgress("Backup validation completed", 1.0);
        LOG_INFO("Successfully validated backup: {} ({} files)", backupDir.string(), manifestEntries->size());
        return UpdateResult::Success;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Exception validating backup: {}", e.what());
        return UpdateResult::InternalError;
    }
}

UpdateResult BackupManager::DeleteBackupInternal(const std::filesystem::path& backupDir)
{
    try
    {
        ReportProgress("Deleting backup", 0.0);

        std::error_code ec;
        if (!std::filesystem::exists(backupDir, ec) || ec)
        {
            // Directory doesn't exist, consider it success
            return UpdateResult::Success;
        }

        auto removed = std::filesystem::remove_all(backupDir, ec);
        if (ec)
        {
            LOG_ERROR("Failed to delete backup directory {}: {}", backupDir.string(), ec.message());
            return UpdateResult::FileSystemError;
        }

        ReportProgress("Backup deleted", 1.0);
        LOG_INFO("Successfully deleted backup: {} ({} items removed)", backupDir.string(), removed);
        return UpdateResult::Success;
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        LOG_ERROR("Filesystem error deleting backup: {}", e.what());
        return UpdateResult::FileSystemError;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Exception deleting backup: {}", e.what());
        return UpdateResult::InternalError;
    }
}

UpdateResult BackupManager::CleanupOldBackupsInternal(
    const std::filesystem::path& backupRootDir,
    size_t maxBackups)
{
    try
    {
        ReportProgress("Cleaning up old backups", 0.0);

        auto backups = ListBackups(backupRootDir);
        if (backups.size() <= maxBackups)
        {
            // No cleanup needed
            return UpdateResult::Success;
        }

        // Delete oldest backups
        size_t backupsToDelete = backups.size() - maxBackups;
        size_t deletedBackups = 0;

        // Backups are already sorted by creation time (newest first)
        for (size_t i = maxBackups; i < backups.size(); ++i)
        {
            auto deleteResult = DeleteBackupInternal(backups[i].BackupPath);
            if (deleteResult != UpdateResult::Success)
            {
                LOG_ERROR("Failed to delete old backup: {}", backups[i].BackupPath.string());
                return deleteResult;
            }

            deletedBackups++;
            double progress = static_cast<double>(deletedBackups) / backupsToDelete;
            ReportProgress(fmt::format("Deleted {} old backups", deletedBackups), progress);
        }

        ReportProgress("Cleanup completed", 1.0);
        LOG_INFO("Successfully cleaned up {} old backups", deletedBackups);
        return UpdateResult::Success;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Exception cleaning up old backups: {}", e.what());
        return UpdateResult::InternalError;
    }
}

UpdateResult BackupManager::CreateBackupManifest(
    const std::filesystem::path& backupDir,
    const std::vector<BackupEntry>& entries)
{
    try
    {
        rapidjson::Document manifest;
        manifest.SetObject();
        auto& allocator = manifest.GetAllocator();

        // Add metadata
        manifest.AddMember("version", "1.0", allocator);
        manifest.AddMember("creation_time", 
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count(), 
            allocator);
        manifest.AddMember("file_count", static_cast<uint64_t>(entries.size()), allocator);

        // Add file entries
        rapidjson::Value fileArray(rapidjson::kArrayType);
        for (const auto& entry : entries)
        {
            rapidjson::Value fileObj(rapidjson::kObjectType);
            
            fileObj.AddMember("source_path", 
                rapidjson::StringRef(entry.SourcePath.string().c_str()), allocator);
            fileObj.AddMember("backup_path", 
                rapidjson::StringRef(entry.BackupPath.string().c_str()), allocator);
            fileObj.AddMember("file_size", static_cast<uint64_t>(entry.FileSize), allocator);
            fileObj.AddMember("last_write_time", 
                static_cast<int64_t>(entry.LastWriteTime.time_since_epoch().count()), allocator);
            
            if (!entry.Checksum.empty())
            {
                fileObj.AddMember("checksum", 
                    rapidjson::StringRef(entry.Checksum.c_str()), allocator);
            }

            fileArray.PushBack(fileObj, allocator);
        }
        
        manifest.AddMember("files", fileArray, allocator);

        // Write manifest to file
        auto manifestPath = backupDir / "backup_manifest.json";
        std::ofstream file(manifestPath);
        if (!file.is_open())
        {
            LOG_ERROR("Failed to create backup manifest file: {}", manifestPath.string());
            return UpdateResult::FileSystemError;
        }

        rapidjson::OStreamWrapper osw(file);
        rapidjson::PrettyWriter<rapidjson::OStreamWrapper> writer(osw);
        manifest.Accept(writer);

        LOG_INFO("Created backup manifest: {}", manifestPath.string());
        return UpdateResult::Success;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Exception creating backup manifest: {}", e.what());
        return UpdateResult::InternalError;
    }
}

std::optional<std::vector<BackupManager::BackupEntry>> BackupManager::ReadBackupManifest(
    const std::filesystem::path& backupDir) const
{
    try
    {
        auto manifestPath = backupDir / "backup_manifest.json";
        
        std::ifstream file(manifestPath);
        if (!file.is_open())
        {
            LOG_ERROR("Failed to open backup manifest file: {}", manifestPath.string());
            return std::nullopt;
        }

        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        
        PA_TRY_OR_ELSE(manifest, ParseJson(content),
        {
            LOG_ERROR("Failed to parse backup manifest JSON");
            return std::nullopt;
        });

        if (!manifest.HasMember("files") || !manifest["files"].IsArray())
        {
            LOG_ERROR("Invalid backup manifest format");
            return std::nullopt;
        }

        std::vector<BackupEntry> entries;
        const auto& filesArray = manifest["files"];

        for (auto it = filesArray.Begin(); it != filesArray.End(); ++it)
        {
            if (!it->IsObject())
                continue;

            BackupEntry entry;
            
            if (it->HasMember("source_path") && (*it)["source_path"].IsString())
            {
                entry.SourcePath = (*it)["source_path"].GetString();
            }
            
            if (it->HasMember("backup_path") && (*it)["backup_path"].IsString())
            {
                entry.BackupPath = (*it)["backup_path"].GetString();
            }
            
            if (it->HasMember("file_size") && (*it)["file_size"].IsUint64())
            {
                entry.FileSize = (*it)["file_size"].GetUint64();
            }
            
            if (it->HasMember("last_write_time") && (*it)["last_write_time"].IsInt64())
            {
                auto timePoint = std::filesystem::file_time_type{
                    std::filesystem::file_time_type::duration{(*it)["last_write_time"].GetInt64()}
                };
                entry.LastWriteTime = timePoint;
            }
            
            if (it->HasMember("checksum") && (*it)["checksum"].IsString())
            {
                entry.Checksum = (*it)["checksum"].GetString();
            }

            entries.push_back(entry);
        }

        return entries;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Exception reading backup manifest: {}", e.what());
        return std::nullopt;
    }
}

std::string BackupManager::CalculateFileChecksum(const std::filesystem::path& filePath) const
{
    try
    {
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open())
        {
            LOG_ERROR("Failed to open file for checksum calculation: {}", filePath.string());
            return "";
        }

        Sha256 sha256;
        constexpr size_t bufferSize = 8192;
        std::vector<char> buffer(bufferSize);

        while (file.read(buffer.data(), bufferSize) || file.gcount() > 0)
        {
            sha256.Update(reinterpret_cast<const uint8_t*>(buffer.data()), file.gcount());
        }

        auto hash = sha256.Finalize();
        
        // Convert to hex string
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (const auto& byte : hash)
        {
            oss << std::setw(2) << static_cast<unsigned>(byte);
        }

        return oss.str();
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Exception calculating file checksum: {}", e.what());
        return "";
    }
}

void BackupManager::ReportProgress(const std::string& operation, double progress)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_progressReporter)
    {
        ProgressInfo progressInfo;
        progressInfo.Stage = UpdateStage::CreatingBackup;
        progressInfo.Message = operation;
        progressInfo.Progress = progress;
        m_progressReporter->ReportProgress(progressInfo);
    }
}

std::uint64_t BackupManager::GetBackupSize(const std::filesystem::path& backupLocation)
{
    std::uint64_t totalSize = 0;
    try
    {
        std::error_code ec;
        if (!std::filesystem::exists(backupLocation, ec) || ec)
        {
            return 0;
        }

        for (const auto& entry : std::filesystem::recursive_directory_iterator(backupLocation, ec))
        {
            if (ec)
                break;
            if (entry.is_regular_file(ec) && !ec)
            {
                totalSize += entry.file_size(ec);
                if (ec)
                    break;
            }
        }
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Exception calculating backup size: {}", e.what());
        return 0;
    }
    return totalSize;
}

bool BackupManager::IsValidBackup(const std::filesystem::path& backupLocation)
{
    try
    {
        std::error_code ec;
        if (!std::filesystem::exists(backupLocation, ec) || ec)
        {
            return false;
        }

        // Check if manifest exists
        auto manifestPath = backupLocation / "backup_manifest.json";
        if (!std::filesystem::exists(manifestPath, ec) || ec)
        {
            return false;
        }

        // Try to read and validate the manifest
        auto manifestEntries = ReadBackupManifest(backupLocation);
        return manifestEntries.has_value();
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Exception validating backup: {}", e.what());
        return false;
    }
}
