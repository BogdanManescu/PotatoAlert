// Copyright 2025 <github.com/razaqq>
#pragma once

#include "Updater/Core/UpdateOrchestrator.hpp"
#include "Updater/Models/UpdateConfiguration.hpp"

#include <memory>


namespace PotatoAlert::Updater {

/**
 * @brief Factory class for creating and configuring UpdateOrchestrator instances
 * 
 * This factory handles dependency injection and platform-specific service creation.
 * It encapsulates the complexity of setting up all the required services and provides
 * a simple interface for creating properly configured updater instances.
 */
class UpdaterFactory
{
public:
    /**
     * @brief Create a new UpdateOrchestrator with default platform-specific implementations
     * @param config Update configuration to use
     * @return Shared pointer to configured UpdateOrchestrator
     */
    static std::shared_ptr<UpdateOrchestrator> CreateDefault(const UpdateConfiguration& config = {});

    /**
     * @brief Create UpdateOrchestrator for Windows platform
     * @param config Update configuration to use
     * @return Shared pointer to configured UpdateOrchestrator
     */
    static std::shared_ptr<UpdateOrchestrator> CreateForWindows(const UpdateConfiguration& config = {});

    /**
     * @brief Create UpdateOrchestrator for Linux platform
     * @param config Update configuration to use
     * @return Shared pointer to configured UpdateOrchestrator
     */
    static std::shared_ptr<UpdateOrchestrator> CreateForLinux(const UpdateConfiguration& config = {});

    /**
     * @brief Create default configuration with sensible defaults
     * @return Default UpdateConfiguration
     */
    static UpdateConfiguration CreateDefaultConfiguration();

private:
    UpdaterFactory() = default;
};

} // namespace PotatoAlert::Updater
