/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#ifndef QUANTILYX_PSDOCUMENT_H
#define QUANTILYX_PSDOCUMENT_H

#include "../../core/Document.h"
#include <memory>
#include <QList>
#include <QDateTime>

namespace QuantilyxDoc {

class PsPage; // Forward declaration

/**
 * @brief PostScript document implementation.
 * 
 * Handles loading and parsing of PostScript (.ps, .eps) files.
 * Rendering typically requires an interpreter like Ghostscript.
 */
class PsDocument : public Document
{
    Q_OBJECT

public:
    /**
     * @brief Constructor.
     * @param parent Parent object.
     */
    explicit PsDocument(QObject* parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~PsDocument() override;

    // --- Document Interface Implementation ---
    bool load(const QString& filePath, const QString& password = QString()) override;
    bool save(const QString& filePath = QString()) override; // Saving PS requires generating PS code
    DocumentType type() const override;
    int pageCount() const override;
    Page* page(int index) const override;
    bool isLocked() const override; // PS can have security settings
    bool isEncrypted() const override;
    QString formatVersion() const override; // e.g., "PostScript Level 2", "EPS"
    bool supportsFeature(const QString& feature) const override;

    // --- PS-Specific Metadata ---
    /**
     * @brief Get the PostScript level (e.g., 2, 3).
     * @return Level number.
     */
    int psLevel() const;

    /**
     * @brief Get the bounding box of the document (from %%BoundingBox DSC comment).
     * @return QRectF representing the document area.
     */
    QRectF documentBoundingBox() const;

    /**
     * @brief Get the intended resolution (from %%HiResBoundingBox DSC comment).
     * @return Resolution in DPI as a QSize.
     */
    QSize intendedResolution() const;

    /**
     * @brief Check if the document is Encapsulated PostScript (EPS).
     * @return True if EPS.
     */
    bool isEps() const;

    // --- PS-Specific Functionality ---
    /**
     * @brief Get the raw PostScript code of the document.
     * @return PS code as QString.
     */
    QString postScriptCode() const;

    /**
     * @brief Check if a specific page contains EPS-specific structures (e.g., preview bitmap).
     * @param pageIndex Index of the page.
     * @return True if page has EPS structures.
     */
    bool pageHasEpsStructures(int pageIndex) const;

    /**
     * @brief Export the document as a high-quality image sequence.
     * @param outputDirectory Directory to save the images.
     * @param format Output image format (e.g., "tiff", "png").
     * @param resolution DPI for rendering.
     * @return True if export was successful.
     */
    bool exportAsImageSequence(const QString& outputDirectory, const QString& format = "tiff", int resolution = 300) const;

signals:
    /**
     * @brief Emitted when the PS file is fully loaded and parsed.
     */
    void psLoaded();

private:
    class Private;
    std::unique_ptr<Private> d;

    // Helper to parse the PS file header and DSC comments
    bool parseHeader();
    // Helper to count pages (often involves parsing for showpage commands or DSC pages)
    bool countPages();
    // Helper to create PsPage objects
    void createPages();
};

} // namespace QuantilyxDoc

#endif // QUANTILYX_PSDOCUMENT_H