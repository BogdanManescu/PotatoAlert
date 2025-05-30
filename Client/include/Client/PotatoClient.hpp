// Copyright 2020 <github.com/razaqq>
#pragma once

#include "Client/Config.hpp"
#include "Client/DatabaseManager.hpp"
#include "Client/Game.hpp"
#include "Client/ReplayAnalyzer.hpp"
#include "Client/ServiceProvider.hpp"
#include "Client/StatsParser.hpp"
#include "Client/SysInfo.hpp"

#include "Core/DirectoryWatcher.hpp"

#include "ReplayParser/ReplayParser.hpp"

#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QNetworkReply>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>


using PotatoAlert::Client::Config;
using PotatoAlert::Client::Game::GameInfo;
using PotatoAlert::Client::StatsParser::MatchContext;

namespace PotatoAlert::Client {

struct GameDirectory
{
	std::filesystem::path Path;
	std::string Status;
	std::optional<GameInfo> Info;
};

enum class Status
{
	Ready,
	Loading,
	Error
};

struct ClientOptions
{
	std::string_view SubmitUrl;
	std::string_view LookupUrl;
	int32_t TransferTimeout;
};

class PotatoClient : public QObject
{
	Q_OBJECT

public:
	explicit PotatoClient(ClientOptions&& clientOptions, const ServiceProvider& serviceProvider)
		: m_options(std::move(clientOptions)), m_services(serviceProvider), m_replayAnalyzer(serviceProvider.Get<ReplayAnalyzer>()) {}
	~PotatoClient() override = default;

	void Init();
	void TriggerRun();
	void ForceRun();
	void UpdateGameInstalls();

private:
	void OnFileChanged(const std::filesystem::path& file);
	void SendRequest(std::string_view requestString, MatchContext&& matchContext);
	void HandleReply(QNetworkReply* reply, auto& successHandler);
	void LookupResult(const std::string& url, const std::string& authToken, const MatchContext& matchContext);

private:
	ClientOptions m_options;
	const ServiceProvider& m_services;
	Core::DirectoryWatcher m_watcher;
	std::string m_lastArenaInfoHash;
	std::vector<GameDirectory> m_gameInfos;
	ReplayAnalyzer& m_replayAnalyzer;
	std::optional<SysInfo> m_sysInfo;
	QNetworkAccessManager* m_networkAccessManager = new QNetworkAccessManager();

signals:
	void MatchReady(const StatsParser::MatchType& match);
	void MatchHistoryNewMatch(const Match& match);
	void ReplaySummaryChanged(uint32_t id, const ReplaySummary& summary);
	void StatusReady(Status status, std::string_view statusText);
	void GameInfosChanged(std::span<const GameDirectory> infos);
};

}  // namespace PotatoAlert::Client
