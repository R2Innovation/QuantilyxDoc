/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#ifndef QUANTILYX_EPUBDOCUMENT_H
#define QUANTILYX_EPUBDOCUMENT_H

#include "../../core/Document.h"
#include <memory>
#include <QList>
#include <QMap>
#include <QDomDocument> // For parsing OPF and NCX
#include <QUrl>         // For resource paths

namespace QuantilyxDoc {

class EpubPage; // Forward declaration

/**
 * @brief EPUB document implementation.
 * 
 * Handles loading and parsing of EPUB files (which are ZIP archives).
 * Extracts content, metadata, and navigation structure (TOC).
 */
class EpubDocument : public Document
{
    Q_OBJECT

public:
    /**
     * @brief Constructor.
     * @param parent Parent object.
     */
    explicit EpubDocument(QObject* parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~EpubDocument() override;

    // --- Document Interface Implementation ---
    bool load(const QString& filePath, const QString& password = QString()) override;
    bool save(const QString& filePath = QString()) override; // Saving EPUB requires writing ZIP
    DocumentType type() const override;
    int pageCount() const override;
    Page* page(int index) const override;
    bool isLocked() const override; // EPUBs are typically not password-protected like PDFs
    bool isEncrypted() const override;
    QString formatVersion() const override; // e.g., "EPUB 2.0", "EPUB 3.0"
    bool supportsFeature(const QString& feature) const override;

    // --- EPUB-Specific Metadata ---
    /**
     * @brief Get the unique identifier for the EPUB.
     * @return UID string.
     */
    QString uid() const;

    /**
     * @brief Get the path to the main content file (usually an OPF file inside the archive).
     * @return Path string.
     */
    QString packagePath() const;

    /**
     * @brief Get the path to the navigation document (usually nav.xhtml or toc.ncx).
     * @return Path string.
     */
    QString navigationPath() const;

    // --- EPUB-Specific Functionality ---
    /**
     * @brief Get the list of all manifest items (HTML, CSS, images, etc.).
     * @return Map of ID -> file path.
     */
    QMap<QString, QString> manifestItems() const;

    /**
     * @brief Get the list of spine items (the reading order of content documents).
     * @return List of manifest IDs in reading order.
     */
    QStringList spineOrder() const;

    /**
     * @brief Get the raw content of a specific file within the EPUB archive.
     * @param filePath Path of the file inside the archive.
     * @return File content as QByteArray.
     */
    QByteArray getFileContent(const QString& filePath) const;

    /**
     * @brief Get the list of all embedded fonts.
     * @return List of font file paths.
     */
    QStringList embeddedFonts() const;

    /**
     * @brief Check if the EPUB has a table of contents.
     * @return True if TOC exists.
     */
    bool hasTableOfContents() const override;

    /**
     * @brief Get the table of contents structure.
     * @return TOC structure (e.g., QList<QVariantMap>).
     */
    QVariantList tableOfContents() const override;

    /**
     * @brief Get the list of all images referenced in the document.
     * @return List of image file paths.
     */
    QStringList imagePaths() const;

    /**
     * @brief Get the list of all hyperlinks in the document.
     * @return List of QUrl objects.
     */
    QList<QUrl> hyperlinks() const;

signals:
    /**
     * @brief Emitted when the EPUB file is fully loaded and parsed.
     */
    void epubLoaded();

private:
    class Private;
    std::unique_ptr<Private> d;

    // Helper to parse the OPF (Open Packaging Format) file
    bool parseOpf(const QDomDocument& opfDoc);
    // Helper to parse the NCX (Navigation Control file for XML) or nav.xhtml file
    bool parseNavigation(const QDomDocument& navDoc);
    // Helper to create EpubPage objects based on spine order
    void createPages();
};

} // namespace QuantilyxDoc

#endif // QUANTILYX_EPUBDOCUMENT_H