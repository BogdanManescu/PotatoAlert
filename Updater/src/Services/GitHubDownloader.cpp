// Copyright 2024 <github.com/razaqq>

#include "Updater/Services/GitHubDownloader.hpp"
#include "Updater/Models/UpdateInfo.hpp"
#include "Updater/Models/UpdateConfiguration.hpp"
#include "Updater/Models/ProgressInfo.hpp"
#include "Updater/Interfaces/IProgressReporter.hpp"

#include "Core/Json.hpp"
#include "Core/Log.hpp"
#include "Core/Version.hpp"

#include <QApplication>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QDir>

#include <chrono>
#include <exception>


using namespace PotatoAlert::Updater;
using namespace PotatoAlert::Core;


GitHubDownloader::GitHubDownloader()
    : m_networkManager(std::make_unique<QNetworkAccessManager>())
    , m_speedUpdateTimer(std::make_unique<QTimer>())
    , m_lastBytesReceived(0)
    , m_downloadStartTime(std::chrono::steady_clock::now())
{
    m_speedUpdateTimer->setInterval(1000); // Update speed every second
    connect(m_speedUpdateTimer.get(), &QTimer::timeout, this, &GitHubDownloader::OnSpeedUpdateTimer);
}

GitHubDownloader::~GitHubDownloader()
{
    Cancel();
}

void GitHubDownloader::SetProgressReporter(std::shared_ptr<IProgressReporter> reporter)
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    m_progressReporter = reporter;
}

std::future<std::pair<UpdateResult, std::optional<UpdateInfo>>> GitHubDownloader::CheckForUpdates(
    const UpdateConfiguration& config)
{
    auto promise = std::make_shared<std::promise<std::pair<UpdateResult, std::optional<UpdateInfo>>>>();
    auto future = promise->get_future();

    try 
    {
        QNetworkRequest request;
        request.setUrl(QUrl(QString::fromStdString(config.VersionCheckUrl)));
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        request.setRawHeader("User-Agent", "PotatoAlert-Updater/1.0");

        auto reply = m_networkManager->get(request);
        
        connect(reply, &QNetworkReply::finished, [this, reply, promise, config]() 
        {
            reply->deleteLater();
            
            if (reply->error() != QNetworkReply::NoError) 
            {
                LOG_ERROR("Network request failed: {}", reply->errorString().toStdString());
                promise->set_value({UpdateResult::NetworkError, std::nullopt});
                return;
            }

            try 
            {
                auto response = reply->readAll().toStdString();
                PA_TRY_OR_ELSE(json, ParseJson(response), 
                {
                    LOG_ERROR("Failed to parse GitHub API response as JSON");
                    promise->set_value({UpdateResult::ParseError, std::nullopt});
                    return;
                });

                if (!json.HasMember("tag_name") || !json.HasMember("assets"))
                {
                    LOG_ERROR("GitHub response missing required fields");
                    promise->set_value({UpdateResult::ParseError, std::nullopt});
                    return;
                }

                std::string versionString = FromJson<std::string>(json["tag_name"]);
                Version remoteVersion(versionString);
                Version currentVersion(QApplication::applicationVersion().toStdString());

                if (remoteVersion <= currentVersion)
                {
                    promise->set_value({UpdateResult::NoUpdateAvailable, std::nullopt});
                    return;
                }

                // Find the appropriate asset for current platform
                std::string targetAsset;
                if (config.Platform == Platform::Windows)
                {
                    targetAsset = "PotatoAlert.zip";
                }
                else if (config.Platform == Platform::Linux)
                {
                    targetAsset = "PotatoAlert_linux.zip";
                }

                std::string downloadUrl;
                size_t fileSize = 0;
                
                const auto& assets = json["assets"];
                for (auto it = assets.Begin(); it != assets.End(); ++it)
                {
                    if ((*it).HasMember("name") && (*it).HasMember("browser_download_url"))
                    {
                        std::string assetName = FromJson<std::string>((*it)["name"]);
                        if (assetName == targetAsset)
                        {
                            downloadUrl = FromJson<std::string>((*it)["browser_download_url"]);
                            if ((*it).HasMember("size"))
                            {
                                fileSize = static_cast<size_t>(FromJson<int64_t>((*it)["size"]));
                            }
                            break;
                        }
                    }
                }

                if (downloadUrl.empty())
                {
                    LOG_ERROR("No suitable asset found for platform");
                    promise->set_value({UpdateResult::AssetNotFound, std::nullopt});
                    return;
                }

                UpdateInfo updateInfo;
                updateInfo.Version = versionString;
                updateInfo.DownloadUrl = downloadUrl;
                updateInfo.FileSize = fileSize;
                updateInfo.ReleaseNotes = json.HasMember("body") ? FromJson<std::string>(json["body"]) : "";
                updateInfo.PublishedAt = json.HasMember("published_at") ? FromJson<std::string>(json["published_at"]) : "";

                promise->set_value({UpdateResult::Success, updateInfo});
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("Exception while parsing update info: {}", e.what());
                promise->set_value({UpdateResult::ParseError, std::nullopt});
            }
        });

        connect(reply, &QNetworkReply::errorOccurred, [promise](QNetworkReply::NetworkError error)
        {
            LOG_ERROR("Network error occurred: {}", static_cast<int>(error));
            promise->set_value({UpdateResult::NetworkError, std::nullopt});
        });
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Exception in CheckForUpdates: {}", e.what());
        promise->set_exception(std::current_exception());
    }

    return future;
}

std::future<UpdateResult> GitHubDownloader::DownloadUpdate(
    const UpdateInfo& updateInfo,
    const std::filesystem::path& destination,
    const UpdateConfiguration& config)
{
    auto promise = std::make_shared<std::promise<UpdateResult>>();
    auto future = promise->get_future();

    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        if (m_downloadState)
        {
            promise->set_value(UpdateResult::OperationInProgress);
            return future;
        }

        m_downloadState = std::make_unique<DownloadState>();
        m_downloadState->Promise = std::move(*promise);
        m_downloadState->Destination = destination;
        m_downloadState->UpdateInfo = updateInfo;
    }

    try
    {
        // Ensure destination directory exists
        std::error_code ec;
        std::filesystem::create_directories(destination.parent_path(), ec);
        if (ec)
        {
            LOG_ERROR("Failed to create destination directory: {}", ec.message());
            SetDownloadResult(UpdateResult::FileSystemError);
            return future;
        }

        QNetworkRequest request;
        request.setUrl(QUrl(QString::fromStdString(updateInfo.DownloadUrl)));
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        request.setRawHeader("User-Agent", "PotatoAlert-Updater/1.0");

        // Set timeout
        request.setTransferTimeout(static_cast<int>(config.NetworkTimeout.count() * 1000));

        auto reply = m_networkManager->get(request);
        
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            m_downloadState->Reply.reset(reply);
        }

        connect(reply, &QNetworkReply::downloadProgress, this, &GitHubDownloader::OnDownloadProgress);
        connect(reply, &QNetworkReply::finished, this, &GitHubDownloader::OnDownloadFinished);
        
        connect(reply, &QNetworkReply::errorOccurred, [this](QNetworkReply::NetworkError error)
        {
            LOG_ERROR("Download error occurred: {}", static_cast<int>(error));
            SetDownloadResult(UpdateResult::NetworkError);
        });

        // Start speed tracking
        m_downloadStartTime = std::chrono::steady_clock::now();
        m_lastBytesReceived = 0;
        m_speedUpdateTimer->start();

        if (m_progressReporter)
        {
            ProgressInfo progress;
            progress.Stage = UpdateStage::Downloading;
            progress.Message = "Starting download...";
            progress.Progress = 0.0;
            m_progressReporter->ReportProgress(progress);
        }
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Exception in DownloadUpdate: {}", e.what());
        SetDownloadResult(UpdateResult::InternalError);
    }

    return future;
}

void GitHubDownloader::Cancel()
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    if (m_downloadState && m_downloadState->Reply)
    {
        m_downloadState->Reply->abort();
        m_speedUpdateTimer->stop();
        
        if (m_progressReporter)
        {
            ProgressInfo progress;
            progress.Stage = UpdateStage::Downloading;
            progress.Message = "Download cancelled";
            progress.Progress = 0.0;
            m_progressReporter->ReportProgress(progress);
        }
    }
}

bool GitHubDownloader::IsDownloading() const
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_downloadState && m_downloadState->Reply && m_downloadState->Reply->isRunning();
}

void GitHubDownloader::OnDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    if (bytesTotal == 0)
        return;

    std::lock_guard<std::mutex> lock(m_stateMutex);
    if (!m_downloadState)
        return;

    m_lastBytesReceived = bytesReceived;

    if (m_progressReporter)
    {
        ProgressInfo progress;
        progress.Stage = UpdateStage::Downloading;
        progress.Progress = static_cast<double>(bytesReceived) / static_cast<double>(bytesTotal);
        progress.BytesDownloaded = static_cast<size_t>(bytesReceived);
        progress.TotalBytes = static_cast<size_t>(bytesTotal);
        
        // Calculate current speed
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_downloadStartTime);
        if (elapsed.count() > 0)
        {
            progress.DownloadSpeed = static_cast<double>(bytesReceived) / elapsed.count();
        }

        progress.Message = fmt::format("Downloading... {:.1f}/{:.1f} MB",
            bytesReceived / 1e6, bytesTotal / 1e6);

        m_progressReporter->ReportProgress(progress);
    }
}

void GitHubDownloader::OnDownloadFinished()
{
    std::unique_lock<std::mutex> lock(m_stateMutex);
    if (!m_downloadState || !m_downloadState->Reply)
        return;

    m_speedUpdateTimer->stop();

    auto reply = m_downloadState->Reply.get();
    auto destination = m_downloadState->Destination;
    
    lock.unlock();

    if (reply->error() != QNetworkReply::NoError)
    {
        LOG_ERROR("Download failed: {}", reply->errorString().toStdString());
        SetDownloadResult(UpdateResult::NetworkError);
        return;
    }

    // Save downloaded data to file
    QFile file(QString::fromStdString(destination.string()));
    if (!file.open(QIODevice::WriteOnly))
    {
        LOG_ERROR("Failed to open destination file for writing: {}", destination.string());
        SetDownloadResult(UpdateResult::FileSystemError);
        return;
    }

    auto data = reply->readAll();
    if (file.write(data) != data.size())
    {
        LOG_ERROR("Failed to write complete data to file");
        SetDownloadResult(UpdateResult::FileSystemError);
        return;
    }

    file.close();

    if (m_progressReporter)
    {
        ProgressInfo progress;
        progress.Stage = UpdateStage::Downloading;
        progress.Message = "Download completed";
        progress.Progress = 1.0;
        m_progressReporter->ReportProgress(progress);
    }

    LOG_INFO("Download completed successfully: {}", destination.string());
    SetDownloadResult(UpdateResult::Success);
}

void GitHubDownloader::OnSpeedUpdateTimer()
{
    // Speed calculation is handled in OnDownloadProgress
    // This timer just ensures regular updates even if progress doesn't change
}

void GitHubDownloader::SetDownloadResult(UpdateResult result)
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    if (m_downloadState)
    {
        m_downloadState->Promise.set_value(result);
        m_downloadState.reset();
    }
}
