/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#ifndef QUANTILYX_DWGDOCUMENT_H
#define QUANTILYX_DWGDOCUMENT_H

#include "../../core/Document.h"
#include <memory>
#include <QList>
#include <QMap>

namespace QuantilyxDoc {

class DwgPage; // Forward declaration

/**
 * @brief DWG (AutoCAD Drawing) document implementation.
 * 
 * Handles loading and parsing of DWG files.
 * Requires ODA Teigha library or ODA File Converter tool.
 * This implementation uses the ODA File Converter via QProcess.
 */
class DwgDocument : public Document
{
    Q_OBJECT

public:
    explicit DwgDocument(QObject* parent = nullptr);
    ~DwgDocument() override;

    // --- Document Interface Implementation ---
    bool load(const QString& filePath, const QString& password = QString()) override;
    bool save(const QString& filePath = QString()) override;
    DocumentType type() const override;
    int pageCount() const override; // Often 1 for 2D, or based on layouts/views
    Page* page(int index) const override;
    bool isLocked() const override;
    bool isEncrypted() const override;
    QString formatVersion() const override; // e.g., "AC1027" for AutoCAD 2013
    bool supportsFeature(const QString& feature) const override;

    // --- DWG-Specific Metadata ---
    QString drawingName() const;
    QString drawingUnits() const;
    QList<QString> layerNames() const;
    int entityCount() const;
    bool is3dDrawing() const;

    // --- DWG-Specific Functionality ---
    /**
     * @brief Export the drawing as a high-quality image.
     * @param outputPath Path to save the image.
     * @param format Output image format (e.g., "png", "tiff").
     * @param resolution DPI for rendering.
     * @return True if export was successful.
     */
    bool exportAsImage(const QString& outputPath, const QString& format = "png", int resolution = 300) const;

signals:
    void dwgLoaded();

private:
    class Private;
    std::unique_ptr<Private> d;

    // Helper to find the ODA File Converter executable
    QString findOdaConverterExecutable() const;

    void createPages();
};

} // namespace QuantilyxDoc

#endif // QUANTILYX_DWGDOCUMENT_H