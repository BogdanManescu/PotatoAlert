// Copyright 2024 <github.com/razaqq>
#pragma once

#include "IFileManager.hpp"
#include "IProcessManager.hpp"
#include "Updater/Models/UpdateResult.hpp"

#include <memory>
#include <filesystem>
#include <cstdint>


namespace PotatoAlert::Updater {

enum class Platform
{
    Windows,
    Linux,
    Unknown
};

class IPlatformManager
{
public:
    virtual ~IPlatformManager() = default;

    // Platform detection
    virtual Platform GetCurrentPlatform() = 0;
    virtual std::string GetPlatformString() = 0;
    virtual std::string GetArchitectureString() = 0;
    
    // Component factories
    virtual std::unique_ptr<IFileManager> CreateFileManager() = 0;
    virtual std::unique_ptr<IProcessManager> CreateProcessManager() = 0;
    
    // Platform-specific capabilities
    virtual bool SupportsAtomicFileOperations() = 0;
    virtual bool RequiresElevationForUpdates() = 0;
    virtual bool SupportsServiceInstallation() = 0;
    
    // System information
    virtual std::uint64_t GetAvailableDiskSpace(const std::filesystem::path& path) = 0;
    virtual std::string GetSystemVersion() = 0;
    virtual bool IsSystemCompatible() = 0;
    
    // Special directories
    virtual std::filesystem::path GetApplicationDirectory() = 0;
    virtual std::filesystem::path GetTempDirectory() = 0;
    virtual std::filesystem::path GetConfigDirectory() = 0;
    
    // Security
    virtual bool CanWriteToApplicationDirectory() = 0;
    virtual UpdateResult RequestWritePermissions(const std::filesystem::path& path) = 0;
};

}  // namespace PotatoAlert::Updater
