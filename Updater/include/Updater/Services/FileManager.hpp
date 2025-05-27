// Copyright 2024 <github.com/razaqq>
#pragma once

#include "Updater/Interfaces/IFileManager.hpp"

#include <filesystem>
#include <memory>
#include <mutex>


namespace PotatoAlert::Updater {

class FileManager : public IFileManager
{
public:
    FileManager();
    ~FileManager() override = default;

    // IFileManager implementation
    void SetProgressReporter(std::shared_ptr<IProgressReporter> reporter) override;

    std::future<UpdateResult> CopyFile(
        const std::filesystem::path& source,
        const std::filesystem::path& destination) override;

    std::future<UpdateResult> MoveFile(
        const std::filesystem::path& source,
        const std::filesystem::path& destination) override;

    std::future<UpdateResult> DeleteFile(
        const std::filesystem::path& filePath) override;

    std::future<UpdateResult> CreateDirectory(
        const std::filesystem::path& dirPath) override;

    std::future<UpdateResult> DeleteDirectory(
        const std::filesystem::path& dirPath) override;

    std::future<UpdateResult> ExtractArchive(
        const std::filesystem::path& archivePath,
        const std::filesystem::path& destination) override;

    std::future<UpdateResult> SetFilePermissions(
        const std::filesystem::path& filePath,
        FilePermissions permissions) override;

    std::future<UpdateResult> ReplaceExecutableFiles(
        const std::filesystem::path& sourceDir,
        const std::filesystem::path& targetDir) override;

    bool FileExists(const std::filesystem::path& filePath) const override;
    bool DirectoryExists(const std::filesystem::path& dirPath) const override;
    size_t GetFileSize(const std::filesystem::path& filePath) const override;
    std::filesystem::file_time_type GetLastWriteTime(const std::filesystem::path& filePath) const override;

    std::vector<std::filesystem::path> GetExecutableFiles(
        const std::filesystem::path& directory) const override;

    UpdateResult GetLastError() const override;

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
