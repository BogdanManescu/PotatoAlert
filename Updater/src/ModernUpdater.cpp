// Copyright 2025 <github.com/razaqq>

#include "Updater/ModernUpdater.hpp"

#include "Core/Log.hpp"
#include "Core/Process.hpp"

using namespace PotatoAlert::Updater;


// Static instance for legacy API
std::shared_ptr<ModernUpdater> ModernUpdater::s_instance = nullptr;

ModernUpdater::ModernUpdater()
    : m_config(UpdaterFactory::CreateDefaultConfiguration())
{
    m_orchestrator = UpdaterFactory::CreateDefault(m_config);
}

std::shared_ptr<ModernUpdater> ModernUpdater::GetInstance()
{
    if (!s_instance)
    {
        s_instance = std::shared_ptr<ModernUpdater>(new ModernUpdater());
    }
    return s_instance;
}

bool ModernUpdater::UpdateAvailable()
{
    try
    {
        auto instance = GetInstance();
        auto future = instance->CheckForUpdatesAsync();
        
        // Wait for result with timeout
        if (future.wait_for(std::chrono::seconds(30)) == std::future_status::ready)
        {
            UpdateResult result = future.get();
            return result == UpdateResult::Success;
        }
        else
        {
            LOG_ERROR("Update check timed out");
            return false;
        }
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Error checking for updates: {}", e.what());
        return false;
    }
}

bool ModernUpdater::StartUpdater(std::string_view args)
{
    try
    {
        // Use the platform manager to start the updater process
        std::string updaterPath = "PotatoUpdater.exe";
        return PotatoAlert::Core::CreateNewProcess(updaterPath, args, true);
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Error starting updater: {}", e.what());
        return false;
    }
}

bool ModernUpdater::StartMain(std::string_view args)
{
    try
    {
        std::string mainPath = "PotatoAlert.exe";
        return PotatoAlert::Core::CreateNewProcess(mainPath, args, false);
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Error starting main application: {}", e.what());
        return false;
    }
}

void ModernUpdater::RemoveTrash()
{
    try
    {
        auto instance = GetInstance();
        // Use the file manager to clean up trash files
        auto fileManager = instance->m_orchestrator->GetFileManager();
        auto currentPath = std::filesystem::current_path();
        auto result = fileManager->CleanupTrash(currentPath);
        
        if (result != UpdateResult::Success)
        {
            LOG_ERROR("Failed to remove trash files");
        }
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Error removing trash files: {}", e.what());
    }
}

std::future<UpdateResult> ModernUpdater::CheckForUpdatesAsync()
{
    return m_orchestrator->CheckForUpdates();
}

std::future<UpdateResult> ModernUpdater::RunUpdateAsync()
{
    return m_orchestrator->PerformUpdate();
}

void ModernUpdater::Cancel()
{
    m_orchestrator->Cancel();
}

void ModernUpdater::SetConfiguration(const UpdateConfiguration& config)
{
    m_config = config;
    m_orchestrator->SetConfiguration(config);
}

const UpdateConfiguration& ModernUpdater::GetConfiguration() const
{
    return m_config;
}

void ModernUpdater::SetProgressCallback(std::function<void(const ProgressInfo&)> callback)
{
    m_orchestrator->SetProgressCallback(std::move(callback));
}

void ModernUpdater::SetErrorCallback(std::function<void(const UpdateError&)> callback)
{
    m_orchestrator->SetErrorCallback(std::move(callback));
}
