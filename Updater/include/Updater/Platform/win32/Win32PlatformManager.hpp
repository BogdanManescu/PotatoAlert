#pragma once

#include "Updater/Interfaces/IPlatformManager.hpp"
#include "Updater/Models/UpdateResult.hpp"
#include "Core/Log.hpp"
#include <windows.h>

namespace PotatoAlert::Updater::Platform::Win32 {

class Win32PlatformManager : public IPlatformManager
{
public:
    Win32PlatformManager();
    ~Win32PlatformManager() override = default;

    // Platform detection
    Platform GetCurrentPlatform() override { return Platform::Windows; }
    std::string GetPlatformString() override;
    std::string GetArchitectureString() override;
    
    // Component factories
    std::unique_ptr<IFileManager> CreateFileManager() override;
    std::unique_ptr<IProcessManager> CreateProcessManager() override;
    
    // Platform-specific capabilities
    bool SupportsAtomicFileOperations() override { return true; }
    bool RequiresElevationForUpdates() override;
    bool SupportsServiceInstallation() override { return true; }
    
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
    std::string GetWindowsVersionString();
    std::string GetProcessorArchitecture();
    bool IsElevated();
    bool CanElevate();
    bool HasWritePermission(const std::filesystem::path& path);
};

} // namespace PotatoAlert::Updater::Platform::Win32
