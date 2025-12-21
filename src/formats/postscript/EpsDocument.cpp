/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "EpsDocument.h"
#include "../../core/Logger.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <QImage>
#include <QBuffer>
#include <QDebug>

namespace QuantilyxDoc {

class EpsDocument::Private {
public:
    Private() : hasPreview(false) {}
    bool hasPreview;
    QImage previewImg;
    // Add any EPS-specific data members if needed
};

EpsDocument::EpsDocument(QObject* parent)
    : PsDocument(parent) // Inherit from PsDocument
    , d(new Private())
{
    LOG_INFO("EpsDocument created.");
}

EpsDocument::~EpsDocument()
{
    LOG_INFO("EpsDocument destroyed.");
}

Document::DocumentType EpsDocument::type() const
{
    return DocumentType::EPS;
}

bool EpsDocument::hasPreviewImage() const
{
    return d->hasPreview;
}

QImage EpsDocument::previewImage() const
{
    return d->previewImg;
}

// Override PsDocument::load if specific EPS logic is needed *during* loading
// Otherwise, let PsDocument::load handle the PS part, and add EPS-specific parsing afterwards.
// For now, assume PsDocument::load succeeds, then parse EPS specifics.
// We could override the load method to call the parent, then parse EPS.
bool EpsDocument::load(const QString& filePath, const QString& password)
{
    // First, let the parent PsDocument class load the PS content
    bool psLoadSuccess = PsDocument::load(filePath, password); // Call parent's load
    if (!psLoadSuccess) {
        // PsDocument::load will have set lastError, just propagate
        return false;
    }

    // If PS loading was successful, parse EPS-specific elements
    parseEpsSpecificElements();

    emit epsLoaded(); // Emit EPS-specific signal
    LOG_INFO("Successfully loaded EPS document: " << filePath << " (Has Preview: " << d->hasPreview << ")");
    return true;
}

void EpsDocument::parseEpsSpecificElements()
{
    // EPS files sometimes contain a preview image in a structured comment block like:
    // %%BeginPreview: <width> <height> <num_components> <bits_per_component>
    // ... binary or hex-encoded image data ...
    // %%EndPreview
    // Parsing this requires careful text/binary handling within the PS file.

    QFile epsFile(filePath());
    if (!epsFile.open(QIODevice::ReadOnly)) {
        LOG_ERROR("EpsDocument: Failed to open EPS file for preview parsing: " << filePath());
        return;
    }

    QTextStream stream(&epsFile);
    QString line;
    bool inPreviewSection = false;
    QStringList previewHeaderParams;
    QByteArray previewData;

    while (!stream.atEnd()) {
        line = stream.readLine().trimmed();

        if (line.startsWith("%%BeginPreview:")) {
            // Extract width, height, components, bits from the line
            QStringList parts = line.split(' ', Qt::SkipEmptyParts);
            if (parts.size() >= 5) { // %%BeginPreview: width height components bits ...
                previewHeaderParams = parts.mid(1, 4); // Get the next 4 numbers
                inPreviewSection = true;
                LOG_DEBUG("EpsDocument: Found BeginPreview section, expecting params: " << previewHeaderParams.join(", "));
            }
        } else if (line.startsWith("%%EndPreview")) {
            inPreviewSection = false;
            // Now try to decode the previewData based on the header params
            if (!previewHeaderParams.isEmpty() && !previewData.isEmpty()) {
                int width = previewHeaderParams[0].toInt();
                int height = previewHeaderParams[1].toInt();
                int components = previewHeaderParams[2].toInt(); // e.g., 3 for RGB
                int bitsPerComp = previewHeaderParams[3].toInt(); // e.g., 8

                // Decode the binary/hex data into a QImage.
                // This is complex and depends on the encoding (hex, base64, raw binary) and color format (RGB, CMYK, etc.).
                // A full implementation would need to handle various encoding schemes and color spaces.
                // For now, let's assume it's a simple raw RGB format (which is often not the case in EPS previews).
                // A real implementation might need a library or complex bit manipulation.

                // Placeholder: Assume raw RGB data (which is often not correct)
                if (components == 3 && bitsPerComp == 8) {
                    QImage preview(width, height, QImage::Format_RGB888);
                    if (previewData.size() == width * height * 3) {
                        memcpy(preview.bits(), previewData.constData(), previewData.size());
                        d->previewImg = preview.rgbSwapped(); // Qt uses BGR internally for RGB888 sometimes, swap if needed
                        d->hasPreview = true;
                        LOG_INFO("EpsDocument: Parsed simple RGB preview image from EPS.");
                    } else {
                        LOG_WARN("EpsDocument: Preview data size does not match expected RGB size.");
                    }
                } else {
                    LOG_WARN("EpsDocument: Unsupported preview format (components: " << components << ", bits: " << bitsPerComp << "). Cannot decode.");
                }
            }
            previewHeaderParams.clear();
            previewData.clear();
        } else if (inPreviewSection) {
            // Accumulate preview data. It might be hex-encoded (%02X per byte) or binary.
            // Hex decoding is common.
            // Example: "FF D8 FF E0 00 10 4A 46 49 46..." -> binary data
            QString hexLine = line;
            hexLine.remove(QRegularExpression("\\s+")); // Remove whitespace
            bool ok;
            for (int i = 0; i < hexLine.size(); i += 2) {
                QString byteStr = hexLine.mid(i, 2);
                quint8 byte = byteStr.toUInt(&ok, 16);
                if (ok) {
                    previewData.append(static_cast<char>(byte));
                } else {
                    LOG_WARN("EpsDocument: Invalid hex byte in preview: " << byteStr);
                }
            }
        }
    }

    if (!d->hasPreview) {
        LOG_DEBUG("EpsDocument: No preview image found in EPS file.");
    }
}

} // namespace QuantilyxDoc