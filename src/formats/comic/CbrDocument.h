/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#ifndef QUANTILYX_CBRDOCUMENT_H
#define QUANTILYX_CBRDOCUMENT_H

#include "../../core/Document.h"
#include <memory>
#include <QList>
#include <QMap>
#include <QImage>

// Forward declaration of UnRAR structures/functions (defined in unrar.h if using unrar library)
// For this example, we'll assume a generic interface or a C++ wrapper around unrar.
// extern "C" {
//     #include "unrar.h" // Hypothetical header
// }

namespace QuantilyxDoc {

class ComicPage; // Reuse the same page class if possible

/**
 * @brief Comic Book RAR (CBR) document implementation.
 * 
 * Handles loading of CBR files (RAR archives containing image files).
 * Treats each image file as a page.
 * NOTE: RAR is proprietary. Using unrar library (based on unRAR source) is common but has licensing implications.
 */
class CbrDocument : public Document
{
    Q_OBJECT

public:
    /**
     * @brief Constructor.
     * @param parent Parent object.
     */
    explicit CbrDocument(QObject* parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~CbrDocument() override;

    // --- Document Interface Implementation ---
    bool load(const QString& filePath, const QString& password = QString()) override;
    bool save(const QString& filePath = QString()) override; // Saving CBR requires writing RAR (complex)
    DocumentType type() const override;
    int pageCount() const override;
    Page* page(int index) const override;
    bool isLocked() const override; // CBRs can be password-protected
    bool isEncrypted() const override;
    QString formatVersion() const override; // Could be "RAR" or a specific version
    bool supportsFeature(const QString& feature) const override;

    // --- CBR-Specific Metadata ---
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

    // --- CBR-Specific Functionality ---
    /**
     * @brief Extract a specific image file to a given path.
     * @param imagePath Path of the image inside the archive.
     * @param outputPath Path to save the extracted image.
     * @return True if extraction was successful.
     */
    bool extractImage(const QString& imagePath, const QString& outputPath) const;

signals:
    /**
     * @brief Emitted when the CBR file is fully loaded and parsed.
     */
    void cbrLoaded();

private:
    class Private;
    std::unique_ptr<Private> d;

    // Helper to load the RAR archive and list files
    bool loadArchive(const QString& filePath, const QString& password);
    // Helper to parse ComicInfo.xml if present
    void parseComicInfo();
    // Helper to create ComicPage objects based on image list
    void createPages();
};

} // namespace QuantilyxDoc

#endif // QUANTILYX_CBRDOCUMENT_H