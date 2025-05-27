// Copyright 2024 <github.com/razaqq>

#include "Updater/Core/ErrorHandler.hpp"

using namespace PotatoAlert::Updater;

ErrorHandler::ErrorHandler()
{
    // Set default retry counts for recoverable errors
    SetMaxRetries(UpdateResult::NetworkError, 3);
    SetMaxRetries(UpdateResult::VerificationFailed, 1);
}

void ErrorHandler::SetErrorCallback(ErrorCallback callback)
{
    m_errorCallback = std::move(callback);
}

void ErrorHandler::ReportError(const UpdateError& error)
{
    m_errorHistory.push_back(error);
    
    if (m_errorCallback)
    {
        m_errorCallback(error);
    }
}

void ErrorHandler::ReportError(UpdateResult code, const std::string& message, const std::string& technicalDetails)
{
    UpdateError error(code, message, technicalDetails);
    ReportError(error);
}

void ErrorHandler::SetRecoveryAction(UpdateResult errorCode, RecoveryAction action)
{
    m_recoveryActions[errorCode] = std::move(action);
}

UpdateResult ErrorHandler::AttemptRecovery(const UpdateError& error)
{
    auto it = m_recoveryActions.find(error.Code);
    if (it != m_recoveryActions.end())
    {
        try
        {
            return it->second();
        }
        catch (...)
        {
            ReportError(UpdateResult::UnknownError, "Recovery action failed", "Exception thrown during recovery");
            return UpdateResult::UnknownError;
        }
    }
    
    return UpdateResult::UnknownError;
}

const std::vector<UpdateError>& ErrorHandler::GetErrorHistory() const
{
    return m_errorHistory;
}

void ErrorHandler::ClearErrorHistory()
{
    m_errorHistory.clear();
    
    // Reset retry counters
    for (auto& [code, count] : m_currentRetries)
    {
        count = 0;
    }
}

std::optional<UpdateError> ErrorHandler::GetLastError() const
{
    if (m_errorHistory.empty())
        return std::nullopt;
    
    return m_errorHistory.back();
}

bool ErrorHandler::IsRecoverable(const UpdateError& error) const
{
    return error.IsRecoverable() && ShouldRetry(error);
}

bool ErrorHandler::IsCritical(const UpdateError& error) const
{
    return error.IsCritical();
}

std::string ErrorHandler::GetUserFriendlyMessage(const UpdateError& error) const
{
    if (!error.Message.empty())
        return error.Message;
    
    return GetDefaultUserMessage(error.Code);
}

void ErrorHandler::SetMaxRetries(UpdateResult errorCode, int maxRetries)
{
    m_maxRetries[errorCode] = maxRetries;
}

bool ErrorHandler::ShouldRetry(const UpdateError& error) const
{
    auto maxIt = m_maxRetries.find(error.Code);
    if (maxIt == m_maxRetries.end())
        return false;
    
    auto currentIt = m_currentRetries.find(error.Code);
    int currentRetries = (currentIt != m_currentRetries.end()) ? currentIt->second : 0;
    
    return currentRetries < maxIt->second;
}

void ErrorHandler::IncrementRetryCount(UpdateResult errorCode)
{
    m_currentRetries[errorCode]++;
}

void ErrorHandler::ResetRetryCount(UpdateResult errorCode)
{
    m_currentRetries[errorCode] = 0;
}

std::string ErrorHandler::GetDefaultUserMessage(UpdateResult code) const
{
    switch (code)
    {
        case UpdateResult::Success:
            return "Update completed successfully";
        case UpdateResult::AlreadyUpToDate:
            return "Application is already up to date";
        case UpdateResult::NetworkError:
            return "Network error occurred during update";
        case UpdateResult::VerificationFailed:
            return "Update file verification failed";
        case UpdateResult::InsufficientPrivileges:
            return "Insufficient privileges to perform update";
        case UpdateResult::DiskSpaceError:
            return "Insufficient disk space for update";
        case UpdateResult::BackupFailed:
            return "Failed to create backup before update";
        case UpdateResult::InstallationFailed:
            return "Update installation failed";
        case UpdateResult::RollbackSucceeded:
            return "Update failed, successfully rolled back to previous version";
        case UpdateResult::RollbackFailed:
            return "Update failed and rollback also failed - manual intervention required";
        case UpdateResult::Cancelled:
            return "Update was cancelled by user";
        case UpdateResult::InvalidConfiguration:
            return "Invalid update configuration";
        case UpdateResult::UnknownError:
        default:
            return "An unknown error occurred during update";
    }
}
