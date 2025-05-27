#pragma once

#include "Updater/Interfaces/IProcessManager.hpp"
#include "Core/Log.hpp"
#include <memory>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

namespace PotatoAlert::Updater::Platform::Linux {

class LinuxProcessManager : public IProcessManager
{
public:
    LinuxProcessManager() = default;
    ~LinuxProcessManager() override = default;

    // IProcessManager interface
    std::future<ProcessResult> StartProcess(
        const std::string& executable,
        const std::vector<std::string>& arguments,
        const ProcessOptions& options = {}
    ) override;

    std::future<ProcessResult> TerminateProcess(
        ProcessId processId,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(5000)
    ) override;

    std::future<bool> IsProcessRunning(ProcessId processId) override;
    std::future<std::vector<ProcessId>> FindProcessesByName(const std::string& processName) override;
    std::future<ProcessResult> WaitForProcessExit(
        ProcessId processId,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(30000)
    ) override;
    std::future<bool> ElevatePrivileges() override;
    std::future<bool> HasElevatedPrivileges() override;

private:
    struct ProcessInfo
    {
        ProcessId Id;
        std::string Command;
        bool IsRunning;
        std::chrono::system_clock::time_point StartTime;
    };

    std::vector<ProcessInfo> m_managedProcesses;
    mutable std::mutex m_processesMutex;

    // Helper methods
    bool SendSignalToProcess(ProcessId processId, int signal);
    bool WaitForProcessTermination(ProcessId processId, std::chrono::milliseconds timeout);
    std::vector<ProcessId> GetProcessIdsByName(const std::string& name);
    bool IsValidProcessId(ProcessId processId);
    std::string BuildCommandLine(const std::string& executable, const std::vector<std::string>& arguments);
    ProcessResult ExecuteCommand(const std::string& command, const ProcessOptions& options);
};

} // namespace PotatoAlert::Updater::Platform::Linux
