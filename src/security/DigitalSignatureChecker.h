/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#ifndef QUANTILYX_DIGITALSIGNATURECHECKER_H
#define QUANTILYX_DIGITALSIGNATURECHECKER_H

#include <QObject>
#include <QList>
#include <QDateTime>
#include <QFuture>
#include <memory>

namespace QuantilyxDoc {

class Document; // Forward declaration

/**
 * @brief Enum describing the status of a digital signature check.
 */
enum class SignatureStatus {
    Valid,          // Signature is valid and trusted
    Invalid,        // Signature is invalid (broken, wrong cert, etc.)
    CertificateExpired, // Signing certificate has expired
    CertificateRevoked, // Signing certificate has been revoked
    CertificateUnknown, // Signing certificate is not trusted/known
    NotSigned,      // Document has no signatures
    Error,          // An error occurred during checking
    Unknown         // Status could not be determined
};

/**
 * @brief Structure holding information about a digital signature found in a document.
 */
struct SignatureInfo {
    QString signerName;           // Name of the person/entity who signed
    QString certificateSubject;   // Subject of the signing certificate
    QString certificateIssuer;    // Issuer of the signing certificate
    QDateTime signingTime;        // Time the signature was created
    SignatureStatus status;       // Status of the signature
    QString statusDetails;        // Detailed reason for status (e.g., "Certificate expired", "Invalid signature data")
    bool isSelfSigned;            // Whether the signing certificate is self-signed
    QString signatureFieldName;   // Name of the signature field in the PDF (if applicable)
    QString location;             // Location provided by the signer (if applicable)
    QString reason;               // Reason provided by the signer (e.g., "Approved", "Reviewed") (if applicable)
    // Add more fields as needed (certificate chain, OCSP/CRL status, etc.)
};

/**
 * @brief Checks for and validates digital signatures within documents.
 * 
 * Supports formats that can contain digital signatures (primarily PDF).
 * Verifies the authenticity and integrity of the signature against the signing certificate
 * and potentially a certificate authority (CA).
 */
class DigitalSignatureChecker : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructor.
     * @param parent Parent object.
     */
    explicit DigitalSignatureChecker(QObject* parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~DigitalSignatureChecker() override;

    /**
     * @brief Get singleton instance.
     * @return Reference to the global DigitalSignatureChecker instance.
     */
    static DigitalSignatureChecker& instance();

    /**
     * @brief Check a document file for digital signatures and validate them.
     * @param filePath Path to the document file.
     * @return List of SignatureInfo structures detailing each signature found and its status.
     */
    QList<SignatureInfo> checkSignatures(const QString& filePath) const;

    /**
     * @brief Check a loaded Document object for digital signatures and validate them.
     * @param document The loaded document object.
     * @return List of SignatureInfo structures detailing each signature found and its status.
     */
    QList<SignatureInfo> checkSignatures(Document* document) const;

    /**
     * @brief Check a document file for digital signatures and validate them asynchronously.
     * @param filePath Path to the document file.
     * @return A QFuture that will hold the list of SignatureInfo upon completion.
     */
    QFuture<QList<SignatureInfo>> checkSignaturesAsync(const QString& filePath) const;

    /**
     * @brief Check if a document file contains any digital signatures.
     * @param filePath Path to the document file.
     * @return True if signatures are present.
     */
    bool hasSignatures(const QString& filePath) const;

    /**
     * @brief Check if a loaded Document object contains any digital signatures.
     * @param document The loaded document object.
     * @return True if signatures are present.
     */
    bool hasSignatures(Document* document) const;

    /**
     * @brief Get the list of supported file formats (extensions) for signature checking.
     * @return List of supported extensions (e.g., "pdf").
     */
    QStringList supportedFormats() const;

    /**
     * @brief Set the path to the directory containing trusted CA certificates.
     * @param path Directory path.
     */
    void setCaCertificatePath(const QString& path);

    /**
     * @brief Get the path to the directory containing trusted CA certificates.
     * @return Directory path string.
     */
    QString caCertificatePath() const;

    /**
     * @brief Set whether to check the Certificate Revocation List (CRL) during validation.
     * @param check Whether to check CRL.
     */
    void setCheckCrlEnabled(bool check);

    /**
     * @brief Check if CRL checking is enabled.
     * @return True if CRL checking is enabled.
     */
    bool isCrlCheckEnabled() const;

    /**
     * @brief Set whether to check the Online Certificate Status Protocol (OCSP) during validation.
     * @param check Whether to check OCSP.
     */
    void setCheckOcspEnabled(bool check);

    /**
     * @brief Check if OCSP checking is enabled.
     * @return True if OCSP checking is enabled.
     */
    bool isOcspCheckEnabled() const;

signals:
    /**
     * @brief Emitted when signature checking starts for a file.
     * @param filePath Path to the file being checked.
     */
    void signatureCheckStarted(const QString& filePath);

    /**
     * @brief Emitted when signature checking finishes for a file.
     * @param filePath Path to the file that was checked.
     * @param signatures List of SignatureInfo structures found.
     */
    void signatureCheckFinished(const QString& filePath, const QList<QuantilyxDoc::DigitalSignatureChecker::SignatureInfo>& signatures);

    /**
     * @brief Emitted when signature checking fails.
     * @param filePath Path to the file that failed checking.
     * @param error Error message.
     */
    void signatureCheckFailed(const QString& filePath, const QString& error);

    /**
     * @brief Emitted periodically during a long-running signature check task.
     * @param progress Progress percentage (0-100).
     */
    void signatureCheckProgress(int progress);

private:
    class Private;
    std::unique_ptr<Private> d;

    // Helper to map internal signature status codes to our enum
    SignatureStatus mapInternalStatus(int internalStatus) const;
};

} // namespace QuantilyxDoc

#endif // QUANTILYX_DIGITALSIGNATURECHECKER_H