/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#ifndef QUANTILYX_DJVUDOCUMENT_H
#define QUANTILYX_DJVUDOCUMENT_H

#include "../../core/Document.h"
#include <memory>
#include <QList>
#include <QDateTime>

// Forward declaration of DjVuLibre structures (defined in ddjvuapi.h)
struct ddjvu_context_s;
struct ddjvu_document_s;
struct ddjvu_pageinfo_s;

namespace QuantilyxDoc {

class DjvuPage; // Forward declaration

/**
 * @brief DjVu document implementation using DjVuLibre.
 * 
 * Handles loading and parsing of DjVu files using the DjVuLibre library.
 * DjVu is excellent for multi-layered documents (text, background image, foreground mask).
 */
class DjvuDocument : public Document
{
    Q_OBJECT

public:
    /**
     * @brief Constructor.
     * @param parent Parent object.
     */
    explicit DjvuDocument(QObject* parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~DjvuDocument() override;

    // --- Document Interface Implementation ---
    bool load(const QString& filePath, const QString& password = QString()) override;
    bool save(const QString& filePath = QString()) override; // Saving DjVu requires writing, complex
    DocumentType type() const override;
    int pageCount() const override;
    Page* page(int index) const override;
    bool isLocked() const override; // DjVu can be password-protected
    bool isEncrypted() const override;
    QString formatVersion() const override; // e.g., "DjVu Version X"
    bool supportsFeature(const QString& feature) const override;

    // --- DjVu-Specific Metadata ---
    /**
     * @brief Get the DjVu file format version.
     * @return Version string.
     */
    QString djvuVersion() const;

    /**
     * @brief Get the overall document bounding box (in pixels).
     * @return QRectF representing the document size.
     */
    QRectF documentBoundingBox() const;

    /**
     * @brief Check if the document has a shared annotation chunk (shared across pages).
     * @return True if shared annotations exist.
     */
    bool hasSharedAnnotations() const;

    /**
     * @brief Get the list of embedded file names.
     * @return List of file names.
     */
    QStringList embeddedFiles() const;

    // --- DjVu-Specific Functionality ---
    /**
     * @brief Check if a specific page has a text layer.
     * @param pageIndex Index of the page.
     * @return True if the page has text.
     */
    bool pageHasText(int pageIndex) const;

    /**
     * @brief Check if a specific page has a foreground mask layer.
     * @param pageIndex Index of the page.
     * @return True if the page has a mask.
     */
    bool pageHasMask(int pageIndex) const;

    /**
     * @brief Get the average compression ratio of the document.
     * @return Compression ratio as a double.
     */
    double averageCompressionRatio() const;

    /**
     * @brief Export a specific page as a high-quality image.
     * @param pageIndex Index of the page to export.
     * @param outputPath Path to save the exported image.
     * @param format Output image format (e.g., "tiff", "png").
     * @return True if export was successful.
     */
    bool exportPageAsImage(int pageIndex, const QString& outputPath, const QString& format = "tiff") const;

signals:
    /**
     * @brief Emitted when the DjVu file is fully loaded and parsed.
     */
    void djvuLoaded();

private:
    class Private;
    std::unique_ptr<Private> d;

    // Helper to initialize DjVuLibre context and load document
    bool initializeAndLoadDocument(const QString& filePath);
    // Helper to query document info (page count, global metadata)
    bool queryDocumentInfo();
    // Helper to create DjvuPage objects
    void createPages();
};

} // namespace QuantilyxDoc

#endif // QUANTILYX_DJVUDOCUMENT_H