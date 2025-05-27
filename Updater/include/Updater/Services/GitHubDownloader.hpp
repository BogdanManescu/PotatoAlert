// Copyright 2024 <github.com/razaqq>
#pragma once

#include "Updater/Interfaces/IDownloader.hpp"
#include "Updater/Models/DownloadProgress.hpp"
#include "Updater/Models/UpdateInfo.hpp"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>


namespace PotatoAlert::Updater {

class GitHubDownloader : public QObject, public IDownloader
{
    Q_OBJECT

public:
    GitHubDownloader();
    ~GitHubDownloader() override;

    // IDownloader implementation
    void SetProgressReporter(std::shared_ptr<IProgressReporter> reporter) override;
    
    std::future<std::pair<UpdateResult, std::optional<UpdateInfo>>> CheckForUpdates(
        const UpdateConfiguration& config) override;
    
    std::future<UpdateResult> DownloadUpdate(
        const UpdateInfo& updateInfo, 
        const std::filesystem::path& destination,
        const UpdateConfiguration& config) override;
    
    void Cancel() override;
    bool IsDownloading() const override;

private slots:
    void OnDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void OnDownloadFinished();
    void OnSpeedUpdateTimer();

private:
    struct DownloadState
    {
        std::promise<UpdateResult> Promise;
        std::unique_ptr<QNetworkReply> Reply;
        std::filesystem::path Destination;
        DownloadProgress Progress;
        bool Cancelled = false;
    };

    // Helper methods
    std::optional<UpdateInfo> ParseGitHubApiResponse(const QByteArray& data, const UpdateConfiguration& config);
    UpdateResult ValidateDownload(const std::filesystem::path& filePath, const UpdateInfo& updateInfo);
    void SetupNetworkRequest(QNetworkRequest& request, const UpdateConfiguration& config);
    std::string FormatBytes(std::uint64_t bytes);    // Member variables
    std::unique_ptr<QNetworkAccessManager> m_networkManager;
    std::shared_ptr<IProgressReporter> m_progressReporter;
    
    std::unique_ptr<DownloadState> m_currentDownload;
    std::atomic<bool> m_downloading{false};
    
    std::unique_ptr<QTimer> m_speedUpdateTimer;
    qint64 m_lastBytesReceived{0};
    std::chrono::steady_clock::time_point m_downloadStartTime;
    DownloadState* m_downloadState{nullptr};
    
    mutable std::mutex m_stateMutex;
};

}  // namespace PotatoAlert::Updater
