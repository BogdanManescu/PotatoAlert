// Copyright 2024 <github.com/razaqq>
#pragma once

#include <chrono>
#include <exception>
#include <optional>
#include <string>


namespace PotatoAlert::Updater {

enum class UpdateResult
{
    Success,
    AlreadyUpToDate,
    NetworkError,
    VerificationFailed,
    InsufficientPrivileges,
    DiskSpaceError,
    BackupFailed,
    InstallationFailed,
    RollbackSucceeded,
    RollbackFailed,
    Cancelled,
    InvalidConfiguration,
    UnknownError
};

enum class UpdateChannel
{
    Stable,
    Beta,
    Alpha,
    Development
};

enum class UpdateState
{
    Idle,
    CheckingForUpdates,
    UpdateAvailable,
    Downloading,
    Verifying,
    BackingUp,
    Installing,
    Finalizing,
    RollingBack,
    Complete,
    Failed
};

}  // namespace PotatoAlert::Updater
