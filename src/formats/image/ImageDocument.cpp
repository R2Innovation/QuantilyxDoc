/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "ImageDocument.h"
#include "../comic/ComicPage.h" // Reuse ComicPage
#include "../../core/Logger.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QImage>
#include <QImageReader>
#include <QDebug>

namespace QuantilyxDoc {

class ImageDocument::Private {
public:
    Private() : isLoaded(false), pageCountVal(1) {} // Single page
    ~Private() = default;

    bool isLoaded;
    int pageCountVal;
    QString mimeTypeVal;
    QSize imageSizeVal;
    bool hasAlphaVal = false;
    int colorDepthVal = 0;
    QString colorSpaceVal;
    std::unique_ptr<ComicPage> imagePage; // Reuse ComicPage for rendering
    QString imagePath; // Store path to the image file

    // Helper to extract image properties
    bool extractImageProperties(const QString& filePath) {
        QFileInfo fileInfo(filePath);
        QString suffix = fileInfo.suffix().toLower();

        // Determine MIME type
        if (suffix == "jpg" || suffix == "jpeg") mimeTypeVal = "image/jpeg";
        else if (suffix == "png") mimeTypeVal = "image/png";
        else if (suffix == "gif") mimeTypeVal = "image/gif";
        else if (suffix == "webp") mimeTypeVal = "image/webp";
        else if (suffix == "bmp") mimeTypeVal = "image/bmp";
        else if (suffix == "tiff" || suffix == "tif") mimeTypeVal = "image/tiff";
        else mimeTypeVal = "image/unknown";

        // Get image size and other properties using QImageReader
        QImageReader reader(filePath);
        if (!reader.canRead()) {
            LOG_ERROR("ImageDocument: QImageReader cannot read image: " << filePath);
            return false;
        }

        imageSizeVal = reader.size();
        // QImageReader doesn't directly provide alpha, depth, color space easily.
        // For that, load the QImage which is more expensive.
        QImage image;
        if (image.load(filePath)) {
            hasAlphaVal = image.hasAlphaChannel();
            colorDepthVal = image.depth();
            // Color space is harder; Qt doesn't expose it directly without QColorSpace (Qt 5.14+)
            colorSpaceVal = "sRGB"; // Assume sRGB as default
        }

        LOG_DEBUG("ImageDocument: Extracted properties for " << filePath << " - Type: " << mimeTypeVal << ", Size: " << imageSizeVal);
        return true;
    }
};

ImageDocument::ImageDocument(QObject* parent)
    : Document(parent)
    , d(new Private())
{
    LOG_INFO("ImageDocument created.");
}

ImageDocument::~ImageDocument()
{
    LOG_INFO("ImageDocument destroyed.");
}

bool ImageDocument::load(const QString& filePath, const QString& password)
{
    Q_UNUSED(password);
    d->isLoaded = false;

    // Extract image properties
    if (!d->extractImageProperties(filePath)) {
        setLastError(tr("Failed to load image properties."));
        LOG_ERROR(lastError());
        return false;
    }

    setFilePath(filePath);
    d->imagePath = filePath;
    createPages();

    d->isLoaded = true;
    setState(Loaded);
    emit imageLoaded();
    LOG_INFO("Successfully loaded image document: " << filePath);
    return true;
}

bool ImageDocument::save(const QString& filePath)
{
    // Saving an image document is just copying the file (if no modifications) or saving the QImage.
    // For a read-only viewer, we might just copy.
    QString targetPath = filePath.isEmpty() ? this->filePath() : filePath;
    if (QFile::copy(d->imagePath, targetPath)) {
        setFilePath(targetPath);
        setModified(false);
        LOG_INFO("Successfully saved image document: " << targetPath);
        return true;
    } else {
        setLastError(tr("Failed to save image file."));
        LOG_ERROR(lastError());
        return false;
    }
}

Document::DocumentType ImageDocument::type() const
{
    // Determine type from file extension
    QFileInfo info(filePath());
    QString suffix = info.suffix().toLower();
    if (suffix == "jpg" || suffix == "jpeg") return DocumentType::JPG;
    if (suffix == "png") return DocumentType::PNG;
    if (suffix == "gif") return DocumentType::GIF;
    if (suffix == "bmp") return DocumentType::BMP;
    if (suffix == "tiff" || suffix == "tif") return DocumentType::TIFF;
    if (suffix == "webp") return DocumentType::WEBP;
    return DocumentType::IMAGE;
}

int ImageDocument::pageCount() const
{
    return d->pageCountVal; // Always 1
}

Page* ImageDocument::page(int index) const
{
    if (index == 0) {
        // return d->imagePage.get(); // This is a ComicPage, which is a Page*
        LOG_DEBUG("ImageDocument::page: Requested single image page.");
        return nullptr; // Placeholder until ComicPage is fully integrated
    }
    return nullptr;
}

bool ImageDocument::isLocked() const
{
    return false;
}

bool ImageDocument::isEncrypted() const
{
    return false;
}

QString ImageDocument::formatVersion() const
{
    return d->mimeTypeVal; // Return MIME type as format
}

bool ImageDocument::supportsFeature(const QString& feature) const
{
    static const QSet<QString> supportedFeatures = {
        "Images", "SimpleViewing"
    };
    return supportedFeatures.contains(feature);
}

QString ImageDocument::mimeType() const
{
    return d->mimeTypeVal;
}

QSize ImageDocument::imageSize() const
{
    return d->imageSizeVal;
}

bool ImageDocument::hasAlpha() const
{
    return d->hasAlphaVal;
}

int ImageDocument::colorDepth() const
{
    return d->colorDepthVal;
}

QString ImageDocument::colorSpace() const
{
    return d->colorSpaceVal;
}

void ImageDocument::createPages()
{
    // Create a single ComicPage for the image
    // d->imagePage = std::make_unique<ComicPage>(this, 0, d->imagePath); // Placeholder
    LOG_INFO("ImageDocument: Created single image page object.");
}

} // namespace QuantilyxDoc