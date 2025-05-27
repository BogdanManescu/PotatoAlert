// Copyright 2024 <github.com/razaqq>
#pragma once

#include <string>
#include <vector>

namespace PotatoAlert::Updater {

struct BackupConfiguration
{
    // File filtering
    std::vector<std::string> IncludePatterns;  // Empty means include all
    std::vector<std::string> ExcludePatterns;  // Always exclude these patterns
    
    // Backup options
    bool CalculateChecksums = true;  // Whether to calculate file checksums for verification
    bool CompressBackups = false;    // Whether to compress backup files
    bool VerifyIntegrity = true;     // Whether to verify backup integrity after creation
    
    // Default configuration
    static BackupConfiguration CreateDefault()
    {
        BackupConfiguration config;
        config.ExcludePatterns = {".trash", ".backup", ".log", ".tmp", "~"};
        return config;
    }
};

}  // namespace PotatoAlert::Updater
