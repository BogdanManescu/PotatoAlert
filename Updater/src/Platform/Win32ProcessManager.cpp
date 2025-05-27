// Copyright 2024 <github.com/razaqq>

#include "Updater/Platform/Win32ProcessManager.hpp"
#include "Updater/Models/ProgressInfo.hpp"
#include "Updater/Interfaces/IProgressReporter.hpp"

#include "Core/Log.hpp"
#include "Core/Encoding.hpp"

#include <future>
#include <thread>
#include <shellapi.h>
#include <psapi.h>
#include <sstream>


using namespace PotatoAlert::Updater;
using namespace PotatoAlert::Core;


Win32ProcessManager::Win32ProcessManager() = default;

void Win32ProcessManager::SetProgressReporter(std::shared_ptr<IProgressReporter> reporter)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_progressReporter = reporter;
}

std::future<UpdateResult> Win32ProcessManager::StartProcess(
    const std::filesystem::path& executablePath,
    const std::vector<std::string>& arguments,
    const ProcessStartInfo& startInfo)
{
    return std::async(std::launch::async, [this, executablePath, arguments, startInfo]() {
        return StartProcessInternal(executablePath, arguments, startInfo);
    });
}

std::future<UpdateResult> Win32ProcessManager::StopProcess(
    ProcessId processId,
    std::chrono::milliseconds timeout)
{
    return std::async(std::launch::async, [this, processId, timeout]() {
        return StopProcessInternal(processId, timeout);
    });
}

std::future<UpdateResult> Win32ProcessManager::WaitForProcessExit(
    ProcessId processId,
    std::chrono::milliseconds timeout)
{
    return std::async(std::launch::async, [this, processId, timeout]() {
        return WaitForProcessExitInternal(processId, timeout);
    });
}

std::vector<ProcessInfo> Win32ProcessManager::GetRunningProcesses(const std::string& processName) const
{
    std::vector<ProcessInfo> processes;
    
    try
    {
        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnapshot == INVALID_HANDLE_VALUE)
        {
            LOG_ERROR("Failed to create process snapshot: {}", GetLastError());
            std::lock_guard<std::mutex> lock(m_mutex);
            m_lastError = UpdateResult::SystemError;
            return processes;
        }

        PROCESSENTRY32W pe;
        ZeroMemory(&pe, sizeof(pe));
        pe.dwSize = sizeof(pe);

        if (Process32FirstW(hSnapshot, &pe))
        {
            do
            {
                std::string currentProcessName = FromWideString(pe.szExeFile);
                if (processName.empty() || currentProcessName == processName)
                {
                    auto processInfo = GetProcessInfo(pe.th32ProcessID);
                    if (processInfo)
                    {
                        processes.push_back(*processInfo);
                    }
                }
            } while (Process32NextW(hSnapshot, &pe));
        }

        CloseHandle(hSnapshot);
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Exception getting running processes: {}", e.what());
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lastError = UpdateResult::InternalError;
    }

    return processes;
}

std::vector<ProcessInfo> Win32ProcessManager::GetProcessesByPath(
    const std::filesystem::path& executablePath) const
{
    std::vector<ProcessInfo> processes;
    
    try
    {
        auto allProcesses = GetRunningProcesses("");
        std::string targetPath = executablePath.string();
        
        for (const auto& process : allProcesses)
        {
            if (process.ExecutablePath == targetPath)
            {
                processes.push_back(process);
            }
        }
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Exception getting processes by path: {}", e.what());
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lastError = UpdateResult::InternalError;
    }

    return processes;
}

bool Win32ProcessManager::IsProcessRunning(ProcessId processId) const
{
    try
    {
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, processId);
        if (hProcess == nullptr)
        {
            return false;
        }

        DWORD exitCode;
        bool result = GetExitCodeProcess(hProcess, &exitCode);
        CloseHandle(hProcess);

        return result && (exitCode == STILL_ACTIVE);
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Exception checking if process is running: {}", e.what());
        return false;
    }
}

bool Win32ProcessManager::IsElevated() const
{
    try
    {
        HANDLE hToken;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
        {
            return false;
        }

        TOKEN_ELEVATION_TYPE elevationType;
        DWORD returnLength = 0;
        bool result = GetTokenInformation(hToken, TokenElevationType, &elevationType, 
            sizeof(elevationType), &returnLength);
        
        CloseHandle(hToken);

        return result && (elevationType == TokenElevationTypeFull);
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Exception checking elevation status: {}", e.what());
        return false;
    }
}

std::future<UpdateResult> Win32ProcessManager::RequestElevation(
    const std::filesystem::path& executablePath,
    const std::vector<std::string>& arguments)
{
    return std::async(std::launch::async, [this, executablePath, arguments]() {
        return RequestElevationInternal(executablePath, arguments);
    });
}

UpdateResult Win32ProcessManager::GetLastError() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastError;
}

UpdateResult Win32ProcessManager::StartProcessInternal(
    const std::filesystem::path& executablePath,
    const std::vector<std::string>& arguments,
    const ProcessStartInfo& startInfo)
{
    try
    {
        ReportProgress("Starting process", 0.0);

        std::wstring wExecutablePath = executablePath.wstring();
        std::string commandLine = BuildCommandLine(arguments);
        std::wstring wCommandLine = ToWideString(commandLine);

        STARTUPINFOW si;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        
        if (startInfo.WindowState == ProcessWindowState::Hidden)
        {
            si.dwFlags |= STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_HIDE;
        }

        PROCESS_INFORMATION pi;
        ZeroMemory(&pi, sizeof(pi));

        DWORD creationFlags = 0;
        if (startInfo.CreateNewConsole)
        {
            creationFlags |= CREATE_NEW_CONSOLE;
        }
        else if (startInfo.WindowState == ProcessWindowState::Hidden)
        {
            creationFlags |= CREATE_NO_WINDOW;
        }

        std::wstring wWorkingDirectory;
        LPCWSTR lpCurrentDirectory = nullptr;
        if (!startInfo.WorkingDirectory.empty())
        {
            wWorkingDirectory = startInfo.WorkingDirectory.wstring();
            lpCurrentDirectory = wWorkingDirectory.c_str();
        }

        BOOL result = CreateProcessW(
            wExecutablePath.c_str(),    // Application name
            wCommandLine.data(),        // Command line (modifiable)
            nullptr,                    // Process security attributes
            nullptr,                    // Thread security attributes
            FALSE,                      // Inherit handles
            creationFlags,              // Creation flags
            nullptr,                    // Environment
            lpCurrentDirectory,         // Working directory
            &si,                        // Startup info
            &pi                         // Process info
        );

        if (!result)
        {
            DWORD error = ::GetLastError();
            LOG_ERROR("Failed to start process {}: {}", executablePath.string(), error);
            return UpdateResult::ProcessStartFailed;
        }

        // Close thread handle immediately as we don't need it
        CloseHandle(pi.hThread);
        
        if (startInfo.WaitForCompletion)
        {
            ReportProgress("Waiting for process completion", 0.5);
            
            DWORD waitResult = WaitForSingleObject(pi.hProcess, startInfo.Timeout.count());
            if (waitResult == WAIT_TIMEOUT)
            {
                LOG_WARN("Process did not complete within timeout: {}", executablePath.string());
                TerminateProcess(pi.hProcess, 1);
                CloseHandle(pi.hProcess);
                return UpdateResult::ProcessTimeout;
            }
            else if (waitResult != WAIT_OBJECT_0)
            {
                LOG_ERROR("Error waiting for process completion: {}", GetLastError());
                CloseHandle(pi.hProcess);
                return UpdateResult::ProcessStartFailed;
            }

            DWORD exitCode;
            if (GetExitCodeProcess(pi.hProcess, &exitCode))
            {
                if (exitCode != 0)
                {
                    LOG_WARN("Process exited with non-zero code: {}", exitCode);
                }
            }
        }

        CloseHandle(pi.hProcess);

        ReportProgress("Process started successfully", 1.0);
        LOG_INFO("Successfully started process: {}", executablePath.string());
        return UpdateResult::Success;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Exception starting process: {}", e.what());
        return UpdateResult::InternalError;
    }
}

UpdateResult Win32ProcessManager::StopProcessInternal(ProcessId processId, std::chrono::milliseconds timeout)
{
    try
    {
        ReportProgress("Stopping process", 0.0);

        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, processId);
        if (hProcess == nullptr)
        {
            DWORD error = ::GetLastError();
            if (error == ERROR_INVALID_PARAMETER)
            {
                // Process doesn't exist, consider it stopped
                return UpdateResult::Success;
            }
            LOG_ERROR("Failed to open process for termination: {}", error);
            return UpdateResult::ProcessStopFailed;
        }

        // First try graceful shutdown by posting WM_CLOSE to all windows
        EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
            DWORD windowProcessId;
            GetWindowThreadProcessId(hwnd, &windowProcessId);
            
            if (windowProcessId == static_cast<DWORD>(lParam))
            {
                PostMessage(hwnd, WM_CLOSE, 0, 0);
            }
            return TRUE;
        }, static_cast<LPARAM>(processId));

        ReportProgress("Waiting for graceful shutdown", 0.3);

        // Wait for graceful shutdown
        DWORD waitResult = WaitForSingleObject(hProcess, timeout.count() / 2);
        if (waitResult == WAIT_OBJECT_0)
        {
            CloseHandle(hProcess);
            ReportProgress("Process stopped gracefully", 1.0);
            LOG_INFO("Process {} stopped gracefully", processId);
            return UpdateResult::Success;
        }

        // If graceful shutdown failed, force terminate
        ReportProgress("Force terminating process", 0.7);
        
        if (!TerminateProcess(hProcess, 1))
        {
            DWORD error = ::GetLastError();
            LOG_ERROR("Failed to terminate process {}: {}", processId, error);
            CloseHandle(hProcess);
            return UpdateResult::ProcessStopFailed;
        }

        // Wait for termination to complete
        waitResult = WaitForSingleObject(hProcess, timeout.count() / 2);
        CloseHandle(hProcess);

        if (waitResult != WAIT_OBJECT_0)
        {
            LOG_ERROR("Process {} did not terminate within timeout", processId);
            return UpdateResult::ProcessTimeout;
        }

        ReportProgress("Process terminated", 1.0);
        LOG_INFO("Process {} terminated", processId);
        return UpdateResult::Success;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Exception stopping process: {}", e.what());
        return UpdateResult::InternalError;
    }
}

UpdateResult Win32ProcessManager::WaitForProcessExitInternal(
    ProcessId processId, 
    std::chrono::milliseconds timeout)
{
    try
    {
        ReportProgress("Waiting for process exit", 0.0);

        HANDLE hProcess = OpenProcess(SYNCHRONIZE, FALSE, processId);
        if (hProcess == nullptr)
        {
            // Process doesn't exist or already exited
            return UpdateResult::Success;
        }

        DWORD waitResult = WaitForSingleObject(hProcess, timeout.count());
        CloseHandle(hProcess);

        if (waitResult == WAIT_OBJECT_0)
        {
            ReportProgress("Process exited", 1.0);
            LOG_INFO("Process {} exited", processId);
            return UpdateResult::Success;
        }
        else if (waitResult == WAIT_TIMEOUT)
        {
            LOG_WARN("Timeout waiting for process {} to exit", processId);
            return UpdateResult::ProcessTimeout;
        }
        else
        {
            LOG_ERROR("Error waiting for process {} to exit: {}", processId, GetLastError());
            return UpdateResult::SystemError;
        }
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Exception waiting for process exit: {}", e.what());
        return UpdateResult::InternalError;
    }
}

UpdateResult Win32ProcessManager::RequestElevationInternal(
    const std::filesystem::path& executablePath,
    const std::vector<std::string>& arguments)
{
    try
    {
        ReportProgress("Requesting elevation", 0.0);

        std::wstring wExecutablePath = executablePath.wstring();
        std::string commandLine = BuildCommandLine(arguments);
        std::wstring wParameters = ToWideString(commandLine);

        SHELLEXECUTEINFOW sei;
        ZeroMemory(&sei, sizeof(sei));
        sei.cbSize = sizeof(sei);
        sei.fMask = SEE_MASK_NOCLOSEPROCESS;
        sei.lpVerb = L"runas";  // Request elevation
        sei.lpFile = wExecutablePath.c_str();
        sei.lpParameters = wParameters.c_str();
        sei.nShow = SW_NORMAL;

        if (!ShellExecuteExW(&sei))
        {
            DWORD error = ::GetLastError();
            if (error == ERROR_CANCELLED)
            {
                LOG_INFO("User cancelled elevation request");
                return UpdateResult::ElevationCancelled;
            }
            LOG_ERROR("Failed to request elevation: {}", error);
            return UpdateResult::ElevationFailed;
        }

        if (sei.hProcess)
        {
            CloseHandle(sei.hProcess);
        }

        ReportProgress("Elevation requested", 1.0);
        LOG_INFO("Successfully requested elevation for: {}", executablePath.string());
        return UpdateResult::Success;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Exception requesting elevation: {}", e.what());
        return UpdateResult::InternalError;
    }
}

std::optional<ProcessInfo> Win32ProcessManager::GetProcessInfo(DWORD processId) const
{
    try
    {
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
        if (hProcess == nullptr)
        {
            return std::nullopt;
        }

        ProcessInfo info;
        info.ProcessId = processId;

        // Get process name
        WCHAR processPath[MAX_PATH];
        DWORD pathSize = MAX_PATH;
        if (QueryFullProcessImageNameW(hProcess, 0, processPath, &pathSize))
        {
            info.ExecutablePath = FromWideString(processPath);
            info.ProcessName = std::filesystem::path(info.ExecutablePath).filename().string();
        }
        else
        {
            // Fallback to module name
            HMODULE hMod;
            DWORD cbNeeded;
            if (EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeeded))
            {
                WCHAR moduleName[MAX_PATH];
                if (GetModuleBaseNameW(hProcess, hMod, moduleName, MAX_PATH))
                {
                    info.ProcessName = FromWideString(moduleName);
                }
            }
        }

        // Get creation time
        FILETIME creationTime, exitTime, kernelTime, userTime;
        if (GetProcessTimes(hProcess, &creationTime, &exitTime, &kernelTime, &userTime))
        {
            ULARGE_INTEGER uli;
            uli.LowPart = creationTime.dwLowDateTime;
            uli.HighPart = creationTime.dwHighDateTime;
            
            // Convert to system_clock time_point (Windows FILETIME epoch is 1601-01-01)
            const auto windowsEpoch = std::chrono::microseconds(11644473600000000ULL);
            auto timeSinceEpoch = std::chrono::microseconds(uli.QuadPart / 10) - windowsEpoch;
            info.StartTime = std::chrono::system_clock::time_point(timeSinceEpoch);
        }

        CloseHandle(hProcess);
        return info;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Exception getting process info: {}", e.what());
        return std::nullopt;
    }
}

std::string Win32ProcessManager::BuildCommandLine(const std::vector<std::string>& arguments) const
{
    if (arguments.empty())
    {
        return "";
    }

    std::ostringstream oss;
    for (size_t i = 0; i < arguments.size(); ++i)
    {
        if (i > 0)
        {
            oss << " ";
        }

        const auto& arg = arguments[i];
        if (arg.find(' ') != std::string::npos || arg.find('\t') != std::string::npos)
        {
            // Argument contains spaces, quote it
            oss << "\"" << arg << "\"";
        }
        else
        {
            oss << arg;
        }
    }

    return oss.str();
}

void Win32ProcessManager::ReportProgress(const std::string& operation, double progress)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_progressReporter)
    {
        ProgressInfo progressInfo;
        progressInfo.Stage = UpdateStage::ProcessManagement;
        progressInfo.Message = operation;
        progressInfo.Progress = progress;
        m_progressReporter->ReportProgress(progressInfo);
    }
}
