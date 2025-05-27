// Copyright 2025 <github.com/razaqq>

#include "Updater/UpdaterFactory.hpp"

#include "Updater/Services/GitHubDownloader.hpp"
#include "Updater/Services/FileManager.hpp"
#include "Updater/Services/BackupManager.hpp"
#include "Updater/Services/CryptoVerifier.hpp"
#include "Updater/Services/ProgressReporter.hpp"

#ifdef WIN32
#include "Updater/Platform/win32/Win32PlatformManager.hpp"
#include "Updater/Platform/Win32ProcessManager.hpp"
#else
#include "Updater/Platform/linux/LinuxPlatformManager.hpp"
#include "Updater/Platform/linux/LinuxProcessManager.hpp"
#endif

using namespace PotatoAlert::Updater;


std::shared_ptr<UpdateOrchestrator> UpdaterFactory::CreateDefault(const UpdateConfiguration& config)
{
#ifdef WIN32
    return CreateForWindows(config);
#else
    return CreateForLinux(config);
#endif
}

std::shared_ptr<UpdateOrchestrator> UpdaterFactory::CreateForWindows(const UpdateConfiguration& config)
{
#ifdef WIN32
    // Create platform-specific services
    auto platformManager = std::make_shared<Platform::Win32::Win32PlatformManager>();
    auto processManager = std::make_shared<Platform::Win32::Win32ProcessManager>();
    auto fileManager = std::make_shared<Services::FileManager>();
    auto backupManager = std::make_shared<Services::BackupManager>(fileManager);
    auto downloader = std::make_shared<Services::GitHubDownloader>();
    auto verifier = std::make_shared<Services::CryptoVerifier>();
    auto progressReporter = std::make_shared<Services::ProgressReporter>();

    // Create orchestrator with all dependencies
    auto orchestrator = std::make_shared<UpdateOrchestrator>(
        platformManager,
        downloader,
        fileManager,
        backupManager,
        processManager,
        verifier,
        progressReporter
    );

    orchestrator->SetConfiguration(config);
    return orchestrator;
#else
    throw std::runtime_error("Win32 platform not supported on this system");
#endif
}

std::shared_ptr<UpdateOrchestrator> UpdaterFactory::CreateForLinux(const UpdateConfiguration& config)
{
#ifndef WIN32
    // Create platform-specific services
    auto platformManager = std::make_shared<Platform::Linux::LinuxPlatformManager>();
    auto processManager = std::make_shared<Platform::Linux::LinuxProcessManager>();
    auto fileManager = std::make_shared<Services::FileManager>();
    auto backupManager = std::make_shared<Services::BackupManager>(fileManager);
    auto downloader = std::make_shared<Services::GitHubDownloader>();
    auto verifier = std::make_shared<Services::CryptoVerifier>();
    auto progressReporter = std::make_shared<Services::ProgressReporter>();

    // Create orchestrator with all dependencies
    auto orchestrator = std::make_shared<UpdateOrchestrator>(
        platformManager,
        downloader,
        fileManager,
        backupManager,
        processManager,
        verifier,
        progressReporter
    );

    orchestrator->SetConfiguration(config);
    return orchestrator;
#else
    throw std::runtime_error("Linux platform not supported on this system");
#endif
}

UpdateConfiguration UpdaterFactory::CreateDefaultConfiguration()
{
    UpdateConfiguration config;
    
    // Update sources
    config.UpdateUrl = "https://github.com/razaqq/PotatoAlert/releases/latest/download/";
    config.VersionCheckUrl = "https://api.github.com/repos/razaqq/PotatoAlert/releases/latest";
    config.Channel = UpdateChannel::Stable;
    
    // Download settings
    config.MaxDownloadSpeed = 0; // Unlimited
    config.ResumeSupport = true;
    config.MaxRetries = 3;
    config.Timeout = std::chrono::seconds(30);
    
    // Security settings
    config.RequireSignatureVerification = true;
    config.AllowInsecureConnections = false;
    
    // Backup settings
    config.BackupLocation = std::filesystem::temp_directory_path() / "PotatoAlertBackup";
    config.MaxBackups = 3;
    config.CompressBackups = false;
    
    // Installation settings
    config.RestartAfterUpdate = true;
    config.SilentMode = false;
    
    return config;
}
