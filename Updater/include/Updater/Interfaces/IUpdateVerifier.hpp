// Copyright 2024 <github.com/razaqq>
#pragma once

#include "Updater/Models/UpdateResult.hpp"
#include "Updater/Models/UpdateInfo.hpp"

#include <filesystem>
#include <string>
#include <vector>


namespace PotatoAlert::Updater {

struct VerificationResult
{
    bool IsValid = false;
    std::string ErrorMessage;
    std::string CalculatedChecksum;
    std::string ExpectedChecksum;
    bool SignatureValid = false;
    std::string SignatureError;
};

class IUpdateVerifier
{
public:
    virtual ~IUpdateVerifier() = default;

    // Checksum verification
    virtual VerificationResult VerifyChecksum(
        const std::filesystem::path& filePath,
        const std::string& expectedChecksum,
        const std::string& algorithm = "SHA256") = 0;
    
    // Digital signature verification
    virtual VerificationResult VerifySignature(
        const std::filesystem::path& filePath,
        const std::filesystem::path& signatureFile) = 0;
    
    virtual VerificationResult VerifySignature(
        const std::filesystem::path& filePath,
        const std::string& signatureUrl) = 0;
    
    // Certificate validation
    virtual bool IsCertificateTrusted(const std::string& certificateFingerprint) = 0;
    virtual std::vector<std::string> GetFileCertificateFingerprints(const std::filesystem::path& filePath) = 0;
    
    // Comprehensive verification
    virtual VerificationResult VerifyUpdate(
        const std::filesystem::path& updateFile,
        const UpdateInfo& updateInfo,
        const std::vector<std::string>& trustedCertificates = {}) = 0;
    
    // File integrity
    virtual bool IsFileCorrupted(const std::filesystem::path& filePath) = 0;
    virtual std::string CalculateFileHash(const std::filesystem::path& filePath, const std::string& algorithm = "SHA256") = 0;
    
    // Platform-specific verification
    virtual bool IsExecutableValid(const std::filesystem::path& executablePath) = 0;
    virtual bool HasValidCodeSignature(const std::filesystem::path& executablePath) = 0;
};

}  // namespace PotatoAlert::Updater
