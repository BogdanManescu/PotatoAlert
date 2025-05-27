// Copyright 2024 <github.com/razaqq>
#pragma once

#include "Updater/Interfaces/IUpdateVerifier.hpp"

#include <filesystem>
#include <memory>
#include <mutex>
#include <vector>
#include <string>


namespace PotatoAlert::Updater {

class CryptoVerifier : public IUpdateVerifier
{
public:
    CryptoVerifier();
    ~CryptoVerifier() override = default;

    // IUpdateVerifier implementation
    void SetProgressReporter(std::shared_ptr<IProgressReporter> reporter) override;

    std::future<UpdateResult> VerifyFileChecksum(
        const std::filesystem::path& filePath,
        const std::string& expectedChecksum,
        ChecksumAlgorithm algorithm) override;

    std::future<UpdateResult> VerifyDigitalSignature(
        const std::filesystem::path& filePath,
        const SignatureConfiguration& config) override;

    std::future<UpdateResult> VerifyUpdatePackage(
        const std::filesystem::path& packagePath,
        const VerificationConfiguration& config) override;

    UpdateResult GetLastError() const override;

private:
    UpdateResult VerifyFileChecksumInternal(
        const std::filesystem::path& filePath,
        const std::string& expectedChecksum,
        ChecksumAlgorithm algorithm);

    UpdateResult VerifyDigitalSignatureInternal(
        const std::filesystem::path& filePath,
        const SignatureConfiguration& config);

    UpdateResult VerifyUpdatePackageInternal(
        const std::filesystem::path& packagePath,
        const VerificationConfiguration& config);

    std::string CalculateChecksum(
        const std::filesystem::path& filePath,
        ChecksumAlgorithm algorithm);

    std::string CalculateSha256(const std::filesystem::path& filePath);
    std::string CalculateSha1(const std::filesystem::path& filePath);
    std::string CalculateMd5(const std::filesystem::path& filePath);

#ifdef _WIN32
    UpdateResult VerifyWindowsSignature(
        const std::filesystem::path& filePath,
        const SignatureConfiguration& config);
#else
    UpdateResult VerifyLinuxSignature(
        const std::filesystem::path& filePath,
        const SignatureConfiguration& config);
#endif

    void ReportProgress(const std::string& operation, double progress);

    std::shared_ptr<IProgressReporter> m_progressReporter;
    mutable std::mutex m_mutex;
    mutable UpdateResult m_lastError{UpdateResult::Success};
};

}  // namespace PotatoAlert::Updater
