/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "ChecksumVerifier.h"
#include "../core/Logger.h"
#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QDir>
#include <QStandardPaths>
#include <QTextStream>
#include <QRegularExpression>
#include <QFuture>
#include <QtConcurrent/QtConcurrent>
#include <QMutex>
#include <QMutexLocker>
#include <QDebug>

namespace QuantilyxDoc {

class ChecksumVerifier::Private {
public:
    Private(ChecksumVerifier* q_ptr)
        : q(q_ptr) {}

    ChecksumVerifier* q;
    mutable QMutex mutex; // Protect access if concurrent verification is needed
    // Could store a list of known good checksums here for quick lookup if needed.
    // For now, we'll just calculate and compare on demand.
};

// Static instance pointer
ChecksumVerifier* ChecksumVerifier::s_instance = nullptr;

ChecksumVerifier& ChecksumVerifier::instance()
{
    if (!s_instance) {
        s_instance = new ChecksumVerifier();
    }
    return *s_instance;
}

ChecksumVerifier::ChecksumVerifier(QObject* parent)
    : QObject(parent)
    , d(new Private(this))
{
    LOG_INFO("ChecksumVerifier created.");
}

ChecksumVerifier::~ChecksumVerifier()
{
    LOG_INFO("ChecksumVerifier destroyed.");
}

QString ChecksumVerifier::calculateChecksum(const QString& filePath, ChecksumAlgorithm algorithm) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        LOG_ERROR("ChecksumVerifier::calculateChecksum: Failed to open file for reading: " << filePath);
        return QString();
    }

    QCryptographicHash hasher(mapToQcHashAlgorithm(algorithm));

    if (!hasher.addData(&file)) {
        LOG_ERROR("ChecksumVerifier::calculateChecksum: Failed to read data from file: " << filePath);
        return QString();
    }

    QString checksum = hasher.result().toHex();
    LOG_DEBUG("ChecksumVerifier: Calculated " << algorithmToString(algorithm) << " checksum for " << filePath << ": " << checksum);
    return checksum;
}

ChecksumVerifier::VerificationResult ChecksumVerifier::verifyFile(const QString& filePath, const QString& expectedChecksum, ChecksumAlgorithm algorithm) const
{
    VerificationResult result;
    result.filePath = filePath;
    result.expectedChecksum = expectedChecksum;
    result.algorithm = algorithm;
    result.success = false; // Assume failure initially

    if (expectedChecksum.isEmpty()) {
        result.errorMessage = "Expected checksum is empty.";
        LOG_ERROR("ChecksumVerifier::verifyFile: " << result.errorMessage);
        emit verificationFailed(filePath, result.errorMessage);
        return result;
    }

    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        result.errorMessage = "File does not exist.";
        LOG_ERROR("ChecksumVerifier::verifyFile: " << result.errorMessage << " Path: " << filePath);
        emit verificationFailed(filePath, result.errorMessage);
        return result;
    }

    result.fileSize = fileInfo.size();

    QString calculatedChecksum = calculateChecksum(filePath, algorithm);
    if (calculatedChecksum.isEmpty()) {
        result.errorMessage = "Failed to calculate checksum.";
        LOG_ERROR("ChecksumVerifier::verifyFile: " << result.errorMessage << " for file: " << filePath);
        emit verificationFailed(filePath, result.errorMessage);
        return result;
    }

    result.calculatedChecksum = calculatedChecksum;
    result.success = (calculatedChecksum.compare(expectedChecksum, Qt::CaseInsensitive) == 0);

    if (result.success) {
        LOG_INFO("ChecksumVerifier: File integrity verified for " << filePath << " using " << algorithmToString(algorithm));
        emit verificationFinished(result);
    } else {
        result.errorMessage = QString("Checksum mismatch. Expected: %1, Calculated: %2").arg(expectedChecksum, calculatedChecksum);
        LOG_ERROR("ChecksumVerifier::verifyFile: " << result.errorMessage);
        emit verificationFailed(filePath, result.errorMessage);
    }

    return result;
}

QFuture<ChecksumVerifier::VerificationResult> ChecksumVerifier::verifyFileAsync(const QString& filePath, const QString& expectedChecksum, ChecksumAlgorithm algorithm) const
{
    // Use QtConcurrent to run the verification in a separate thread
    return QtConcurrent::run(&ChecksumVerifier::verifyFile, this, filePath, expectedChecksum, algorithm);
}

QList<ChecksumVerifier::ChecksumAlgorithm> ChecksumVerifier::supportedAlgorithms() const
{
    // List all algorithms supported by QCryptographicHash that we map to our enum
    return QList<ChecksumAlgorithm>() << ChecksumAlgorithm::MD5
                                      << ChecksumAlgorithm::SHA1
                                      << ChecksumAlgorithm::SHA256
                                      << ChecksumAlgorithm::SHA512
                                      << ChecksumAlgorithm::CRC32; // Note: CRC32 is not in QCryptographicHash, would need a custom implementation or external library
}

QString ChecksumVerifier::algorithmToString(ChecksumAlgorithm algorithm)
{
    switch (algorithm) {
        case ChecksumAlgorithm::MD5: return "MD5";
        case ChecksumAlgorithm::SHA1: return "SHA1";
        case ChecksumAlgorithm::SHA256: return "SHA256";
        case ChecksumAlgorithm::SHA512: return "SHA512";
        case ChecksumAlgorithm::CRC32: return "CRC32";
        case ChecksumAlgorithm::Unknown: return "Unknown";
        default: return "Unknown";
    }
}

ChecksumVerifier::ChecksumAlgorithm ChecksumVerifier::stringToAlgorithm(const QString& str)
{
    QString upperStr = str.toUpper();
    if (upperStr == "MD5") return ChecksumAlgorithm::MD5;
    if (upperStr == "SHA1") return ChecksumAlgorithm::SHA1;
    if (upperStr == "SHA256") return ChecksumAlgorithm::SHA256;
    if (upperStr == "SHA512") return ChecksumAlgorithm::SHA512;
    if (upperStr == "CRC32") return ChecksumAlgorithm::CRC32;
    return ChecksumAlgorithm::Unknown;
}

QString ChecksumVerifier::fileExtensionForAlgorithm(ChecksumAlgorithm algorithm)
{
    switch (algorithm) {
        case ChecksumAlgorithm::MD5: return ".md5";
        case ChecksumAlgorithm::SHA1: return ".sha1";
        case ChecksumAlgorithm::SHA256: return ".sha256";
        case ChecksumAlgorithm::SHA512: return ".sha512";
        case ChecksumAlgorithm::CRC32: return ".crc32";
        case ChecksumAlgorithm::Unknown: return ".chk"; // Generic extension
        default: return ".chk";
    }
}

QString ChecksumVerifier::readChecksumFromFile(const QString& filePath, ChecksumAlgorithm algorithm) const
{
    QFileInfo fileInfo(filePath);
    QString checksumFilePath = fileInfo.absolutePath() + "/" + fileInfo.completeBaseName() + fileExtensionForAlgorithm(algorithm);

    QFile checksumFile(checksumFilePath);
    if (!checksumFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        LOG_WARN("ChecksumVerifier::readChecksumFromFile: Checksum file not found: " << checksumFilePath);
        return QString();
    }

    QTextStream stream(&checksumFile);
    QString line = stream.readLine().trimmed(); // Read the first line
    checksumFile.close();

    // The format of checksum files can vary (e.g., "checksum filename", "filename: checksum", just "checksum").
    // We'll assume the simplest case: just the checksum on the first line, or a line like "checksum *filename" (common for shasum tools).
    QRegularExpression checksumRegex(R"(^([a-fA-F0-9]+)\s*(\*?\s*"?)?" + QRegularExpression::escape(fileInfo.fileName()) + R"(("?\s*)?$)");
    QRegularExpressionMatch match = checksumRegex.match(line);
    if (match.hasMatch()) {
        QString extractedChecksum = match.captured(1);
        LOG_DEBUG("ChecksumVerifier: Read " << algorithmToString(algorithm) << " checksum from file " << checksumFilePath << ": " << extractedChecksum);
        return extractedChecksum;
    } else {
        // If the line doesn't match the expected format containing the filename, assume the whole line *is* the checksum (less robust)
        if (line.length() == expectedChecksumLength(algorithm)) { // Helper to check length
             LOG_DEBUG("ChecksumVerifier: Read checksum from file " << checksumFilePath << " (format unclear): " << line);
             return line;
        }
    }

    LOG_WARN("ChecksumVerifier::readChecksumFromFile: Could not parse checksum from file: " << checksumFilePath << ". Content: " << line);
    return QString();
}

bool ChecksumVerifier::generateChecksumFile(const QString& filePath, ChecksumAlgorithm algorithm) const
{
    QString calculatedChecksum = calculateChecksum(filePath, algorithm);
    if (calculatedChecksum.isEmpty()) {
        LOG_ERROR("ChecksumVerifier::generateChecksumFile: Failed to calculate checksum for: " << filePath);
        return false;
    }

    QFileInfo fileInfo(filePath);
    QString checksumFilePath = fileInfo.absolutePath() + "/" + fileInfo.completeBaseName() + fileExtensionForAlgorithm(algorithm);

    QFile checksumFile(checksumFilePath);
    if (!checksumFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        LOG_ERROR("ChecksumVerifier::generateChecksumFile: Failed to open checksum file for writing: " << checksumFilePath);
        return false;
    }

    QTextStream stream(&checksumFile);
    // Standard format: checksum *filename (for binary mode, which is common)
    stream << calculatedChecksum << " *" << fileInfo.fileName() << "\n";
    checksumFile.close();

    LOG_INFO("ChecksumVerifier: Generated checksum file: " << checksumFilePath);
    return true;
}

// --- Private Helpers ---

QCryptographicHash::Algorithm ChecksumVerifier::mapToQcHashAlgorithm(ChecksumAlgorithm algo) const
{
    switch (algo) {
        case ChecksumAlgorithm::MD5: return QCryptographicHash::Md5;
        case ChecksumAlgorithm::SHA1: return QCryptographicHash::Sha1;
        case ChecksumAlgorithm::SHA256: return QCryptographicHash::Sha256;
        case ChecksumAlgorithm::SHA512: return QCryptographicHash::Sha512;
        // CRC32 is not available in QCryptographicHash
        case ChecksumAlgorithm::CRC32:
        case ChecksumAlgorithm::Unknown:
        default:
            LOG_WARN("ChecksumVerifier::mapToQcHashAlgorithm: Unsupported algorithm for QCryptographicHash: " << static_cast<int>(algo));
            return QCryptographicHash::Sha256; // Fallback
    }
}

int ChecksumVerifier::expectedChecksumLength(ChecksumAlgorithm algorithm) const
{
    switch (algorithm) {
        case ChecksumAlgorithm::MD5: return 32; // 128 bits in hex
        case ChecksumAlgorithm::SHA1: return 40; // 160 bits in hex
        case ChecksumAlgorithm::SHA256: return 64; // 256 bits in hex
        case ChecksumAlgorithm::SHA512: return 128; // 512 bits in hex
        case ChecksumAlgorithm::CRC32: return 8; // 32 bits in hex
        case ChecksumAlgorithm::Unknown:
        default: return 0;
    }
}

} // namespace QuantilyxDoc