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

class UpdateError
{
public:
    UpdateResult Code;
    std::string Message;
    std::string TechnicalDetails;
    std::optional<std::exception_ptr> Exception;
    std::chrono::system_clock::time_point Timestamp;

    UpdateError(UpdateResult code, std::string message, std::string technicalDetails = "")
        : Code(code)
        , Message(std::move(message))
        , TechnicalDetails(std::move(technicalDetails))
        , Timestamp(std::chrono::system_clock::now())
    {
    }

    UpdateError(UpdateResult code, std::string message, std::exception_ptr exception)
        : Code(code)
        , Message(std::move(message))
        , Exception(exception)
        , Timestamp(std::chrono::system_clock::now())
    {
    }
};

}  // namespace PotatoAlert::Updater
