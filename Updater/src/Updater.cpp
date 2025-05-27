// Copyright 2021 <github.com/razaqq>

#include "Updater/Updater.hpp"
#include "Updater/ModernUpdater.hpp"

#include "Core/Defer.hpp"
#include "Core/Directory.hpp"
#include "Core/Encoding.hpp"
#include "Core/Format.hpp"
#include "Core/Json.hpp"
#include "Core/Log.hpp"
#include "Core/Process.hpp"
#include "Core/String.hpp"
#include "Core/Version.hpp"
#include "Core/Zip.hpp"

#include <QApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QTimer>
#include <QUrl>
#include <QtNetwork>

#include <chrono>
#include <filesystem>
#include <string>
#include <utility>

#include "win32.h"
#include "tlhelp32.h"

/*
 * LEGACY IMPLEMENTATION - DEPRECATED
 * 
 * This file contains the legacy updater implementation for backward compatibility.
 * The actual update logic has been moved to the new ModernUpdater architecture.
 *  * Static methods delegate to ModernUpdater for consistency.
 */


using PotatoAlert::Updater::Updater;
using namespace PotatoAlert::Core;
namespace fs = std::filesystem;

// needs libssl-1_1-x64.dll and libcrypto-1_1-x64.dll from OpenSSL
static constexpr std::string_view g_updateURL = "https://github.com/razaqq/PotatoAlert/releases/latest/download/{}";
static constexpr std::string_view g_versionURL = "https://api.github.com/repos/razaqq/PotatoAlert/releases/latest";
// TODO: beta https://api.github.com/repos/razaqq/PotatoAlert/releases


namespace {

std::optional<DWORD> FindProcessByName(LPCTSTR Name)
{
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnap != INVALID_HANDLE_VALUE)
	{
		PROCESSENTRY32 pe;
		ZeroMemory(&pe, sizeof(PROCESSENTRY32));
		pe.dwSize = sizeof(PROCESSENTRY32);
		Process32First(hSnap, &pe);
		do
		{
			if (!lstrcmpi(pe.szExeFile, Name))
			{
				return pe.th32ProcessID;
			}
		} while (Process32Next(hSnap, &pe));
	}
	return {};
}

}

// makes a request to the github api and checks if there is a new version available
bool Updater::UpdateAvailable()
{
	// Delegate to ModernUpdater
	return ModernUpdater::UpdateAvailable();
}

// Legacy static methods now delegate to ModernUpdater
bool Updater::StartUpdater(std::string_view args)
{
	// Delegate to ModernUpdater
	return ModernUpdater::StartUpdater(args);
}

bool Updater::StartMain(std::string_view args)
{
	// Delegate to ModernUpdater
	return ModernUpdater::StartMain(args);
}

void Updater::RemoveTrash()
{
	// Delegate to ModernUpdater
	ModernUpdater::RemoveTrash();
}

// starts the update process
void Updater::Run()
{
	LOG_INFO("Starting update...");

	// Delegate to ModernUpdater for the actual update logic
	try
	{
		auto modernUpdater = std::make_shared<ModernUpdater>();
				// Set up progress callback to emit legacy signals
		modernUpdater->SetProgressCallback([this](const ProgressInfo& progress) {
			if (progress.BytesDownloaded.has_value() && progress.TotalBytes.has_value()) {
				QString progressStr = QString("%1/%2 MB").arg(
					QString::number(progress.BytesDownloaded.value() / 1e6, 'f', 1),
					QString::number(progress.TotalBytes.value() / 1e6, 'f', 1)
				);
				
				QString speedStr = "0 MB/s";
				if (progress.DownloadSpeedBytesPerSecond.has_value()) {
					speedStr = QString("%1 MB/s").arg(
						QString::number(progress.DownloadSpeedBytesPerSecond.value() / 1e6, 'f', 1)
					);
				}
				
				int percentage = static_cast<int>(progress.GetProgressPercent());
				emit DownloadProgress(percentage, progressStr, speedStr);
			}
		});
		// Set up error callback
		modernUpdater->SetErrorCallback([this](const UpdateError& error) {
			LOG_ERROR("Update failed: {}", error.Message);
			End(false);
		});

		// Run the update asynchronously
		auto future = modernUpdater->RunUpdateAsync();
		
		// Wait for completion in a Qt-compatible way
		QTimer timer;
		QEventLoop loop;
		
		timer.setSingleShot(true);
		timer.setInterval(100); // Check every 100ms
		
		connect(&timer, &QTimer::timeout, [&]() {
			if (future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
				try {
					UpdateResult result = future.get();
					bool success = (result == UpdateResult::Success);
					loop.quit();
					End(success);
				} catch (const std::exception& e) {
					LOG_ERROR("Update failed with exception: {}", e.what());
					loop.quit();
					End(false);
				}
			} else {
				timer.start(); // Continue checking
			}
		});
		
		timer.start();
		loop.exec(); // This will block until the update completes
	}
	catch (const std::exception& e)
	{
		LOG_ERROR("Failed to initialize modern updater: {}", e.what());
		End(false);
	}
}

// Legacy static methods now delegate to ModernUpdater
bool Updater::StartUpdater(std::string_view args)
{
	// Delegate to ModernUpdater
	return ModernUpdater::StartUpdater(args);
}

bool Updater::StartMain(std::string_view args)
{
	// Delegate to ModernUpdater
	return ModernUpdater::StartMain(args);
}

void Updater::RemoveTrash()
{
	// Delegate to ModernUpdater
	ModernUpdater::RemoveTrash();
}

// Legacy helper methods - kept for internal use by End() method
[[noreturn]] void Updater::End(bool success, bool revert)
{
	// Note: These backup methods and WaitForOtherProcessExit are kept as legacy
	// until the backup functionality is fully integrated into the new architecture
	if (revert)
		RevertBackup();
	RemoveBackup();

	if (success)
	{
		StartUpdater("--clear");
	}
	else
	{
		StartMain();
	}
	QApplication::exit(0); // exit this process
	ExitCurrentProcess(0);
}

// gets info about the elevation state {bool isElevated, bool canElevate}
Updater::ElevationInfo Updater::GetElevationInfo()
{
	HANDLE hToken;
	if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
	{
		PA_DEFER
		{
			CloseHandle(hToken);
		};
		TOKEN_ELEVATION_TYPE tet;
		DWORD returnLength = 0;
		if (GetTokenInformation(hToken, TokenElevationType, &tet, sizeof(tet), &returnLength))
		{
			assert(returnLength == sizeof(tet));
			return {tet == TokenElevationTypeFull, tet == TokenElevationTypeLimited};
		}
	}
	return {false, false};
}

bool Updater::CreateBackup()
{
	std::error_code ec;

	// check if old backup exists and remove it
	const bool exists = fs::exists(BackupDest(), ec);
	if (ec)
	{
		LOG_ERROR("Failed to check if backup exists: {}", ec.message());
		return false;
	}

	if (exists)
	{
		fs::remove_all(BackupDest(), ec);
		if (ec)
		{
			LOG_ERROR("Failed to remove old backup: {}", ec.message());
			return false;
		}
	}

	// copy new backup
	fs::copy(UpdateDest(), BackupDest(), fs::copy_options::recursive, ec);
	if (ec)
	{
		LOG_ERROR("Failed to copy backup to backup dest: {}", ec.message());
		return false;
	}

	// fix permissions
	fs::permissions(BackupDest(), fs::perms::all, ec);
	if (ec)
	{
		LOG_ERROR("Failed to change permissions of backup: {}", ec.message());
		return false;
	}
	return true;
}

bool Updater::RemoveBackup()
{
	std::error_code ec;
	fs::remove_all(BackupDest(), ec);

	if (ec)
	{
		LOG_ERROR("Failed to delete backup: {}", ec.message());
		return false;
	}
	return true;
}

bool Updater::RevertBackup()
{
	std::error_code ec;
	fs::copy(BackupDest(),UpdateDest(),
			fs::copy_options::recursive | fs::copy_options::overwrite_existing,
			ec);

	if (ec)
	{
		LOG_ERROR("Failed to revert backup: {}", ec.message());
		return false;
	}
	return true;
}

// renames all exe/dll files to .trash
bool Updater::RenameToTrash()
{
	std::error_code ec;

	auto it = fs::recursive_directory_iterator(UpdateDest(), ec);
	if (ec)
	{
		LOG_ERROR("Failed to get directory iterator: {}", ec.message());
		return false;
	}

	for (auto& p : it)
	{
		Path path = p.path();

		const bool regularFile = p.is_regular_file(ec);
		if (ec)
		{
			LOG_ERROR("Failed to check if {} is regular file: {}", path, ec);
			return false;
		}

		const std::string ext = path.extension().string();
		if (regularFile && (ext == ".dll" || ext == ".exe"))
		{
			path += Path(".trash");
			fs::rename(p, path, ec);
			if (ec)
			{
				LOG_ERROR("Failed to rename {} to .trash: {}", path, ec);
				return false;
			}
		}
	}
	return true;
}

void Updater::WaitForOtherProcessExit()
{
	std::optional<DWORD> pid = FindProcessByName(TEXT("PotatoUpdater.exe"));

	if (pid && GetCurrentProcessId() != pid.value())
	{
		if (const HANDLE hHandle = OpenProcess(SYNCHRONIZE, false, pid.value()))
		{
			LOG_INFO("Waiting for other updater process to terminate");
			WaitForSingleObject(hHandle, 10000);
			LOG_INFO("Other updater process terminated");
		}
	}
}
