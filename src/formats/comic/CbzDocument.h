/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#ifndef QUANTILYX_CBZDOCUMENT_H
#define QUANTILYX_CBZDOCUMENT_H

#include "../../core/Document.h"
#include <memory>
#include <QList>
#include <QMap>
#include <QImage>

namespace QuantilyxDoc {

class ComicPage; // Reuse a generic page class if structure is similar enough, or create CbzPage

/**
 * @brief Comic Book ZIP (CBZ) document implementation.
 * 
 * Handles loading of CBZ files (ZIP archives containing image files).
 * Treats each image file as a page.
 */
class CbzDocument : public Document
{
    Q_OBJECT

public:
    /**
     * @brief Constructor.
     * @param parent Parent object.
     */
    explicit CbzDocument(QObject* parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~CbzDocument() override;

    // --- Document Interface Implementation ---
    bool load(const QString& filePath, const QString& password = QString()) override;
    bool save(const QString& filePath = QString()) override; // Saving CBZ requires writing ZIP
    DocumentType type() const override;
    int pageCount() const override;
    Page* page(int index) const override;
    bool isLocked() const override; // CBZs are typically not password-protected
    bool isEncrypted() const override;
    QString formatVersion() const override; // Could be "ZIP" or a specific comic spec version if applicable
    bool supportsFeature(const QString& feature) const override;

    // --- CBZ-Specific Metadata ---
    /**
     * @brief Get the list of all image file paths inside the archive, in order.
     * @return List of file paths.
     */
    QStringList imagePaths() const;

    /**
     * @brief Get the list of all non-image files (e.g., metadata.xml).
     * @return List of file paths.
     */
    QStringList otherFiles() const;

    /**
     * @brief Check if the archive contains a ComicInfo.xml file.
     * @return True if ComicInfo.xml exists.
     */
    bool hasComicInfo() const;

    /**
     * @brief Get the raw content of the ComicInfo.xml file.
     * @return XML content as QString.
     */
    QString comicInfoXml() const;

    // --- CBZ-Specific Functionality ---
    /**
     * @brief Extract a specific image file to a given path.
     * @param imagePath Path of the image inside the archive.
     * @param outputPath Path to save the extracted image.
     * @return True if extraction was successful.
     */
    bool extractImage(const QString& imagePath, const QString& outputPath) const;

signals:
    /**
     * @brief Emitted when the CBZ file is fully loaded and parsed.
     */
    void cbzLoaded();

private:
    class Private;
    std::unique_ptr<Private> d;

    // Helper to load the ZIP archive and list files
    bool loadArchive(const QString& filePath);
    // Helper to parse ComicInfo.xml if present
    void parseComicInfo();
    // Helper to create ComicPage objects based on image list
    void createPages();
};

} // namespace QuantilyxDoc

#endif // QUANTILYX_CBZDOCUMENT_H