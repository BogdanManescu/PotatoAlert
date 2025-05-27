// Copyright 2025 <github.com/razaqq>
#pragma once

#include "Updater/Core/UpdateOrchestrator.hpp"
#include "Updater/UpdaterFactory.hpp"
#include "Updater/Models/UpdateResult.hpp"
#include "Updater/Models/UpdateConfiguration.hpp"
#include "Updater/Models/ProgressInfo.hpp"
#include "Updater/Models/UpdateError.hpp"

#include <memory>
#include <functional>
#include <future>


namespace PotatoAlert::Updater {

/**
 * @brief Modern updater facade providing backward compatibility
 * 
 * This class provides the same interface as the legacy Updater class but uses
 * the new UpdateOrchestrator architecture internally. It serves as a migration
 * path and maintains compatibility with existing code.
 */
class ModernUpdater
{
public:
    ModernUpdater();
    ~ModernUpdater() = default;

    // Legacy static API for backward compatibility
    static bool UpdateAvailable();
    static bool StartUpdater(std::string_view args = "");
    static bool StartMain(std::string_view args = "");
    static void RemoveTrash();

    // Modern async API
    std::future<UpdateResult> CheckForUpdatesAsync();
    std::future<UpdateResult> RunUpdateAsync();
    void Cancel();

    // Configuration
    void SetConfiguration(const UpdateConfiguration& config);
    const UpdateConfiguration& GetConfiguration() const;

    // Callbacks for progress and error reporting
    void SetProgressCallback(std::function<void(const ProgressInfo&)> callback);
    void SetErrorCallback(std::function<void(const UpdateError&)> callback);

private:
    std::shared_ptr<UpdateOrchestrator> m_orchestrator;
    UpdateConfiguration m_config;

    // Static instance for legacy API
    static std::shared_ptr<ModernUpdater> s_instance;
    static std::shared_ptr<ModernUpdater> GetInstance();
};

} // namespace PotatoAlert::Updater
