#pragma once

#include "Updater/Interfaces/IPlatformManager.hpp"
#include "Updater/Models/UpdateResult.hpp"
#include "Core/Log.hpp"
#include <sys/stat.h>
#include <unistd.h>

namespace PotatoAlert::Updater::Platform::Linux {

class LinuxPlatformManager : public IPlatformManager
{
public:
    LinuxPlatformManager();
    ~LinuxPlatformManager() override = default;

    // Platform detection
    Platform GetCurrentPlatform() override { return Platform::Linux; }
    std::string GetPlatformString() override;
    std::string GetArchitectureString() override;
    
    // Component factories
    std::unique_ptr<IFileManager> CreateFileManager() override;
    std::unique_ptr<IProcessManager> CreateProcessManager() override;
    
    // Platform-specific capabilities
    bool SupportsAtomicFileOperations() override { return true; }
    bool RequiresElevationForUpdates() override;
    bool SupportsServiceInstallation() override;
    
    // System information
    std::uint64_t GetAvailableDiskSpace(const std::filesystem::path& path) override;
    std::string GetSystemVersion() override;
    bool IsSystemCompatible() override;
    
    // Special directories
    std::filesystem::path GetApplicationDirectory() override;
    std::filesystem::path GetTempDirectory() override;
    std::filesystem::path GetConfigDirectory() override;
    
    // Security
    bool CanWriteToApplicationDirectory() override;
    UpdateResult RequestWritePermissions(const std::filesystem::path& path) override;

private:
    std::string m_platformString;
    std::string m_architectureString;
    std::string m_systemVersion;
    
    // Helper methods
    bool InitializeSystemInfo();
    std::string GetLinuxDistribution();
    std::string GetProcessorArchitecture();
    bool HasWritePermission(const std::filesystem::path& path);
    std::string ReadFile(const std::string& path);
    std::string ExecuteCommand(const std::string& command);
};

} // namespace PotatoAlert::Updater::Platform::Linux
