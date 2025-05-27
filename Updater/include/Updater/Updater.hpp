// Copyright 2021 <github.com/razaqq>
#pragma once

#include <QtNetwork/QNetworkAccessManager>
#include <QWidget>

#include <filesystem>
#include <string>
#include <utility>


namespace PotatoAlert::Updater {

enum class Edition
{
	Windows,
	Linux
};

#if defined(WIN32)
	static constexpr Edition CurrentEdition = Edition::Windows;
#else
	static constexpr Edition CurrentEdition = Edition::Linux;
#endif

static constexpr std::string_view UpdateArchiveFile(Edition edition)
{
	switch (edition)
	{
		case Edition::Windows:
			return "PotatoAlert.zip";
		case Edition::Linux:
			return "PotatoAlert_linux.zip";
	}
	return "";
}

/**
 * @brief Legacy Updater class - DEPRECATED
 * 
 * This class is deprecated and maintained only for backward compatibility.
 * New code should use ModernUpdater or UpdateOrchestrator directly.
 * 
 * @deprecated Use ModernUpdater instead
 */
class [[deprecated("Use ModernUpdater instead")]] Updater : public QWidget
{
	Q_OBJECT

public:
	// Legacy static API - delegated to ModernUpdater
	static bool UpdateAvailable();
	static bool StartUpdater(std::string_view args = "");
	static bool StartMain(std::string_view args = "");
	static void RemoveTrash();
	// Legacy instance methods - for compatibility with existing GUI code
	void Run();

private:
	using Path = std::filesystem::path;

	// Legacy methods - kept for internal compatibility
	[[noreturn]] static void End(bool success = true, bool revert = false);

	// Legacy backup methods
	static bool CreateBackup();
	static bool RemoveBackup();
	static bool RevertBackup();
	static bool RenameToTrash();

	// Legacy path methods
	static Path UpdateDest() { return std::filesystem::absolute(std::filesystem::current_path()); }
	static Path BackupDest() { return Path(std::filesystem::temp_directory_path() / "PotatoAlertBackup"); }
	static Path UpdateArchive() { return Path(std::filesystem::temp_directory_path() / UpdateArchiveFile(CurrentEdition)); }

	struct ElevationInfo
	{
		bool IsElevated;
		bool CanElevate;
	};
	static ElevationInfo GetElevationInfo();
	static void WaitForOtherProcessExit();

	constexpr static std::string_view m_updaterBinary = "PotatoUpdater.exe";
	constexpr static std::string_view m_mainBinary = "PotatoAlert.exe";

signals:
	void DownloadProgress(int percent, const QString& progress, const QString& speed);
};

}  // namespace PotatoAlert::Updater
