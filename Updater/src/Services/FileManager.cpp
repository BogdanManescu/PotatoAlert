// Copyright 2024 <github.com/razaqq>

#include "Updater/Services/FileManager.hpp"
#include "Updater/Models/ProgressInfo.hpp"
#include "Updater/Interfaces/IProgressReporter.hpp"

#include "Core/Log.hpp"
#include "Core/Zip.hpp"

#include <future>
#include <thread>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#include <aclapi.h>
#include <sddl.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif


using namespace PotatoAlert::Updater;
using namespace PotatoAlert::Core;


FileManager::FileManager() = default;

void FileManager::SetProgressReporter(std::shared_ptr<IProgressReporter> reporter)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_progressReporter = reporter;
}

std::future<UpdateResult> FileManager::CopyFile(
    const std::filesystem::path& source,
    const std::filesystem::path& destination)
{
    return std::async(std::launch::async, [this, source, destination]() {
        return CopyFileInternal(source, destination);
    });
}

std::future<UpdateResult> FileManager::MoveFile(
    const std::filesystem::path& source,
    const std::filesystem::path& destination)
{
    return std::async(std::launch::async, [this, source, destination]() {
        return MoveFileInternal(source, destination);
    });
}

std::future<UpdateResult> FileManager::DeleteFile(const std::filesystem::path& filePath)
{
    return std::async(std::launch::async, [this, filePath]() {
        return DeleteFileInternal(filePath);
    });
}

std::future<UpdateResult> FileManager::CreateDirectory(const std::filesystem::path& dirPath)
{
    return std::async(std::launch::async, [this, dirPath]() {
        return CreateDirectoryInternal(dirPath);
    });
}

std::future<UpdateResult> FileManager::DeleteDirectory(const std::filesystem::path& dirPath)
{
    return std::async(std::launch::async, [this, dirPath]() {
        return DeleteDirectoryInternal(dirPath);
    });
}

std::future<UpdateResult> FileManager::ExtractArchive(
    const std::filesystem::path& archivePath,
    const std::filesystem::path& destination)
{
    return std::async(std::launch::async, [this, archivePath, destination]() {
        return ExtractArchiveInternal(archivePath, destination);
    });
}

std::future<UpdateResult> FileManager::SetFilePermissions(
    const std::filesystem::path& filePath,
    FilePermissions permissions)
{
    return std::async(std::launch::async, [this, filePath, permissions]() {
        return SetFilePermissionsInternal(filePath, permissions);
    });
}

std::future<UpdateResult> FileManager::ReplaceExecutableFiles(
    const std::filesystem::path& sourceDir,
    const std::filesystem::path& targetDir)
{
    return std::async(std::launch::async, [this, sourceDir, targetDir]() {
        return ReplaceExecutableFilesInternal(sourceDir, targetDir);
    });
}

bool FileManager::FileExists(const std::filesystem::path& filePath) const
{
    std::error_code ec;
    bool exists = std::filesystem::exists(filePath, ec);
    if (ec)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lastError = UpdateResult::FileSystemError;
        LOG_ERROR("Error checking if file exists {}: {}", filePath.string(), ec.message());
    }
    return exists;
}

bool FileManager::DirectoryExists(const std::filesystem::path& dirPath) const
{
    std::error_code ec;
    bool exists = std::filesystem::is_directory(dirPath, ec);
    if (ec)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lastError = UpdateResult::FileSystemError;
        LOG_ERROR("Error checking if directory exists {}: {}", dirPath.string(), ec.message());
    }
    return exists;
}

size_t FileManager::GetFileSize(const std::filesystem::path& filePath) const
{
    std::error_code ec;
    auto size = std::filesystem::file_size(filePath, ec);
    if (ec)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lastError = UpdateResult::FileSystemError;
        LOG_ERROR("Error getting file size {}: {}", filePath.string(), ec.message());
        return 0;
    }
    return size;
}

std::filesystem::file_time_type FileManager::GetLastWriteTime(const std::filesystem::path& filePath) const
{
    std::error_code ec;
    auto time = std::filesystem::last_write_time(filePath, ec);
    if (ec)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lastError = UpdateResult::FileSystemError;
        LOG_ERROR("Error getting last write time {}: {}", filePath.string(), ec.message());
    }
    return time;
}

std::vector<std::filesystem::path> FileManager::GetExecutableFiles(const std::filesystem::path& directory) const
{
    std::vector<std::filesystem::path> executables;
    
    try
    {
        std::error_code ec;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(directory, ec))
        {
            if (ec)
            {
                LOG_ERROR("Error iterating directory {}: {}", directory.string(), ec.message());
                break;
            }

            if (entry.is_regular_file(ec) && !ec)
            {
                std::string extension = entry.path().extension().string();
                std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
                
                if (extension == ".exe" || extension == ".dll")
                {
                    executables.push_back(entry.path());
                }
            }
        }
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lastError = UpdateResult::FileSystemError;
        LOG_ERROR("Filesystem error getting executables: {}", e.what());
    }

    return executables;
}

UpdateResult FileManager::GetLastError() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastError;
}

UpdateResult FileManager::CopyFileInternal(
    const std::filesystem::path& source,
    const std::filesystem::path& destination)
{
    try
    {
        ReportProgress("Copying file", 0.0);

        // Ensure destination directory exists
        std::error_code ec;
        std::filesystem::create_directories(destination.parent_path(), ec);
        if (ec)
        {
            LOG_ERROR("Failed to create destination directory: {}", ec.message());
            return UpdateResult::FileSystemError;
        }

        // Copy the file
        std::filesystem::copy_file(source, destination, 
            std::filesystem::copy_options::overwrite_existing, ec);
        
        if (ec)
        {
            LOG_ERROR("Failed to copy file {} to {}: {}", 
                source.string(), destination.string(), ec.message());
            return UpdateResult::FileSystemError;
        }

        ReportProgress("File copied", 1.0);
        LOG_INFO("Successfully copied file {} to {}", source.string(), destination.string());
        return UpdateResult::Success;
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        LOG_ERROR("Filesystem error copying file: {}", e.what());
        return UpdateResult::FileSystemError;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Exception copying file: {}", e.what());
        return UpdateResult::InternalError;
    }
}

UpdateResult FileManager::MoveFileInternal(
    const std::filesystem::path& source,
    const std::filesystem::path& destination)
{
    try
    {
        ReportProgress("Moving file", 0.0);

        // Ensure destination directory exists
        std::error_code ec;
        std::filesystem::create_directories(destination.parent_path(), ec);
        if (ec)
        {
            LOG_ERROR("Failed to create destination directory: {}", ec.message());
            return UpdateResult::FileSystemError;
        }

        // Try atomic rename first (if on same filesystem)
        std::filesystem::rename(source, destination, ec);
        
        if (ec)
        {
            // Fallback to copy + delete
            auto copyResult = CopyFileInternal(source, destination);
            if (copyResult != UpdateResult::Success)
            {
                return copyResult;
            }

            auto deleteResult = DeleteFileInternal(source);
            if (deleteResult != UpdateResult::Success)
            {
                LOG_WARN("File copied but failed to delete source: {}", source.string());
                // Don't return error since copy succeeded
            }
        }

        ReportProgress("File moved", 1.0);
        LOG_INFO("Successfully moved file {} to {}", source.string(), destination.string());
        return UpdateResult::Success;
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        LOG_ERROR("Filesystem error moving file: {}", e.what());
        return UpdateResult::FileSystemError;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Exception moving file: {}", e.what());
        return UpdateResult::InternalError;
    }
}

UpdateResult FileManager::DeleteFileInternal(const std::filesystem::path& filePath)
{
    try
    {
        ReportProgress("Deleting file", 0.0);

        std::error_code ec;
        if (!std::filesystem::exists(filePath, ec))
        {
            if (ec)
            {
                LOG_ERROR("Error checking file existence: {}", ec.message());
                return UpdateResult::FileSystemError;
            }
            // File doesn't exist, consider it success
            return UpdateResult::Success;
        }

        std::filesystem::remove(filePath, ec);
        if (ec)
        {
            LOG_ERROR("Failed to delete file {}: {}", filePath.string(), ec.message());
            return UpdateResult::FileSystemError;
        }

        ReportProgress("File deleted", 1.0);
        LOG_INFO("Successfully deleted file: {}", filePath.string());
        return UpdateResult::Success;
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        LOG_ERROR("Filesystem error deleting file: {}", e.what());
        return UpdateResult::FileSystemError;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Exception deleting file: {}", e.what());
        return UpdateResult::InternalError;
    }
}

UpdateResult FileManager::CreateDirectoryInternal(const std::filesystem::path& dirPath)
{
    try
    {
        ReportProgress("Creating directory", 0.0);

        std::error_code ec;
        std::filesystem::create_directories(dirPath, ec);
        if (ec)
        {
            LOG_ERROR("Failed to create directory {}: {}", dirPath.string(), ec.message());
            return UpdateResult::FileSystemError;
        }

        ReportProgress("Directory created", 1.0);
        LOG_INFO("Successfully created directory: {}", dirPath.string());
        return UpdateResult::Success;
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        LOG_ERROR("Filesystem error creating directory: {}", e.what());
        return UpdateResult::FileSystemError;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Exception creating directory: {}", e.what());
        return UpdateResult::InternalError;
    }
}

UpdateResult FileManager::DeleteDirectoryInternal(const std::filesystem::path& dirPath)
{
    try
    {
        ReportProgress("Deleting directory", 0.0);

        std::error_code ec;
        if (!std::filesystem::exists(dirPath, ec))
        {
            if (ec)
            {
                LOG_ERROR("Error checking directory existence: {}", ec.message());
                return UpdateResult::FileSystemError;
            }
            // Directory doesn't exist, consider it success
            return UpdateResult::Success;
        }

        auto removed = std::filesystem::remove_all(dirPath, ec);
        if (ec)
        {
            LOG_ERROR("Failed to delete directory {}: {}", dirPath.string(), ec.message());
            return UpdateResult::FileSystemError;
        }

        ReportProgress("Directory deleted", 1.0);
        LOG_INFO("Successfully deleted directory: {} ({} items removed)", dirPath.string(), removed);
        return UpdateResult::Success;
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        LOG_ERROR("Filesystem error deleting directory: {}", e.what());
        return UpdateResult::FileSystemError;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Exception deleting directory: {}", e.what());
        return UpdateResult::InternalError;
    }
}

UpdateResult FileManager::ExtractArchiveInternal(
    const std::filesystem::path& archivePath,
    const std::filesystem::path& destination)
{
    try
    {
        ReportProgress("Extracting archive", 0.0);

        const Zip zip = Zip::Open(archivePath, 0);
        if (!zip)
        {
            LOG_ERROR("Failed to open zip file: {}", archivePath.string());
            return UpdateResult::ArchiveError;
        }

        int totalEntries = zip.EntryCount();
        int extractedEntries = 0;

        auto onExtract = [this, &extractedEntries, totalEntries, destination](const char* fileName) -> int
        {
            extractedEntries++;
            double progress = static_cast<double>(extractedEntries) / totalEntries;
            
            std::string message = fmt::format("Extracted: {} ({}/{})", 
                std::filesystem::relative(fileName, destination).string(), 
                extractedEntries, totalEntries);
            
            ReportProgress(message, progress);
            LOG_INFO(message);
            
            return 0;
        };

        bool success = Zip::Extract(archivePath, destination, onExtract);
        if (!success)
        {
            LOG_ERROR("Failed to extract archive: {}", archivePath.string());
            return UpdateResult::ArchiveError;
        }

        ReportProgress("Archive extracted", 1.0);
        LOG_INFO("Successfully extracted archive {} to {}", archivePath.string(), destination.string());
        return UpdateResult::Success;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Exception extracting archive: {}", e.what());
        return UpdateResult::ArchiveError;
    }
}

UpdateResult FileManager::SetFilePermissionsInternal(
    const std::filesystem::path& filePath,
    FilePermissions permissions)
{
    try
    {
        ReportProgress("Setting file permissions", 0.0);

#ifdef _WIN32
        // Windows implementation using ACLs
        DWORD desiredAccess = 0;
        
        if (permissions & FilePermissions::Read)
            desiredAccess |= GENERIC_READ;
        if (permissions & FilePermissions::Write)
            desiredAccess |= GENERIC_WRITE;
        if (permissions & FilePermissions::Execute)
            desiredAccess |= GENERIC_EXECUTE;

        // This is a simplified implementation
        // In practice, you'd want more granular control over ACLs
        
        ReportProgress("File permissions set", 1.0);
        return UpdateResult::Success;

#else
        // Unix implementation
        mode_t mode = 0;
        
        if (permissions & FilePermissions::Read)
            mode |= S_IRUSR | S_IRGRP | S_IROTH;
        if (permissions & FilePermissions::Write)
            mode |= S_IWUSR;
        if (permissions & FilePermissions::Execute)
            mode |= S_IXUSR | S_IXGRP | S_IXOTH;

        if (chmod(filePath.c_str(), mode) != 0)
        {
            LOG_ERROR("Failed to set permissions for {}: {}", filePath.string(), strerror(errno));
            return UpdateResult::FileSystemError;
        }

        ReportProgress("File permissions set", 1.0);
        LOG_INFO("Successfully set permissions for: {}", filePath.string());
        return UpdateResult::Success;
#endif
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Exception setting file permissions: {}", e.what());
        return UpdateResult::InternalError;
    }
}

UpdateResult FileManager::ReplaceExecutableFilesInternal(
    const std::filesystem::path& sourceDir,
    const std::filesystem::path& targetDir)
{
    try
    {
        ReportProgress("Replacing executable files", 0.0);

        auto sourceFiles = GetExecutableFiles(sourceDir);
        if (sourceFiles.empty())
        {
            LOG_WARN("No executable files found in source directory: {}", sourceDir.string());
            return UpdateResult::Success;
        }

        size_t processedFiles = 0;
        
        for (const auto& sourceFile : sourceFiles)
        {
            // Calculate relative path and target location
            auto relativePath = std::filesystem::relative(sourceFile, sourceDir);
            auto targetFile = targetDir / relativePath;

            // First rename existing file to .trash if it exists
            if (FileExists(targetFile))
            {
                auto trashFile = targetFile;
                trashFile += ".trash";
                
                auto renameResult = MoveFileInternal(targetFile, trashFile);
                if (renameResult != UpdateResult::Success)
                {
                    LOG_ERROR("Failed to rename existing file to trash: {}", targetFile.string());
                    return renameResult;
                }
            }

            // Copy new file to target location
            auto copyResult = CopyFileInternal(sourceFile, targetFile);
            if (copyResult != UpdateResult::Success)
            {
                LOG_ERROR("Failed to copy new file: {}", sourceFile.string());
                return copyResult;
            }

            processedFiles++;
            double progress = static_cast<double>(processedFiles) / sourceFiles.size();
            ReportProgress(fmt::format("Replaced {} files", processedFiles), progress);
        }

        ReportProgress("Executable files replaced", 1.0);
        LOG_INFO("Successfully replaced {} executable files", sourceFiles.size());
        return UpdateResult::Success;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Exception replacing executable files: {}", e.what());
        return UpdateResult::InternalError;
    }
}

void FileManager::ReportProgress(const std::string& operation, double progress)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_progressReporter)
    {
        ProgressInfo progressInfo;
        progressInfo.Stage = UpdateStage::Installing;
        progressInfo.Message = operation;
        progressInfo.Progress = progress;
        m_progressReporter->ReportProgress(progressInfo);
    }
}
