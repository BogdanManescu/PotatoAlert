// Copyright 2024 <github.com/razaqq>
#pragma once

#include "Updater/Models/UpdateResult.hpp"

#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>


namespace PotatoAlert::Updater {

struct ProcessInfo
{
    std::uint32_t ProcessId;
    std::string ProcessName;
    std::filesystem::path ExecutablePath;
    bool IsRunning;
};

class IProcessManager
{
public:
    virtual ~IProcessManager() = default;

    // Process enumeration
    virtual std::vector<ProcessInfo> FindProcessesByName(const std::string& processName) = 0;
    virtual std::optional<ProcessInfo> GetProcessInfo(std::uint32_t processId) = 0;
    
    // Process lifecycle
    virtual UpdateResult StartProcess(
        const std::filesystem::path& executablePath,
        const std::string& arguments = "",
        bool elevated = false,
        bool waitForExit = false) = 0;
    
    virtual UpdateResult TerminateProcess(std::uint32_t processId, bool force = false) = 0;
    virtual UpdateResult WaitForProcessExit(std::uint32_t processId, std::chrono::seconds timeout) = 0;
    
    // Current process operations
    virtual void ExitCurrentProcess(std::uint32_t exitCode) = 0;
    virtual std::uint32_t GetCurrentProcessId() = 0;
    
    // Privilege management
    virtual bool IsElevated() = 0;
    virtual bool CanElevate() = 0;
    virtual UpdateResult RequestElevation() = 0;
    
    // Process coordination for updates
    virtual UpdateResult WaitForApplicationExit(const std::string& applicationName, std::chrono::seconds timeout) = 0;
    virtual UpdateResult TerminateApplication(const std::string& applicationName, bool force = false) = 0;
    
    // Special operations
    virtual UpdateResult RestartAsElevated(const std::string& arguments = "") = 0;
    virtual UpdateResult StartUpdater(const std::string& arguments = "") = 0;
    virtual UpdateResult StartMainApplication(const std::string& arguments = "") = 0;
};

}  // namespace PotatoAlert::Updater
