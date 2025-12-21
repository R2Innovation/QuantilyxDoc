/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#ifndef QUANTILYX_COMICPAGE_H
#define QUANTILYX_COMICPAGE_H

#include "../../core/Page.h"
#include <memory>
#include <QSizeF>
#include <QRectF>
#include <QImage>

namespace QuantilyxDoc {

class Document; // Base document class

/**
 * @brief Generic page implementation for image-based documents (CBZ, CBR, potentially single-image formats).
 * 
 * Represents a single image file within an archive or a standalone image.
 * Handles rendering the image and potentially extracting text if OCR is applied.
 */
class ComicPage : public Page
{
    Q_OBJECT

public:
    /**
     * @brief Constructor for an image-based page.
     * @param document The parent Document this page belongs to.
     * @param pageIndex The 0-based index of this page within the document.
     * @param imagePath The path to the image file (inside an archive or standalone) corresponding to this page.
     * @param parent Parent object.
     */
    explicit ComicPage(Document* document, int pageIndex, const QString& imagePath, QObject* parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~ComicPage() override;

    // --- Page Interface Implementation ---
    QImage render(int width, int height, int dpi = 72) override;
    QString text() const override; // Might be empty or come from OCR
    QList<QRectF> searchText(const QString& text, bool caseSensitive = false, bool wholeWords = false) const override;
    QObject* hitTest(const QPointF& position) const override; // Could return nullptr or an image object if supported
    QList<QObject*> links() const override; // Likely empty for static images
    QVariantMap metadata() const override;

    // --- Image-Specific Page Properties ---
    /**
     * @brief Get the path to the image file for this page.
     * @return File path string.
     */
    QString imagePath() const;

    /**
     * @brief Get the original size of the image file.
     * @return Size in pixels.
     */
    QSize imageSize() const;

    /**
     * @brief Get the MIME type of the image file.
     * @return MIME type string (e.g., "image/jpeg").
     */
    QString imageMimeType() const;

    /**
     * @brief Check if the image has transparency (e.g., PNG with alpha channel).
     * @return True if the image has an alpha channel.
     */
    bool hasTransparency() const;

    /**
     * @brief Get the color depth of the image (e.g., 24, 32).
     * @return Color depth in bits per pixel.
     */
    int colorDepth() const;

signals:
    /**
     * @brief Emitted when the page's image content is loaded or changes significantly.
     */
    void imageLoaded();

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace QuantilyxDoc

#endif // QUANTILYX_COMICPAGE_H