#include "Updater/Platform/linux/LinuxProcessManager.hpp"
#include "Core/String.hpp"
#include <cstring>
#include <fstream>
#include <sstream>
#include <thread>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <fcntl.h>
#include <pwd.h>

using namespace PotatoAlert::Core;
using namespace PotatoAlert::Updater;
using namespace PotatoAlert::Updater::Platform::Linux;

std::future<ProcessResult> LinuxProcessManager::StartProcess(
    const std::string& executable,
    const std::vector<std::string>& arguments,
    const ProcessOptions& options)
{
    return std::async(std::launch::async, [this, executable, arguments, options]() -> ProcessResult {
        try {
            LOG_INFO("Starting process: {} with {} arguments", executable, arguments.size());

            // Prepare command and arguments
            std::vector<char*> argv;
            argv.push_back(const_cast<char*>(executable.c_str()));
            
            for (const auto& arg : arguments) {
                argv.push_back(const_cast<char*>(arg.c_str()));
            }
            argv.push_back(nullptr);

            // Fork the process
            pid_t pid = fork();
            
            if (pid == -1) {
                LOG_ERROR("Failed to fork process: {}", strerror(errno));
                return ProcessResult::ProcessStartFailed;
            }

            if (pid == 0) {
                // Child process
                try {
                    // Set working directory if specified
                    if (!options.WorkingDirectory.empty()) {
                        if (chdir(options.WorkingDirectory.c_str()) != 0) {
                            LOG_ERROR("Failed to change working directory: {}", strerror(errno));
                            _exit(1);
                        }
                    }

                    // Handle elevation if requested
                    if (options.RunElevated) {
                        // Check if we need to use sudo
                        if (getuid() != 0) {
                            // Prepare sudo command
                            std::vector<char*> sudoArgv;
                            sudoArgv.push_back(const_cast<char*>("sudo"));
                            sudoArgv.push_back(const_cast<char*>("-n")); // Non-interactive
                            
                            for (auto* arg : argv) {
                                if (arg != nullptr) {
                                    sudoArgv.push_back(arg);
                                }
                            }
                            sudoArgv.push_back(nullptr);
                            
                            execvp("sudo", sudoArgv.data());
                        } else {
                            execvp(executable.c_str(), argv.data());
                        }
                    } else {
                        execvp(executable.c_str(), argv.data());
                    }
                    
                    // If we reach here, exec failed
                    LOG_ERROR("Failed to execute process: {}", strerror(errno));
                    _exit(1);
                } catch (...) {
                    _exit(1);
                }
            } else {
                // Parent process
                ProcessInfo info;
                info.Id = static_cast<ProcessId>(pid);
                info.Command = executable;
                info.IsRunning = true;
                info.StartTime = std::chrono::system_clock::now();

                {
                    std::lock_guard<std::mutex> lock(m_processesMutex);
                    m_managedProcesses.push_back(info);
                }

                LOG_INFO("Successfully started process {} with PID {}", executable, pid);
                return ProcessResult::Success;
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Exception in StartProcess: {}", e.what());
            return ProcessResult::ProcessStartFailed;
        }
    });
}

std::future<ProcessResult> LinuxProcessManager::TerminateProcess(
    ProcessId processId,
    std::chrono::milliseconds timeout)
{
    return std::async(std::launch::async, [this, processId, timeout]() -> ProcessResult {
        try {
            LOG_INFO("Terminating process with PID {}", processId);

            if (!IsValidProcessId(processId)) {
                return ProcessResult::ProcessNotFound;
            }

            // First, try graceful termination with SIGTERM
            if (SendSignalToProcess(processId, SIGTERM)) {
                // Wait for graceful termination
                if (WaitForProcessTermination(processId, timeout)) {
                    LOG_INFO("Process {} terminated gracefully", processId);
                    return ProcessResult::Success;
                }
            }

            // If graceful termination failed, force kill with SIGKILL
            LOG_WARN("Graceful termination failed for process {}, forcing termination", processId);
            if (SendSignalToProcess(processId, SIGKILL)) {
                // Wait a bit longer for forced termination
                if (WaitForProcessTermination(processId, std::chrono::milliseconds(5000))) {
                    LOG_INFO("Process {} forcefully terminated", processId);
                    return ProcessResult::Success;
                }
            }

            LOG_ERROR("Failed to terminate process {}", processId);
            return ProcessResult::ProcessTerminationFailed;
        } catch (const std::exception& e) {
            LOG_ERROR("Exception in TerminateProcess: {}", e.what());
            return ProcessResult::ProcessTerminationFailed;
        }
    });
}

std::future<bool> LinuxProcessManager::IsProcessRunning(ProcessId processId)
{
    return std::async(std::launch::async, [this, processId]() -> bool {
        return IsValidProcessId(processId);
    });
}

std::future<std::vector<ProcessId>> LinuxProcessManager::FindProcessesByName(const std::string& processName)
{
    return std::async(std::launch::async, [this, processName]() -> std::vector<ProcessId> {
        return GetProcessIdsByName(processName);
    });
}

std::future<ProcessResult> LinuxProcessManager::WaitForProcessExit(
    ProcessId processId,
    std::chrono::milliseconds timeout)
{
    return std::async(std::launch::async, [this, processId, timeout]() -> ProcessResult {
        try {
            if (WaitForProcessTermination(processId, timeout)) {
                return ProcessResult::Success;
            }
            return ProcessResult::ProcessWaitTimeout;
        } catch (const std::exception& e) {
            LOG_ERROR("Exception in WaitForProcessExit: {}", e.what());
            return ProcessResult::ProcessWaitFailed;
        }
    });
}

std::future<bool> LinuxProcessManager::ElevatePrivileges()
{
    return std::async(std::launch::async, []() -> bool {
        // On Linux, we typically use sudo for elevation
        // Check if we're already root
        if (getuid() == 0) {
            return true;
        }

        // Check if sudo is available and configured
        if (system("sudo -n true 2>/dev/null") == 0) {
            LOG_INFO("Sudo privileges are available");
            return true;
        }

        LOG_WARN("Cannot elevate privileges - sudo not available or not configured");
        return false;
    });
}

std::future<bool> LinuxProcessManager::HasElevatedPrivileges()
{
    return std::async(std::launch::async, []() -> bool {
        return getuid() == 0;
    });
}

// Private helper methods

bool LinuxProcessManager::SendSignalToProcess(ProcessId processId, int signal)
{
    int result = kill(static_cast<pid_t>(processId), signal);
    if (result != 0) {
        if (errno != ESRCH) { // ESRCH means process doesn't exist
            LOG_ERROR("Failed to send signal {} to process {}: {}", signal, processId, strerror(errno));
        }
        return false;
    }
    return true;
}

bool LinuxProcessManager::WaitForProcessTermination(ProcessId processId, std::chrono::milliseconds timeout)
{
    auto startTime = std::chrono::steady_clock::now();
    auto timeoutTime = startTime + timeout;

    while (std::chrono::steady_clock::now() < timeoutTime) {
        if (!IsValidProcessId(processId)) {
            return true; // Process has exited
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return false; // Timeout reached
}

std::vector<ProcessId> LinuxProcessManager::GetProcessIdsByName(const std::string& name)
{
    std::vector<ProcessId> pids;
    
    DIR* procDir = opendir("/proc");
    if (!procDir) {
        LOG_ERROR("Failed to open /proc directory: {}", strerror(errno));
        return pids;
    }

    struct dirent* entry;
    while ((entry = readdir(procDir)) != nullptr) {
        // Check if directory name is numeric (PID)
        char* endptr;
        long pid = strtol(entry->d_name, &endptr, 10);
        if (*endptr != '\0' || pid <= 0) {
            continue;
        }

        // Read the command name from /proc/PID/comm
        std::string commPath = "/proc/" + std::string(entry->d_name) + "/comm";
        std::ifstream commFile(commPath);
        if (commFile.is_open()) {
            std::string processName;
            std::getline(commFile, processName);
            
            // Remove trailing newline if present
            if (!processName.empty() && processName.back() == '\n') {
                processName.pop_back();
            }
            
            if (processName == name) {
                pids.push_back(static_cast<ProcessId>(pid));
            }
        }
    }

    closedir(procDir);
    return pids;
}

bool LinuxProcessManager::IsValidProcessId(ProcessId processId)
{
    // Check if process exists by sending signal 0
    int result = kill(static_cast<pid_t>(processId), 0);
    return result == 0;
}

std::string LinuxProcessManager::BuildCommandLine(
    const std::string& executable,
    const std::vector<std::string>& arguments)
{
    std::string commandLine = executable;
    for (const auto& arg : arguments) {
        commandLine += " \"" + arg + "\"";
    }
    return commandLine;
}

ProcessResult LinuxProcessManager::ExecuteCommand(
    const std::string& command,
    const ProcessOptions& options)
{
    // This method could be used for simple command execution
    // For now, we'll implement it as a basic system() call wrapper
    int result = system(command.c_str());
    
    if (result == -1) {
        return ProcessResult::ProcessStartFailed;
    }
    
    // Check exit status
    if (WIFEXITED(result)) {
        int exitCode = WEXITSTATUS(result);
        if (exitCode == 0) {
            return ProcessResult::Success;
        } else {
            return ProcessResult::ProcessTerminationFailed;
        }
    }
    
    return ProcessResult::ProcessTerminationFailed;
}
