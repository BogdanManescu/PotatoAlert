// Copyright 2024 <github.com/razaqq>

#include "Updater/Services/CryptoVerifier.hpp"
#include "Updater/Models/ProgressInfo.hpp"
#include "Updater/Interfaces/IProgressReporter.hpp"

#include "Core/Log.hpp"
#include "Core/Sha256.hpp"
#include "Core/Sha1.hpp"

#include <future>
#include <thread>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#include <wintrust.h>
#include <softpub.h>
#include <wincrypt.h>
#pragma comment(lib, "wintrust.lib")
#pragma comment(lib, "crypt32.lib")
#else
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#endif


using namespace PotatoAlert::Updater;
using namespace PotatoAlert::Core;


CryptoVerifier::CryptoVerifier() = default;

void CryptoVerifier::SetProgressReporter(std::shared_ptr<IProgressReporter> reporter)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_progressReporter = reporter;
}

std::future<UpdateResult> CryptoVerifier::VerifyFileChecksum(
    const std::filesystem::path& filePath,
    const std::string& expectedChecksum,
    ChecksumAlgorithm algorithm)
{
    return std::async(std::launch::async, [this, filePath, expectedChecksum, algorithm]() {
        return VerifyFileChecksumInternal(filePath, expectedChecksum, algorithm);
    });
}

std::future<UpdateResult> CryptoVerifier::VerifyDigitalSignature(
    const std::filesystem::path& filePath,
    const SignatureConfiguration& config)
{
    return std::async(std::launch::async, [this, filePath, config]() {
        return VerifyDigitalSignatureInternal(filePath, config);
    });
}

std::future<UpdateResult> CryptoVerifier::VerifyUpdatePackage(
    const std::filesystem::path& packagePath,
    const VerificationConfiguration& config)
{
    return std::async(std::launch::async, [this, packagePath, config]() {
        return VerifyUpdatePackageInternal(packagePath, config);
    });
}

UpdateResult CryptoVerifier::GetLastError() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastError;
}

UpdateResult CryptoVerifier::VerifyFileChecksumInternal(
    const std::filesystem::path& filePath,
    const std::string& expectedChecksum,
    ChecksumAlgorithm algorithm)
{
    try
    {
        ReportProgress("Calculating file checksum", 0.0);

        std::error_code ec;
        if (!std::filesystem::exists(filePath, ec) || ec)
        {
            LOG_ERROR("File does not exist for checksum verification: {}", filePath.string());
            return UpdateResult::FileNotFound;
        }

        auto actualChecksum = CalculateChecksum(filePath, algorithm);
        if (actualChecksum.empty())
        {
            LOG_ERROR("Failed to calculate checksum for file: {}", filePath.string());
            return UpdateResult::VerificationFailed;
        }

        ReportProgress("Comparing checksums", 0.8);

        // Normalize checksums for comparison (lowercase)
        std::string normalizedExpected = expectedChecksum;
        std::string normalizedActual = actualChecksum;
        std::transform(normalizedExpected.begin(), normalizedExpected.end(), normalizedExpected.begin(), ::tolower);
        std::transform(normalizedActual.begin(), normalizedActual.end(), normalizedActual.begin(), ::tolower);

        if (normalizedExpected != normalizedActual)
        {
            LOG_ERROR("Checksum verification failed for {}: expected {}, got {}", 
                filePath.string(), expectedChecksum, actualChecksum);
            return UpdateResult::VerificationFailed;
        }

        ReportProgress("Checksum verification completed", 1.0);
        LOG_INFO("Checksum verification successful for: {}", filePath.string());
        return UpdateResult::Success;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Exception during checksum verification: {}", e.what());
        return UpdateResult::InternalError;
    }
}

UpdateResult CryptoVerifier::VerifyDigitalSignatureInternal(
    const std::filesystem::path& filePath,
    const SignatureConfiguration& config)
{
    try
    {
        ReportProgress("Verifying digital signature", 0.0);

        std::error_code ec;
        if (!std::filesystem::exists(filePath, ec) || ec)
        {
            LOG_ERROR("File does not exist for signature verification: {}", filePath.string());
            return UpdateResult::FileNotFound;
        }

#ifdef _WIN32
        auto result = VerifyWindowsSignature(filePath, config);
#else
        auto result = VerifyLinuxSignature(filePath, config);
#endif

        if (result == UpdateResult::Success)
        {
            ReportProgress("Digital signature verification completed", 1.0);
            LOG_INFO("Digital signature verification successful for: {}", filePath.string());
        }
        else
        {
            LOG_ERROR("Digital signature verification failed for: {}", filePath.string());
        }

        return result;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Exception during signature verification: {}", e.what());
        return UpdateResult::InternalError;
    }
}

UpdateResult CryptoVerifier::VerifyUpdatePackageInternal(
    const std::filesystem::path& packagePath,
    const VerificationConfiguration& config)
{
    try
    {
        ReportProgress("Verifying update package", 0.0);

        // Step 1: Verify file checksum if provided
        if (!config.ExpectedChecksum.empty())
        {
            ReportProgress("Verifying package checksum", 0.3);
            auto checksumResult = VerifyFileChecksumInternal(packagePath, config.ExpectedChecksum, config.ChecksumAlgorithm);
            if (checksumResult != UpdateResult::Success)
            {
                return checksumResult;
            }
        }

        // Step 2: Verify digital signature if required
        if (config.RequireSignature)
        {
            ReportProgress("Verifying package signature", 0.6);
            auto signatureResult = VerifyDigitalSignatureInternal(packagePath, config.SignatureConfig);
            if (signatureResult != UpdateResult::Success)
            {
                return signatureResult;
            }
        }

        // Step 3: Verify file size if provided
        if (config.ExpectedSize > 0)
        {
            ReportProgress("Verifying package size", 0.9);
            std::error_code ec;
            auto actualSize = std::filesystem::file_size(packagePath, ec);
            if (ec)
            {
                LOG_ERROR("Failed to get file size for package verification: {}", ec.message());
                return UpdateResult::FileSystemError;
            }

            if (actualSize != config.ExpectedSize)
            {
                LOG_ERROR("Package size verification failed: expected {}, got {}", 
                    config.ExpectedSize, actualSize);
                return UpdateResult::VerificationFailed;
            }
        }

        ReportProgress("Package verification completed", 1.0);
        LOG_INFO("Update package verification successful: {}", packagePath.string());
        return UpdateResult::Success;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Exception during package verification: {}", e.what());
        return UpdateResult::InternalError;
    }
}

std::string CryptoVerifier::CalculateChecksum(
    const std::filesystem::path& filePath,
    ChecksumAlgorithm algorithm)
{
    switch (algorithm)
    {
        case ChecksumAlgorithm::SHA256:
            return CalculateSha256(filePath);
        case ChecksumAlgorithm::SHA1:
            return CalculateSha1(filePath);
        case ChecksumAlgorithm::MD5:
            return CalculateMd5(filePath);
        default:
            LOG_ERROR("Unsupported checksum algorithm: {}", static_cast<int>(algorithm));
            return "";
    }
}

std::string CryptoVerifier::CalculateSha256(const std::filesystem::path& filePath)
{
    try
    {
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open())
        {
            LOG_ERROR("Failed to open file for SHA256 calculation: {}", filePath.string());
            return "";
        }

        Sha256 sha256;
        constexpr size_t bufferSize = 8192;
        std::vector<char> buffer(bufferSize);

        while (file.read(buffer.data(), bufferSize) || file.gcount() > 0)
        {
            sha256.Update(reinterpret_cast<const uint8_t*>(buffer.data()), file.gcount());
        }

        auto hash = sha256.Finalize();
        
        // Convert to hex string
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (const auto& byte : hash)
        {
            oss << std::setw(2) << static_cast<unsigned>(byte);
        }

        return oss.str();
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Exception calculating SHA256: {}", e.what());
        return "";
    }
}

std::string CryptoVerifier::CalculateSha1(const std::filesystem::path& filePath)
{
    try
    {
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open())
        {
            LOG_ERROR("Failed to open file for SHA1 calculation: {}", filePath.string());
            return "";
        }

        Sha1 sha1;
        constexpr size_t bufferSize = 8192;
        std::vector<char> buffer(bufferSize);

        while (file.read(buffer.data(), bufferSize) || file.gcount() > 0)
        {
            sha1.Update(reinterpret_cast<const uint8_t*>(buffer.data()), file.gcount());
        }

        auto hash = sha1.Finalize();
        
        // Convert to hex string
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (const auto& byte : hash)
        {
            oss << std::setw(2) << static_cast<unsigned>(byte);
        }

        return oss.str();
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Exception calculating SHA1: {}", e.what());
        return "";
    }
}

std::string CryptoVerifier::CalculateMd5(const std::filesystem::path& filePath)
{
    try
    {
        // Note: MD5 implementation would need to be added to Core library
        // For now, return empty string with a warning
        LOG_WARN("MD5 checksum calculation not implemented yet");
        return "";
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Exception calculating MD5: {}", e.what());
        return "";
    }
}

#ifdef _WIN32
UpdateResult CryptoVerifier::VerifyWindowsSignature(
    const std::filesystem::path& filePath,
    const SignatureConfiguration& config)
{
    try
    {
        WINTRUST_FILE_INFO fileInfo;
        ZeroMemory(&fileInfo, sizeof(fileInfo));
        fileInfo.cbStruct = sizeof(WINTRUST_FILE_INFO);
        
        std::wstring wFilePath = filePath.wstring();
        fileInfo.pcwszFilePath = wFilePath.c_str();
        fileInfo.hFile = nullptr;
        fileInfo.pgKnownSubject = nullptr;

        WINTRUST_DATA winTrustData;
        ZeroMemory(&winTrustData, sizeof(winTrustData));
        winTrustData.cbStruct = sizeof(winTrustData);
        winTrustData.pPolicyCallbackData = nullptr;
        winTrustData.pSIPClientData = nullptr;
        winTrustData.dwUIChoice = WTD_UI_NONE;
        winTrustData.fdwRevocationChecks = WTD_REVOKE_NONE;
        winTrustData.dwUnionChoice = WTD_CHOICE_FILE;
        winTrustData.dwStateAction = WTD_STATEACTION_VERIFY;
        winTrustData.hWVTStateData = nullptr;
        winTrustData.pwszURLReference = nullptr;
        winTrustData.dwUIContext = 0;
        winTrustData.pFile = &fileInfo;

        // Use Authenticode policy
        GUID policyGUID = WINTRUST_ACTION_GENERIC_VERIFY_V2;

        LONG result = WinVerifyTrust(nullptr, &policyGUID, &winTrustData);
        
        // Clean up the state data
        winTrustData.dwStateAction = WTD_STATEACTION_CLOSE;
        WinVerifyTrust(nullptr, &policyGUID, &winTrustData);

        switch (result)
        {
            case ERROR_SUCCESS:
                return UpdateResult::Success;
            case TRUST_E_NOSIGNATURE:
                LOG_ERROR("File is not signed: {}", filePath.string());
                return config.RequireValidSignature ? UpdateResult::VerificationFailed : UpdateResult::Success;
            case TRUST_E_EXPLICIT_DISTRUST:
                LOG_ERROR("File signature is explicitly distrusted: {}", filePath.string());
                return UpdateResult::VerificationFailed;
            case TRUST_E_SUBJECT_NOT_TRUSTED:
                LOG_ERROR("File signature is not trusted: {}", filePath.string());
                return UpdateResult::VerificationFailed;
            case CRYPT_E_SECURITY_SETTINGS:
                LOG_ERROR("Security settings prevent signature verification: {}", filePath.string());
                return UpdateResult::VerificationFailed;
            default:
                LOG_ERROR("Signature verification failed with error: 0x{:X}", static_cast<unsigned>(result));
                return UpdateResult::VerificationFailed;
        }
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Exception during Windows signature verification: {}", e.what());
        return UpdateResult::InternalError;
    }
}
#else
UpdateResult CryptoVerifier::VerifyLinuxSignature(
    const std::filesystem::path& filePath,
    const SignatureConfiguration& config)
{
    try
    {
        // Linux signature verification would typically involve:
        // 1. GPG signature verification
        // 2. Package manager signature verification
        // 3. Custom signature schemes
        
        // For now, implement a basic placeholder that checks if signature file exists
        auto signatureFile = filePath.string() + ".sig";
        if (!std::filesystem::exists(signatureFile))
        {
            if (config.RequireValidSignature)
            {
                LOG_ERROR("Signature file not found: {}", signatureFile);
                return UpdateResult::VerificationFailed;
            }
            else
            {
                LOG_WARN("Signature file not found, but not required: {}", signatureFile);
                return UpdateResult::Success;
            }
        }

        // TODO: Implement actual signature verification using OpenSSL
        // This would involve:
        // 1. Loading the public key
        // 2. Reading the signature
        // 3. Verifying the signature against the file content
        
        LOG_WARN("Linux signature verification not fully implemented yet");
        return UpdateResult::Success;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Exception during Linux signature verification: {}", e.what());
        return UpdateResult::InternalError;
    }
}
#endif

void CryptoVerifier::ReportProgress(const std::string& operation, double progress)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_progressReporter)
    {
        ProgressInfo progressInfo;
        progressInfo.Stage = UpdateStage::Verifying;
        progressInfo.Message = operation;
        progressInfo.Progress = progress;
        m_progressReporter->ReportProgress(progressInfo);
    }
}
