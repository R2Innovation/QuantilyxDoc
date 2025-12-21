/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "ComicPage.h"
#include "CbzDocument.h" // Example of a document type that might own this page
#include "CbrDocument.h" // Example of a document type that might own this page
#include "../../core/Logger.h"
#include <QImage>
#include <QPainter>
#include <QBuffer>
#include <QFileInfo>
#include <QRegularExpression>
#include <QDebug>

namespace QuantilyxDoc {

class ComicPage::Private {
public:
    Private(Document* doc, int pIndex, const QString& imgPath)
        : document(doc), pageIndexVal(pIndex), imagePathVal(imgPath) {}

    Document* document;
    int pageIndexVal;
    QString imagePathVal;
    QImage cachedImage; // Cache the loaded image
    QSize originalImageSize;
    QString mimeType;
    bool loaded = false;

    // Helper to load the image from the document's archive or from a file path
    bool loadImage() {
        if (loaded && !cachedImage.isNull()) return true; // Already loaded

        // Determine if this belongs to an archive-based document (CBZ, CBR) or a single image
        // This requires checking the dynamic type of 'document' or having a specific path resolution method in the document.
        // For now, let's assume the imagePathVal is relative to an archive or is an absolute path for single images.
        // We'll try to get the data via the Document's getFileContent method if it's archive-based.

        CbzDocument* cbzDoc = dynamic_cast<CbzDocument*>(document);
        CbrDocument* cbrDoc = dynamic_cast<CbrDocument*>(document);

        QByteArray imageData;
        if (cbzDoc) {
            // Load from CBZ archive
            imageData = cbzDoc->getFileContent(imagePathVal); // This method needs to be added to CbzDocument if it doesn't exist for raw access
            // Or use the Private access if necessary: cbzDoc->d->readFileFromZip(imagePathVal);
        } else if (cbrDoc) {
            // Load from CBR archive (requires RAR library integration)
            // imageData = cbrDoc->getFileContent(imagePathVal); // This method needs RAR integration
             LOG_ERROR("ComicPage::loadImage: CBR loading requires RAR library integration, which is not available.");
             return false;
        } else {
            // Assume it's a path to a standalone image file
            QFile imageFile(imagePathVal);
            if (!imageFile.open(QIODevice::ReadOnly)) {
                LOG_ERROR("ComicPage::loadImage: Failed to open image file: " << imagePathVal);
                return false;
            }
            imageData = imageFile.readAll();
            imageFile.close();
        }

        if (imageData.isEmpty()) {
            LOG_ERROR("ComicPage::loadImage: No image data retrieved for: " << imagePathVal);
            return false;
        }

        // Load image from byte array
        QBuffer buffer(&imageData);
        buffer.open(QIODevice::ReadOnly);
        cachedImage.load(&buffer, nullptr); // nullptr lets Qt detect format

        if (cachedImage.isNull()) {
            LOG_ERROR("ComicPage::loadImage: Failed to load image from data: " << imagePathVal);
            return false;
        }

        originalImageSize = cachedImage.size();
        // Determine MIME type based on file extension or image format
        QFileInfo info(imagePathVal);
        QString suffix = info.suffix().toLower();
        if (suffix == "jpg" || suffix == "jpeg") mimeType = "image/jpeg";
        else if (suffix == "png") mimeType = "image/png";
        else if (suffix == "gif") mimeType = "image/gif";
        else if (suffix == "webp") mimeType = "image/webp";
        else if (suffix == "bmp") mimeType = "image/bmp";
        else if (suffix == "tiff" || suffix == "tif") mimeType = "image/tiff";
        else mimeType = "image/unknown";

        loaded = true;
        LOG_DEBUG("ComicPage::loadImage: Loaded image for page " << pageIndexVal << ", size: " << originalImageSize);
        return true;
    }
};

ComicPage::ComicPage(Document* document, int pageIndex, const QString& imagePath, QObject* parent)
    : Page(document, parent) // Pass the base Document*
    , d(new Private(document, pageIndex, imagePath))
{
    LOG_DEBUG("ComicPage created for index " << pageIndex << " from file: " << imagePath);
}

ComicPage::~ComicPage()
{
    LOG_DEBUG("ComicPage for index " << d->pageIndexVal << " destroyed.");
}

QImage ComicPage::render(int width, int height, int dpi)
{
    Q_UNUSED(dpi); // For simple image scaling, DPI might be handled by the caller via width/height

    if (!d->loadImage()) {
        LOG_WARN("ComicPage::render: Failed to load image for page " << d->pageIndexVal);
        return QImage(); // Return null image
    }

    if (d->cachedImage.isNull()) {
        LOG_WARN("ComicPage::render: Cached image is null for page " << d->pageIndexVal);
        return QImage();
    }

    // Scale the image to the requested size
    QImage scaledImage = d->cachedImage.scaled(width, height, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    LOG_DEBUG("ComicPage::render: Rendered page " << d->pageIndexVal << " to size " << scaledImage.size());
    return scaledImage;
}

QString ComicPage::text() const
{
    // Static images typically don't have selectable text unless OCR was performed.
    // This would require an OCR engine to be run on the image.
    LOG_DEBUG("ComicPage::text: No inherent text in static image. OCR required for text extraction.");
    return QString(); // Return empty string
}

QList<QRectF> ComicPage::searchText(const QString& text, bool caseSensitive, bool wholeWords) const
{
    Q_UNUSED(text);
    Q_UNUSED(caseSensitive);
    Q_UNUSED(wholeWords);
    // Searching text on a static image without OCR text layer is not feasible.
    LOG_DEBUG("ComicPage::searchText: Cannot search text on static image without OCR text layer.");
    return QList<QRectF>(); // Return empty list
}

QObject* ComicPage::hitTest(const QPointF& position) const
{
    Q_UNUSED(position);
    // For a static image page, hit testing usually just confirms the point is within the page bounds.
    // There are no interactive elements like links or selectable text regions unless OCR data is available.
    // Return nullptr or maybe the page object itself?
    // For now, return nullptr.
    LOG_DEBUG("ComicPage::hitTest: Hit testing on static image returns nullptr.");
    return nullptr;
}

QList<QObject*> ComicPage::links() const
{
    // Static images do not contain hyperlinks.
    LOG_DEBUG("ComicPage::links: Static image contains no links.");
    return QList<QObject*>(); // Return empty list
}

QVariantMap ComicPage::metadata() const
{
    if (!d->loadImage()) {
         LOG_WARN("ComicPage::metadata: Failed to load image to get metadata for page " << d->pageIndexVal);
         return QVariantMap();
    }

    QVariantMap map;
    map["Index"] = d->pageIndexVal;
    map["ImagePath"] = d->imagePathVal;
    map["OriginalSizePixels"] = d->originalImageSize;
    map["MimeType"] = d->mimeType;
    map["HasAlpha"] = d->cachedImage.hasAlphaChannel();
    map["ColorDepth"] = d->cachedImage.depth();
    // Add more specific image metadata if available from QImage or loaded format
    return map;
}

QString ComicPage::imagePath() const
{
    return d->imagePathVal;
}

QSize ComicPage::imageSize() const
{
    if (!d->loadImage()) {
        LOG_WARN("ComicPage::imageSize: Failed to load image to get size for page " << d->pageIndexVal);
        return QSize();
    }
    return d->originalImageSize;
}

QString ComicPage::imageMimeType() const
{
    if (!d->loadImage()) {
        LOG_WARN("ComicPage::imageMimeType: Failed to load image to get MIME type for page " << d->pageIndexVal);
        return QString();
    }
    return d->mimeType;
}

bool ComicPage::hasTransparency() const
{
    if (!d->loadImage()) {
        LOG_WARN("ComicPage::hasTransparency: Failed to load image to check transparency for page " << d->pageIndexVal);
        return false;
    }
    return d->cachedImage.hasAlphaChannel();
}

int ComicPage::colorDepth() const
{
    if (!d->loadImage()) {
        LOG_WARN("ComicPage::colorDepth: Failed to load image to get color depth for page " << d->pageIndexVal);
        return 0;
    }
    return d->cachedImage.depth();
}

} // namespace QuantilyxDoc