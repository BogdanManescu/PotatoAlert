// Copyright 2024 <github.com/razaqq>
#pragma once

#include "Updater/Interfaces/IFileManager.hpp"
#include "Updater/Interfaces/IProgressReporter.hpp"

#include <filesystem>
#include <future>
#include <memory>
#include <mutex>


namespace PotatoAlert::Updater {

class FileManager : public IFileManager
{
public:
    FileManager();
    ~FileManager() override = default;    // IFileManager implementation
    UpdateResult CopyFile(
        const std::filesystem::path& source,
        const std::filesystem::path& destination) override;

    UpdateResult MoveFile(
        const std::filesystem::path& source,
        const std::filesystem::path& destination) override;

    UpdateResult DeleteFile(
        const std::filesystem::path& path) override;

    UpdateResult CreateDirectory(
        const std::filesystem::path& path) override;

    UpdateResult DeleteDirectory(
        const std::filesystem::path& path,
        bool recursive = false) override;

    UpdateResult AtomicReplace(
        const std::filesystem::path& source,
        const std::filesystem::path& target) override;

    bool FileExists(const std::filesystem::path& path) override;
    bool DirectoryExists(const std::filesystem::path& path) override;
    std::uint64_t GetFileSize(const std::filesystem::path& path) override;
    std::string CalculateFileChecksum(
        const std::filesystem::path& path,
        const std::string& algorithm = "SHA256") override;

    UpdateResult CopyDirectory(
        const std::filesystem::path& source,
        const std::filesystem::path& destination,
        bool overwrite = false) override;

    std::vector<std::filesystem::path> ListDirectory(
        const std::filesystem::path& path,
        bool recursive = false) override;

    UpdateResult RenameToTrash(
        const std::filesystem::path& path) override;

    UpdateResult CleanupTrash(
        const std::filesystem::path& directory) override;

    UpdateResult ExtractArchive(
        const std::filesystem::path& archive,
        const std::filesystem::path& destination) override;

    bool CanWrite(const std::filesystem::path& path) override;
    UpdateResult SetPermissions(
        const std::filesystem::path& path,
        int permissions) override;

private:
    UpdateResult CopyFileInternal(
        const std::filesystem::path& source,
        const std::filesystem::path& destination);

    UpdateResult MoveFileInternal(
        const std::filesystem::path& source,
        const std::filesystem::path& destination);

    UpdateResult DeleteFileInternal(const std::filesystem::path& filePath);
    UpdateResult CreateDirectoryInternal(const std::filesystem::path& dirPath);
    UpdateResult DeleteDirectoryInternal(const std::filesystem::path& dirPath);
    
    UpdateResult ExtractArchiveInternal(
        const std::filesystem::path& archivePath,
        const std::filesystem::path& destination);

    UpdateResult SetFilePermissionsInternal(
        const std::filesystem::path& filePath,
        FilePermissions permissions);

    UpdateResult ReplaceExecutableFilesInternal(
        const std::filesystem::path& sourceDir,
        const std::filesystem::path& targetDir);

    void ReportProgress(const std::string& operation, double progress);

    std::shared_ptr<IProgressReporter> m_progressReporter;
    mutable std::mutex m_mutex;
    mutable UpdateResult m_lastError{UpdateResult::Success};
};

}  // namespace PotatoAlert::Updater
