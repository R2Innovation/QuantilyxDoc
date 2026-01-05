/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#ifndef QUANTILYX_CHECKSUMVERIFIER_H
#define QUANTILYX_CHECKSUMVERIFIER_H

#include <QObject>
#include <QHash>
#include <QList>
#include <QFuture>
#include <memory>
#include <QCryptographicHash>

namespace QuantilyxDoc {

/**
 * @brief Enum describing the type of checksum/hash algorithm used.
 */
enum class ChecksumAlgorithm {
    MD5,
    SHA1,
    SHA256,
    SHA512,
    CRC32, // Not a cryptographic hash, but useful for basic integrity checks
    Unknown
};

/**
 * @brief Structure holding the result of a checksum verification.
 */
struct VerificationResult {
    bool success;                 // Whether the checksum matched
    QString filePath;             // Path of the verified file
    QString expectedChecksum;     // The checksum that was expected
    QString calculatedChecksum;   // The checksum calculated from the file
    ChecksumAlgorithm algorithm;  // The algorithm used
    qint64 fileSize;              // Size of the file checked
    QString errorMessage;         // Error message if success is false due to an error (not mismatch)
};

/**
 * @brief Verifies the integrity of documents and files using checksums/hashes.
 * 
 * Calculates checksums (MD5, SHA1, SHA256, etc.) for files and compares them
 * against known good values to ensure they have not been tampered with or corrupted.
 */
class ChecksumVerifier : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructor.
     * @param parent Parent object.
     */
    explicit ChecksumVerifier(QObject* parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~ChecksumVerifier() override;

    /**
     * @brief Get singleton instance.
     * @return Reference to the global ChecksumVerifier instance.
     */
    static ChecksumVerifier& instance();

    /**
     * @brief Calculate the checksum of a file using a specific algorithm.
     * @param filePath Path to the file.
     * @param algorithm The hash algorithm to use.
     * @return The calculated checksum string, or an empty string on failure.
     */
    QString calculateChecksum(const QString& filePath, ChecksumAlgorithm algorithm) const;

    /**
     * @brief Verify a file against an expected checksum.
     * @param filePath Path to the file to verify.
     * @param expectedChecksum The expected checksum string.
     * @param algorithm The algorithm used to generate the expected checksum.
     * @return A VerificationResult struct indicating success/failure and details.
     */
    VerificationResult verifyFile(const QString& filePath, const QString& expectedChecksum, ChecksumAlgorithm algorithm) const;

    /**
     * @brief Verify a file against an expected checksum asynchronously.
     * @param filePath Path to the file to verify.
     * @param expectedChecksum The expected checksum string.
     * @param algorithm The algorithm used to generate the expected checksum.
     * @return A QFuture that will hold the VerificationResult upon completion.
     */
    QFuture<VerificationResult> verifyFileAsync(const QString& filePath, const QString& expectedChecksum, ChecksumAlgorithm algorithm) const;

    /**
     * @brief Get the list of supported checksum algorithms.
     * @return List of ChecksumAlgorithm enums.
     */
    QList<ChecksumAlgorithm> supportedAlgorithms() const;

    /**
     * @brief Convert a ChecksumAlgorithm enum to its string representation (e.g., "SHA256").
     * @param algorithm The algorithm enum.
     * @return String representation.
     */
    static QString algorithmToString(ChecksumAlgorithm algorithm);

    /**
     * @brief Convert a string representation to a ChecksumAlgorithm enum.
     * @param str String representation (e.g., "sha256", "MD5").
     * @return The algorithm enum, or ChecksumAlgorithm::Unknown if the string is invalid.
     */
    static ChecksumAlgorithm stringToAlgorithm(const QString& str);

    /**
     * @brief Get the standard file extension associated with a checksum file (e.g., ".sha256" for SHA256).
     * @param algorithm The algorithm.
     * @return File extension string (including the dot).
     */
    static QString fileExtensionForAlgorithm(ChecksumAlgorithm algorithm);

    /**
     * @brief Attempt to read an expected checksum from a standard checksum file (e.g., document.pdf.sha256).
     * @param filePath Path to the original file (e.g., "document.pdf").
     * @param algorithm The algorithm to look for.
     * @return The checksum string read from the file, or an empty string if not found or invalid.
     */
    QString readChecksumFromFile(const QString& filePath, ChecksumAlgorithm algorithm) const;

    /**
     * @brief Generate a standard checksum file (e.g., document.pdf.sha256) for a given file.
     * @param filePath Path to the original file.
     * @param algorithm The algorithm to use for the checksum.
     * @return True if the checksum file was generated successfully.
     */
    bool generateChecksumFile(const QString& filePath, ChecksumAlgorithm algorithm) const;

signals:
    /**
     * @brief Emitted when a checksum verification starts.
     * @param filePath Path to the file being verified.
     */
    void verificationStarted(const QString& filePath);

    /**
     * @brief Emitted when a checksum verification finishes successfully.
     * @param result The VerificationResult struct containing details.
     */
    void verificationFinished(const QuantilyxDoc::ChecksumVerifier::VerificationResult& result);

    /**
     * @brief Emitted when a checksum verification fails (due to error or mismatch).
     * @param filePath Path to the file that failed verification.
     * @param error Error message.
     */
    void verificationFailed(const QString& filePath, const QString& error);

    /**
     * @brief Emitted periodically during a long-running checksum calculation/verification task.
     * @param progress Progress percentage (0-100).
     */
    void verificationProgress(int progress);

private:
    class Private;
    std::unique_ptr<Private> d;

    // Helper to map our ChecksumAlgorithm enum to QCryptographicHash::Algorithm
    static QCryptographicHash::Algorithm quantilyxToQcHashAlgorithm(ChecksumAlgorithm algo);
};

} // namespace QuantilyxDoc

#endif // QUANTILYX_CHECKSUMVERIFIER_H