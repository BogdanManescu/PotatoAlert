// Copyright 2024 <github.com/razaqq>
#pragma once

#include "Updater/Models/UpdateError.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>


namespace PotatoAlert::Updater {

class ErrorHandler
{
public:
    using ErrorCallback = std::function<void(const UpdateError&)>;
    using RecoveryAction = std::function<UpdateResult()>;

    ErrorHandler();

    // Error reporting
    void SetErrorCallback(ErrorCallback callback);
    void ReportError(const UpdateError& error);
    void ReportError(UpdateResult code, const std::string& message, const std::string& technicalDetails = "");

    // Error recovery
    void SetRecoveryAction(UpdateResult errorCode, RecoveryAction action);
    UpdateResult AttemptRecovery(const UpdateError& error);

    // Error history
    const std::vector<UpdateError>& GetErrorHistory() const;
    void ClearErrorHistory();
    std::optional<UpdateError> GetLastError() const;

    // Error analysis
    bool IsRecoverable(const UpdateError& error) const;
    bool IsCritical(const UpdateError& error) const;
    std::string GetUserFriendlyMessage(const UpdateError& error) const;

    // Retry logic
    void SetMaxRetries(UpdateResult errorCode, int maxRetries);
    bool ShouldRetry(const UpdateError& error) const;
    void IncrementRetryCount(UpdateResult errorCode);
    void ResetRetryCount(UpdateResult errorCode);

private:
    ErrorCallback m_errorCallback;
    std::vector<UpdateError> m_errorHistory;
    std::unordered_map<UpdateResult, RecoveryAction> m_recoveryActions;
    std::unordered_map<UpdateResult, int> m_maxRetries;
    std::unordered_map<UpdateResult, int> m_currentRetries;

    std::string GetDefaultUserMessage(UpdateResult code) const;
};

}  // namespace PotatoAlert::Updater
