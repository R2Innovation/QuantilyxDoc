/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#ifndef QUANTILYX_DXFDOCUMENT_H
#define QUANTILYX_DXFDOCUMENT_H

#include "../../core/Document.h"
#include <memory>
#include <QList>

namespace QuantilyxDoc {

class DxfPage; // Forward declaration

/**
 * @brief DXF (Drawing Exchange Format) document implementation.
 * 
 * Handles loading and parsing of DXF files (2D/3D CAD data).
 * Rendering typically requires a CAD library or conversion to vector graphics.
 */
class DxfDocument : public Document
{
    Q_OBJECT

public:
    explicit DxfDocument(QObject* parent = nullptr);
    ~DxfDocument() override;

    // --- Document Interface Implementation ---
    bool load(const QString& filePath, const QString& password = QString()) override;
    bool save(const QString& filePath = QString()) override;
    DocumentType type() const override;
    int pageCount() const override; // Often single page for 2D, or layers as pages
    Page* page(int index) const override;
    bool isLocked() const override;
    bool isEncrypted() const override;
    QString formatVersion() const override; // e.g., "AC1027" for AutoCAD 2013
    bool supportsFeature(const QString& feature) const override;

    // --- DXF-Specific Metadata ---
    QString drawingName() const;
    QString drawingUnits() const;
    QList<QString> layerNames() const;
    int entityCount() const; // Number of entities (lines, circles, etc.)
    bool is3dDrawing() const;

signals:
    void dxfLoaded();

private:
    class Private;
    std::unique_ptr<Private> d;

    void createPages();
};

} // namespace QuantilyxDoc

#endif // QUANTILYX_DXFDOCUMENT_H