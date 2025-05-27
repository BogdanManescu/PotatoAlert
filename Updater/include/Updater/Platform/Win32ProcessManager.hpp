// Copyright 2024 <github.com/razaqq>
#pragma once

#include "Updater/Interfaces/IProcessManager.hpp"

#include <windows.h>
#include <tlhelp32.h>

#include <vector>
#include <memory>
#include <mutex>


namespace PotatoAlert::Updater {

class Win32ProcessManager : public IProcessManager
{
public:
    Win32ProcessManager();
    ~Win32ProcessManager() override = default;

    // IProcessManager implementation
    void SetProgressReporter(std::shared_ptr<IProgressReporter> reporter) override;

    std::future<UpdateResult> StartProcess(
        const std::filesystem::path& executablePath,
        const std::vector<std::string>& arguments,
        const ProcessStartInfo& startInfo) override;

    std::future<UpdateResult> StopProcess(
        ProcessId processId,
        std::chrono::milliseconds timeout) override;

    std::future<UpdateResult> WaitForProcessExit(
        ProcessId processId,
        std::chrono::milliseconds timeout) override;

    std::vector<ProcessInfo> GetRunningProcesses(
        const std::string& processName) const override;

    std::vector<ProcessInfo> GetProcessesByPath(
        const std::filesystem::path& executablePath) const override;

    bool IsProcessRunning(ProcessId processId) const override;
    bool IsElevated() const override;
    
    std::future<UpdateResult> RequestElevation(
        const std::filesystem::path& executablePath,
        const std::vector<std::string>& arguments) override;

    UpdateResult GetLastError() const override;

private:
    UpdateResult StartProcessInternal(
        const std::filesystem::path& executablePath,
        const std::vector<std::string>& arguments,
        const ProcessStartInfo& startInfo);

    UpdateResult StopProcessInternal(ProcessId processId, std::chrono::milliseconds timeout);
    UpdateResult WaitForProcessExitInternal(ProcessId processId, std::chrono::milliseconds timeout);
    
    UpdateResult RequestElevationInternal(
        const std::filesystem::path& executablePath,
        const std::vector<std::string>& arguments);

    std::optional<ProcessInfo> GetProcessInfo(DWORD processId) const;
    std::string BuildCommandLine(const std::vector<std::string>& arguments) const;
    
    void ReportProgress(const std::string& operation, double progress);

    std::shared_ptr<IProgressReporter> m_progressReporter;
    mutable std::mutex m_mutex;
    mutable UpdateResult m_lastError{UpdateResult::Success};
};

}  // namespace PotatoAlert::Updater
