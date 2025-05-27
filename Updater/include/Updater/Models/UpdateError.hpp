// Copyright 2024 <github.com/razaqq>
#pragma once

#include "UpdateResult.hpp"

#include <string>
#include <chrono>
#include <exception>
#include <optional>


namespace PotatoAlert::Updater {

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

    [[nodiscard]] std::string GetDetailedMessage() const
    {
        std::string result = Message;
        if (!TechnicalDetails.empty())
        {
            result += " Technical details: " + TechnicalDetails;
        }
        return result;
    }

    [[nodiscard]] bool IsCritical() const
    {
        return Code == UpdateResult::RollbackFailed 
            || Code == UpdateResult::InsufficientPrivileges
            || Code == UpdateResult::DiskSpaceError;
    }

    [[nodiscard]] bool IsRecoverable() const
    {
        return Code == UpdateResult::NetworkError
            || Code == UpdateResult::VerificationFailed
            || Code == UpdateResult::Cancelled;
    }
};

}  // namespace PotatoAlert::Updater
