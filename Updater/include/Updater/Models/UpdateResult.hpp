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
    DiskSpaceError,    BackupFailed,
    InstallationFailed,
    RollbackSucceeded,
    RollbackFailed,
    Cancelled,
    InvalidConfiguration,
    UnknownError,    ParseError,
    NoUpdateAvailable,
    AssetNotFound,
    OperationInProgress,
    InternalError,
    FileSystemError
};

enum class FilePermissions
{
    ReadOnly = 0x444,
    ReadWrite = 0x644,
    Execute = 0x755,
    All = 0x777
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
