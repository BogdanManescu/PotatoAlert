// Copyright 2022 <github.com/razaqq>

#include "Client/DatabaseManager.hpp"
#include "Client/ReplayAnalyzer.hpp"

#include "Core/Encoding.hpp"
#include "Core/File.hpp"
#include "Core/Log.hpp"
#include "Core/String.hpp"

#include "GameFileUnpack/GameFileUnpack.hpp"

#include "ReplayParser/ReplayParser.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <optional>
#include <ranges>
#include <string>


namespace fs = std::filesystem;

using namespace std::chrono_literals;
using namespace PotatoAlert::Core;
using PotatoAlert::Client::ReplayAnalyzer;
using PotatoAlert::GameFileUnpack::Unpacker;
using PotatoAlert::GameFileUnpack::UnpackResult;

bool ReplayAnalyzer::HasGameFiles(Version gameVersion) const
{
	return ReplayParser::HasGameScripts(gameVersion, m_gameFilePath);
	// &&MinimapRenderer::HasGameParams(gameVersion, m_gameFilePath);
}

UnpackResult<void> ReplayAnalyzer::UnpackGameFiles(const fs::path& dst, const fs::path& pkgPath, const fs::path& idxPath)
{
	Unpacker unpacker(pkgPath, idxPath);
	PA_TRYV(unpacker.Parse());
	PA_TRYV(unpacker.Extract("scripts/", dst));
	PA_TRYV(unpacker.Extract("content/GameParams.data", dst));
	return {};
}

void ReplayAnalyzer::OnFileChanged(const std::filesystem::path& file)
{
	if (file.extension() == fs::path(".wowsreplay") && File::Exists(file) &&
		file.filename() != fs::path("temp.wowsreplay"))
	{
		LOG_TRACE("Replay file {} changed", file);
		AnalyzeReplay(file, 30s);
	}
}

void ReplayAnalyzer::AnalyzeReplay(const fs::path& path, std::chrono::seconds readDelay)
{
	auto analyze = [this](const fs::path& file, std::chrono::seconds delay) -> void
	{
		// this is honestly not ideal, but i don't see another way of fixing it
		LOG_TRACE(STR("Analyzing replay file {} after {} delay..."), file, delay);
		std::this_thread::sleep_for(delay);

		PA_TRY_OR_ELSE(summary, ReplayParser::AnalyzeReplay(file, m_gameFilePath),
		{
			LOG_ERROR(STR("Failed to analyze replay file {}: {}"), file, StringWrap(error));
			return;
		});

		LOG_TRACE(STR("Replay analysis complete of file: {}"), file);

		const DatabaseManager& dbm = m_services.Get<DatabaseManager>();

		PA_TRY_OR_ELSE(match, dbm.GetMatch(summary.Hash),
		{
			LOG_ERROR("Failed to get match from match history: {}", error);
			return;
		});

		if (match)
		{
			PA_TRYV_OR_ELSE(dbm.SetMatchReplaySummary(summary.Hash, summary),
			{
				LOG_ERROR("Failed to set replay summary for match '{}': {}", summary.Hash, error);
				return;
			});
			emit ReplaySummaryReady(match.value().Id, summary);
			LOG_TRACE(STR("Set replay summary for replay: {}"), file);
			return;
		}
		LOG_TRACE("Cannot find replay to set summary with hash '{}'", summary.Hash);
	};

	// if this replay was never analyzed or analyzing finished, analyze it
	// this avoids running multiple analyzes if the game writes to the replay multiple times
	if (!m_futures.contains(path.native()))
	{
		m_futures.emplace(path.native(), m_threadPool.Enqueue(analyze, path, readDelay));
	}

	if (m_futures.at(path.native()).wait_for(0s) == std::future_status::ready)
	{
		m_futures.at(path.native()) = m_threadPool.Enqueue(analyze, path, readDelay);
	}
}

void ReplayAnalyzer::AnalyzeDirectory(const fs::path& directory)
{
	const DatabaseManager& dbm = m_services.Get<DatabaseManager>();

	PA_TRY_OR_ELSE(matches, dbm.GetNonAnalyzedMatches(),
	{
		LOG_ERROR("Failed to get non-analyzed matches from match history: {}", error);
		return;
	});

	std::ranges::sort(matches, [](const NonAnalyzedMatch& left, const NonAnalyzedMatch& right)
	{
		return left.ReplayName < right.ReplayName;
	});

	std::error_code ec;
	auto it = fs::recursive_directory_iterator(directory, ec);
	if (ec)
	{
		LOG_ERROR("Failed to iterate replay directory '{}': {}", directory, ec.message());
		return;
	}

	for (const fs::directory_entry& entry : it)
	{
		if (entry.is_regular_file() && entry.path().extension() == ".wowsreplay")
		{
			auto found = std::ranges::find_if(matches, [&](const NonAnalyzedMatch& match)
			{
				const std::string fileName = String::ToLower(entry.path().filename().string());
				return String::ToLower(match.ReplayName) == fileName;
			});

			if (found != matches.end())
			{
				AnalyzeReplay(entry.path());
			}
		}
	}
}
