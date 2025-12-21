/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#ifndef QUANTILYX_IMAGEDOCUMENT_H
#define QUANTILYX_IMAGEDOCUMENT_H

#include "../../core/Document.h"
#include <memory>
#include <QList>
#include <QStringList>

namespace QuantilyxDoc {

class ComicPage; // Reuse ComicPage for single images

/**
 * @brief Image document implementation for single-page image formats.
 * 
 * Handles loading of common image formats (JPEG, PNG, GIF, etc.).
 */
class ImageDocument : public Document
{
    Q_OBJECT

public:
    explicit ImageDocument(QObject* parent = nullptr);
    ~ImageDocument() override;

    // --- Document Interface Implementation ---
    bool load(const QString& filePath, const QString& password = QString()) override;
    bool save(const QString& filePath = QString()) override;
    DocumentType type() const override;
    int pageCount() const override; // Always 1 for single images
    Page* page(int index) const override;
    bool isLocked() const override;
    bool isEncrypted() const override;
    QString formatVersion() const override; // e.g., "JPEG", "PNG"
    bool supportsFeature(const QString& feature) const override;

    // --- Image-Specific Properties ---
    QString mimeType() const;
    QSize imageSize() const;
    bool hasAlpha() const;
    int colorDepth() const;
    QString colorSpace() const;

signals:
    void imageLoaded();

private:
    class Private;
    std::unique_ptr<Private> d;

    void createPages();
};

} // namespace QuantilyxDoc

#endif // QUANTILYX_IMAGEDOCUMENT_H