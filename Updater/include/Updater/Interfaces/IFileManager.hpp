// Copyright 2024 <github.com/razaqq>
#pragma once

#include "Updater/Models/UpdateResult.hpp"

#include <filesystem>
#include <vector>


namespace PotatoAlert::Updater {

class IFileManager
{
public:
    virtual ~IFileManager() = default;

    // Basic file operations
    virtual UpdateResult CopyFile(const std::filesystem::path& source, const std::filesystem::path& destination) = 0;
    virtual UpdateResult MoveFile(const std::filesystem::path& source, const std::filesystem::path& destination) = 0;
    virtual UpdateResult DeleteFile(const std::filesystem::path& path) = 0;
    virtual UpdateResult CreateDirectory(const std::filesystem::path& path) = 0;
    virtual UpdateResult DeleteDirectory(const std::filesystem::path& path, bool recursive = false) = 0;
    
    // Atomic operations (when supported by platform)
    virtual UpdateResult AtomicReplace(const std::filesystem::path& source, const std::filesystem::path& target) = 0;
    
    // File validation
    virtual bool FileExists(const std::filesystem::path& path) = 0;
    virtual bool DirectoryExists(const std::filesystem::path& path) = 0;
    virtual std::uint64_t GetFileSize(const std::filesystem::path& path) = 0;
    virtual std::string CalculateFileChecksum(const std::filesystem::path& path, const std::string& algorithm = "SHA256") = 0;
    
    // Bulk operations
    virtual UpdateResult CopyDirectory(const std::filesystem::path& source, const std::filesystem::path& destination, bool overwrite = false) = 0;
    virtual std::vector<std::filesystem::path> ListDirectory(const std::filesystem::path& path, bool recursive = false) = 0;
    
    // Special operations for updates
    virtual UpdateResult RenameToTrash(const std::filesystem::path& path) = 0;  // Rename files to .trash extension
    virtual UpdateResult CleanupTrash(const std::filesystem::path& directory) = 0;  // Remove .trash files
    virtual UpdateResult ExtractArchive(const std::filesystem::path& archive, const std::filesystem::path& destination) = 0;
    
    // Platform-specific
    virtual bool CanWrite(const std::filesystem::path& path) = 0;
    virtual UpdateResult SetPermissions(const std::filesystem::path& path, int permissions) = 0;
};

}  // namespace PotatoAlert::Updater
