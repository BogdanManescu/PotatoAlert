#include "Updater/Platform/win32/Win32PlatformManager.hpp"
#include "Updater/Platform/Win32ProcessManager.hpp"
#include "Updater/Services/FileManager.hpp"
#include "Core/String.hpp"
#include <shlobj.h>
#include <versionhelpers.h>
#include <sddl.h>
#include <aclapi.h>

using namespace PotatoAlert::Core;
using namespace PotatoAlert::Updater;
using namespace PotatoAlert::Updater::Platform::Win32;

Win32PlatformManager::Win32PlatformManager()
{
    InitializeSystemInfo();
}

std::string Win32PlatformManager::GetPlatformString()
{
    if (m_platformString.empty()) {
        m_platformString = GetWindowsVersionString();
    }
    return m_platformString;
}

std::string Win32PlatformManager::GetArchitectureString()
{
    if (m_architectureString.empty()) {
        m_architectureString = GetProcessorArchitecture();
    }
    return m_architectureString;
}

std::unique_ptr<IFileManager> Win32PlatformManager::CreateFileManager()
{
    return std::make_unique<Services::FileManager>();
}

std::unique_ptr<IProcessManager> Win32PlatformManager::CreateProcessManager()
{
    return std::make_unique<Win32ProcessManager>();
}

bool Win32PlatformManager::RequiresElevationForUpdates()
{
    // Check if the application directory requires elevation to write
    auto appDir = GetApplicationDirectory();
    return !CanWriteToApplicationDirectory();
}

std::uint64_t Win32PlatformManager::GetAvailableDiskSpace(const std::filesystem::path& path)
{
    ULARGE_INTEGER freeBytesAvailable;
    ULARGE_INTEGER totalNumberOfBytes;
    ULARGE_INTEGER totalNumberOfFreeBytes;
    
    std::wstring widePath = path.wstring();
    
    if (GetDiskFreeSpaceExW(
        widePath.c_str(),
        &freeBytesAvailable,
        &totalNumberOfBytes,
        &totalNumberOfFreeBytes)) {
        return freeBytesAvailable.QuadPart;
    }
    
    LOG_ERROR("Failed to get disk space for path: {}, Error: {}", path.string(), GetLastError());
    return 0;
}

std::string Win32PlatformManager::GetSystemVersion()
{
    if (m_systemVersion.empty()) {
        m_systemVersion = GetWindowsVersionString();
    }
    return m_systemVersion;
}

bool Win32PlatformManager::IsSystemCompatible()
{
    // Check if we're running on a supported Windows version
    // Minimum requirement: Windows 10 version 1809 (build 17763)
    return IsWindows10OrGreater();
}

std::filesystem::path Win32PlatformManager::GetApplicationDirectory()
{
    wchar_t path[MAX_PATH];
    if (GetModuleFileNameW(NULL, path, MAX_PATH) > 0) {
        std::filesystem::path appPath(path);
        return appPath.parent_path();
    }
    
    LOG_ERROR("Failed to get application directory");
    return std::filesystem::current_path();
}

std::filesystem::path Win32PlatformManager::GetTempDirectory()
{
    wchar_t tempPath[MAX_PATH];
    DWORD pathLength = GetTempPathW(MAX_PATH, tempPath);
    
    if (pathLength > 0 && pathLength < MAX_PATH) {
        return std::filesystem::path(tempPath);
    }
    
    LOG_ERROR("Failed to get temp directory");
    return std::filesystem::temp_directory_path();
}

std::filesystem::path Win32PlatformManager::GetConfigDirectory()
{
    wchar_t* appDataPath = nullptr;
    
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &appDataPath))) {
        std::filesystem::path configPath(appDataPath);
        CoTaskMemFree(appDataPath);
        
        configPath /= "PotatoAlert";
        return configPath;
    }
    
    LOG_ERROR("Failed to get config directory");
    return GetApplicationDirectory();
}

bool Win32PlatformManager::CanWriteToApplicationDirectory()
{
    return HasWritePermission(GetApplicationDirectory());
}

UpdateResult Win32PlatformManager::RequestWritePermissions(const std::filesystem::path& path)
{
    if (HasWritePermission(path)) {
        return UpdateResult::Success;
    }
    
    if (!IsElevated() && CanElevate()) {
        LOG_INFO("Requesting elevation for write permissions to: {}", path.string());
        // This would typically involve restarting the process with elevation
        // For now, we'll just indicate that elevation is needed
        return UpdateResult::InsufficientPrivileges;
    }
    
    LOG_ERROR("Cannot obtain write permissions for path: {}", path.string());
    return UpdateResult::InsufficientPrivileges;
}

// Private helper methods

bool Win32PlatformManager::InitializeSystemInfo()
{
    try {
        m_platformString = GetWindowsVersionString();
        m_architectureString = GetProcessorArchitecture();
        m_systemVersion = m_platformString;
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to initialize system info: {}", e.what());
        return false;
    }
}

std::string Win32PlatformManager::GetWindowsVersionString()
{
    std::string version = "Windows";
    
    // Note: IsWindows11OrGreater() is not available in older SDKs
    // We'll check for Windows 11 by examining the build number
    bool isWindows11 = false;
    
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, 
                     L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 
                     0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        
        wchar_t buildNumber[256];
        DWORD bufferSize = sizeof(buildNumber);
        
        if (RegQueryValueExW(hKey, L"CurrentBuild", NULL, NULL, 
                            (LPBYTE)buildNumber, &bufferSize) == ERROR_SUCCESS) {
            int build = _wtoi(buildNumber);
            // Windows 11 starts from build 22000
            isWindows11 = (build >= 22000);
        }
        
        RegCloseKey(hKey);
    }
    
    if (isWindows11) {
        version = "Windows 11";
    } else if (IsWindows10OrGreater()) {
        version = "Windows 10";
    } else if (IsWindows8Point1OrGreater()) {
        version = "Windows 8.1";
    } else if (IsWindows8OrGreater()) {
        version = "Windows 8";
    } else if (IsWindows7OrGreater()) {
        version = "Windows 7";
    }
    
    // Try to get more detailed version information
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, 
                     L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 
                     0, KEY_READ, &hKey) == ERROR_SUCCESS) {
          wchar_t buildNumber[256];
        DWORD bufferSize = sizeof(buildNumber);        
        if (RegQueryValueExW(hKey, L"CurrentBuild", NULL, NULL, 
                            (LPBYTE)buildNumber, &bufferSize) == ERROR_SUCCESS) {
            // Convert wide string to narrow string
            std::wstring wideBuild(buildNumber);
            std::string narrowBuild(wideBuild.begin(), wideBuild.end());
            version += " (Build " + narrowBuild + ")";
        }
        
        RegCloseKey(hKey);
    }
    
    return version;
}

std::string Win32PlatformManager::GetProcessorArchitecture()
{
    SYSTEM_INFO systemInfo;
    GetNativeSystemInfo(&systemInfo);
    
    switch (systemInfo.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64:
            return "x64";
        case PROCESSOR_ARCHITECTURE_ARM:
            return "ARM";
        case PROCESSOR_ARCHITECTURE_ARM64:
            return "ARM64";
        case PROCESSOR_ARCHITECTURE_INTEL:
            return "x86";
        default:
            return "Unknown";
    }
}

bool Win32PlatformManager::IsElevated()
{
    BOOL elevated = FALSE;
    HANDLE token = NULL;
    
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        TOKEN_ELEVATION elevation;
        DWORD size = sizeof(elevation);
        
        if (GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size)) {
            elevated = elevation.TokenIsElevated;
        }
        
        CloseHandle(token);
    }
    
    return elevated != FALSE;
}

bool Win32PlatformManager::CanElevate()
{
    HANDLE token = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        return false;
    }
    
    TOKEN_ELEVATION_TYPE elevationType;
    DWORD size = sizeof(elevationType);
    
    bool canElevate = false;
    if (GetTokenInformation(token, TokenElevationType, &elevationType, sizeof(elevationType), &size)) {
        canElevate = (elevationType == TokenElevationTypeLimited);
    }
    
    CloseHandle(token);
    return canElevate;
}

bool Win32PlatformManager::HasWritePermission(const std::filesystem::path& path)
{
    std::wstring widePath = path.wstring();
    
    // Try to create a temporary file to test write permissions
    std::wstring testFile = widePath + L"\\write_test_temp.tmp";
    
    HANDLE hFile = CreateFileW(
        testFile.c_str(),
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_NEW,
        FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE,
        NULL
    );
    
    if (hFile != INVALID_HANDLE_VALUE) {
        CloseHandle(hFile);
        return true;
    }
    
    DWORD error = GetLastError();
    if (error == ERROR_ACCESS_DENIED || error == ERROR_PRIVILEGE_NOT_HELD) {
        return false;
    }
    
    // If the error is something else (like path not existing), 
    // assume we can write if we have the proper privileges
    return IsElevated();
}
