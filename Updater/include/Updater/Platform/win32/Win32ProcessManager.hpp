// Copyright 2024 <github.com/razaqq>
#pragma once

#include "Updater/Interfaces/IProcessManager.hpp"

#include <memory>


namespace PotatoAlert::Updater {

class Win32ProcessManager : public IProcessManager
{
public:
    Win32ProcessManager();
    ~Win32ProcessManager() override;

    // IProcessManager implementation
    std::vector<ProcessInfo> FindProcessesByName(const std::string& processName) override;
    std::optional<ProcessInfo> GetProcessInfo(std::uint32_t processId) override;
    
    UpdateResult StartProcess(
        const std::filesystem::path& executablePath,
        const std::string& arguments = "",
        bool elevated = false,
        bool waitForExit = false) override;
    
    UpdateResult TerminateProcess(std::uint32_t processId, bool force = false) override;
    UpdateResult WaitForProcessExit(std::uint32_t processId, std::chrono::seconds timeout) override;
    
    void ExitCurrentProcess(std::uint32_t exitCode) override;
    std::uint32_t GetCurrentProcessId() override;
    
    bool IsElevated() override;
    bool CanElevate() override;
    UpdateResult RequestElevation() override;
    
    UpdateResult WaitForApplicationExit(const std::string& applicationName, std::chrono::seconds timeout) override;
    UpdateResult TerminateApplication(const std::string& applicationName, bool force = false) override;
    
    UpdateResult RestartAsElevated(const std::string& arguments = "") override;
    UpdateResult StartUpdater(const std::string& arguments = "") override;
    UpdateResult StartMainApplication(const std::string& arguments = "") override;

private:
    struct ElevationInfo
    {
        bool IsElevated;
        bool CanElevate;
    };
    
    ElevationInfo GetElevationInfo();
    std::optional<std::uint32_t> FindProcessByName(const std::string& processName);
};

}  // namespace PotatoAlert::Updater
