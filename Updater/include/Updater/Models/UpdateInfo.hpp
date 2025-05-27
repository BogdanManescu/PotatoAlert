// Copyright 2024 <github.com/razaqq>
#pragma once

#include "UpdateResult.hpp"

#include "Core/Version.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>


namespace PotatoAlert::Updater {

class UpdateInfo
{
public:
    PotatoAlert::Core::Version Version;
    std::string VersionString;
    std::string ReleaseNotes;
    std::string DownloadUrl;
    std::uint64_t FileSize = 0;
    std::string Checksum;
    std::string ChecksumAlgorithm = "SHA256";
    std::string SignatureUrl;
    UpdateChannel Channel = UpdateChannel::Stable;
    std::chrono::system_clock::time_point ReleaseDate;
    bool IsSecurityUpdate = false;
    bool IsCriticalUpdate = false;
    std::vector<std::string> Dependencies;
    std::filesystem::path LocalPath;  // Set when downloaded

    UpdateInfo() = default;

    UpdateInfo(PotatoAlert::Core::Version version, std::string downloadUrl)
        : Version(std::move(version))
        , VersionString(Version.ToString())
        , DownloadUrl(std::move(downloadUrl))
        , ReleaseDate(std::chrono::system_clock::now())
    {
    }

    [[nodiscard]] bool IsValid() const
    {
        return !VersionString.empty() && !DownloadUrl.empty();
    }

    [[nodiscard]] std::string GetFileSizeString() const
    {
        if (FileSize == 0)
            return "Unknown size";
        
        const double sizeMB = static_cast<double>(FileSize) / (1024.0 * 1024.0);
        if (sizeMB >= 1024.0)
        {
            const double sizeGB = sizeMB / 1024.0;
            return std::to_string(sizeGB) + " GB";
        }
        else
        {
            return std::to_string(sizeMB) + " MB";
        }
    }

    [[nodiscard]] std::string GetChannelString() const
    {
        switch (Channel)
        {
            case UpdateChannel::Stable: return "Stable";
            case UpdateChannel::Beta: return "Beta";
            case UpdateChannel::Alpha: return "Alpha";
            case UpdateChannel::Development: return "Development";
            default: return "Unknown";
        }
    }
};

}  // namespace PotatoAlert::Updater
