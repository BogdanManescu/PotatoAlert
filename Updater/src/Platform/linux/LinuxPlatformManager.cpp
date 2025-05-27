#include "Updater/Platform/linux/LinuxPlatformManager.hpp"
#include "Updater/Platform/linux/LinuxProcessManager.hpp"
#include "Updater/Services/FileManager.hpp"
#include "Core/String.hpp"
#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <pwd.h>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <climits>

using namespace PotatoAlert::Core;
using namespace PotatoAlert::Updater;
using namespace PotatoAlert::Updater::Platform::Linux;

LinuxPlatformManager::LinuxPlatformManager()
{
    InitializeSystemInfo();
}

std::string LinuxPlatformManager::GetPlatformString()
{
    if (m_platformString.empty()) {
        m_platformString = GetLinuxDistribution();
    }
    return m_platformString;
}

std::string LinuxPlatformManager::GetArchitectureString()
{
    if (m_architectureString.empty()) {
        m_architectureString = GetProcessorArchitecture();
    }
    return m_architectureString;
}

std::unique_ptr<IFileManager> LinuxPlatformManager::CreateFileManager()
{
    return std::make_unique<Services::FileManager>();
}

std::unique_ptr<IProcessManager> LinuxPlatformManager::CreateProcessManager()
{
    return std::make_unique<LinuxProcessManager>();
}

bool LinuxPlatformManager::RequiresElevationForUpdates()
{
    // Check if the application directory requires root privileges to write
    auto appDir = GetApplicationDirectory();
    return !CanWriteToApplicationDirectory();
}

bool LinuxPlatformManager::SupportsServiceInstallation()
{
    // Check if systemd is available
    std::filesystem::path systemctlPath = "/usr/bin/systemctl";
    if (std::filesystem::exists(systemctlPath)) {
        return true;
    }
    
    // Check for other init systems
    std::filesystem::path initPath = "/sbin/init";
    if (std::filesystem::exists(initPath)) {
        return true;
    }
    
    return false;
}

std::uint64_t LinuxPlatformManager::GetAvailableDiskSpace(const std::filesystem::path& path)
{
    struct statvfs stat;
    
    if (statvfs(path.c_str(), &stat) == 0) {
        return static_cast<std::uint64_t>(stat.f_bavail) * stat.f_frsize;
    }
    
    LOG_ERROR("Failed to get disk space for path: {}, errno: {}", path.string(), errno);
    return 0;
}

std::string LinuxPlatformManager::GetSystemVersion()
{
    if (m_systemVersion.empty()) {
        m_systemVersion = GetLinuxDistribution();
    }
    return m_systemVersion;
}

bool LinuxPlatformManager::IsSystemCompatible()
{
    // Check kernel version - require at least Linux 3.0
    struct utsname unameData;
    if (uname(&unameData) == 0) {
        std::string release(unameData.release);
        
        // Parse major version
        size_t dotPos = release.find('.');
        if (dotPos != std::string::npos) {
            try {
                int majorVersion = std::stoi(release.substr(0, dotPos));
                return majorVersion >= 3;
            } catch (const std::exception& e) {
                LOG_WARN("Failed to parse kernel version: {}", e.what());
            }
        }
    }
    
    LOG_WARN("Could not determine kernel version, assuming compatible");
    return true;
}

std::filesystem::path LinuxPlatformManager::GetApplicationDirectory()
{
    // Get the directory where the current executable is located
    char result[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
    
    if (count != -1) {
        std::filesystem::path appPath(std::string(result, count));
        return appPath.parent_path();
    }
    
    LOG_ERROR("Failed to get application directory");
    return std::filesystem::current_path();
}

std::filesystem::path LinuxPlatformManager::GetTempDirectory()
{
    // Check environment variables first
    const char* tmpDir = getenv("TMPDIR");
    if (tmpDir && strlen(tmpDir) > 0) {
        return std::filesystem::path(tmpDir);
    }
    
    tmpDir = getenv("TMP");
    if (tmpDir && strlen(tmpDir) > 0) {
        return std::filesystem::path(tmpDir);
    }
    
    tmpDir = getenv("TEMP");
    if (tmpDir && strlen(tmpDir) > 0) {
        return std::filesystem::path(tmpDir);
    }
    
    // Fall back to standard temp directory
    return std::filesystem::path("/tmp");
}

std::filesystem::path LinuxPlatformManager::GetConfigDirectory()
{
    // Check XDG_CONFIG_HOME first
    const char* xdgConfigHome = getenv("XDG_CONFIG_HOME");
    if (xdgConfigHome && strlen(xdgConfigHome) > 0) {
        std::filesystem::path configPath(xdgConfigHome);
        configPath /= "PotatoAlert";
        return configPath;
    }
    
    // Fall back to ~/.config
    const char* home = getenv("HOME");
    if (home && strlen(home) > 0) {
        std::filesystem::path configPath(home);
        configPath /= ".config" / "PotatoAlert";
        return configPath;
    }
    
    // Last resort: use application directory
    LOG_WARN("Could not determine config directory, using application directory");
    return GetApplicationDirectory();
}

bool LinuxPlatformManager::CanWriteToApplicationDirectory()
{
    return HasWritePermission(GetApplicationDirectory());
}

UpdateResult LinuxPlatformManager::RequestWritePermissions(const std::filesystem::path& path)
{
    if (HasWritePermission(path)) {
        return UpdateResult::Success;
    }
    
    // Check if we can use sudo
    if (getuid() != 0) {
        // Test if sudo is available and configured
        int result = system("sudo -n true 2>/dev/null");
        if (result == 0) {
            LOG_INFO("Sudo privileges are available for write permissions to: {}", path.string());
            return UpdateResult::Success;
        } else {
            LOG_WARN("Sudo is required but not available for path: {}", path.string());
            return UpdateResult::InsufficientPrivileges;
        }
    }
    
    LOG_ERROR("Cannot obtain write permissions for path: {}", path.string());
    return UpdateResult::InsufficientPrivileges;
}

// Private helper methods

bool LinuxPlatformManager::InitializeSystemInfo()
{
    try {
        m_platformString = GetLinuxDistribution();
        m_architectureString = GetProcessorArchitecture();
        m_systemVersion = m_platformString;
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to initialize system info: {}", e.what());
        return false;
    }
}

std::string LinuxPlatformManager::GetLinuxDistribution()
{
    // Try to read /etc/os-release first (standard)
    std::string osRelease = ReadFile("/etc/os-release");
    if (!osRelease.empty()) {
        std::istringstream stream(osRelease);
        std::string line;
          while (std::getline(stream, line)) {
            if (line.find("PRETTY_NAME=") == 0) {
                // Extract value between quotes
                size_t start = line.find('"');
                size_t end = line.rfind('"');
                if (start != std::string::npos && end != std::string::npos && end > start) {
                    return line.substr(start + 1, end - start - 1);
                }
            }
        }
    }
    
    // Fall back to uname
    struct utsname unameData;
    if (uname(&unameData) == 0) {
        return std::string(unameData.sysname) + " " + std::string(unameData.release);
    }
    
    return "Linux (Unknown Distribution)";
}

std::string LinuxPlatformManager::GetProcessorArchitecture()
{
    struct utsname unameData;
    if (uname(&unameData) == 0) {
        std::string machine(unameData.machine);
        
        // Normalize architecture names
        if (machine == "x86_64" || machine == "amd64") {
            return "x64";
        } else if (machine == "i386" || machine == "i686") {
            return "x86";
        } else if (machine.find("arm") == 0) {
            if (machine.find("64") != std::string::npos) {
                return "ARM64";
            } else {
                return "ARM";
            }
        } else if (machine.find("aarch64") == 0) {
            return "ARM64";
        }
        
        return machine;
    }
    
    return "Unknown";
}

bool LinuxPlatformManager::HasWritePermission(const std::filesystem::path& path)
{
    // Test write permission by trying to create a temporary file
    std::filesystem::path testFile = path / ".write_test_temp";
    
    try {
        std::ofstream file(testFile, std::ios::out | std::ios::trunc);
        if (file.is_open()) {
            file << "test";
            file.close();
            
            // Clean up
            std::filesystem::remove(testFile);
            return true;
        }
    } catch (const std::exception& e) {
        LOG_DEBUG("Write permission test failed: {}", e.what());
    }
    
    // Fall back to access() system call
    return access(path.c_str(), W_OK) == 0;
}

std::string LinuxPlatformManager::ReadFile(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        return "";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string LinuxPlatformManager::ExecuteCommand(const std::string& command)
{
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        return "";
    }
    
    std::string result;
    char buffer[256];
    
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    
    pclose(pipe);
    
    // Remove trailing newline if present
    if (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }
    
    return result;
}
