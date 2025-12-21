/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "PsPage.h"
#include "PsDocument.h"
#include "../../core/Logger.h"
#include <QImage>
#include <QImageReader>
#include <QTemporaryFile>
#include <QDir>
#include <QProcess>
#include <QStandardPaths>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QDebug>

namespace QuantilyxDoc {

class PsPage::Private {
public:
    Private(PsDocument* doc, int pIndex)
        : document(doc), pageIndexVal(pIndex) {}

    PsDocument* document;
    int pageIndexVal;
    QRectF pageBBox;
    mutable QHash<QString, QImage> renderCache; // Cache images by render parameters

    // Helper to find the Ghostscript executable
    QString findGhostscriptExecutable() const {
        // Check common locations and PATH
        QStringList possibleNames = {
    #ifdef Q_OS_WIN
            "gswin64c.exe", "gswin32c.exe", "gs.exe"
    #else
            "gs"
    #endif
        };

        for (const QString& name : possibleNames) {
            QString fullPath = QStandardPaths::findExecutable(name);
            if (!fullPath.isEmpty()) {
                return fullPath;
            }
        }

        // If not found in PATH, check common installation directories
    #ifdef Q_OS_WIN
        QStringList commonPaths = {
            "C:/Program Files/gs/gs*/bin/gswin64c.exe",
            "C:/Program Files (x86)/gs/gs*/bin/gswin32c.exe",
            "C:/Program Files/ghostscript/gs*/bin/gswin64c.exe"
        };
        for (const QString& pattern : commonPaths) {
            // Use QDir to find matching directories with wildcards
            QFileInfo fileInfo(pattern);
            QString dirPath = fileInfo.dir().absolutePath();
            QString fileNamePattern = fileInfo.fileName();
            QDir dir(dirPath);
            QStringList matchingDirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QString& dirName : matchingDirs) {
                if (QDir(dirPath + "/" + dirName).exists()) {
                    QString exePath = dirPath + "/" + dirName + "/bin/" + fileNamePattern.replace("*", "");
                    if (QFile::exists(exePath)) {
                        return exePath;
                    }
                }
            }
        }
    #endif

        return QString(); // Not found
    }
};

PsPage::PsPage(PsDocument* document, int pageIndex, QObject* parent)
    : Page(document, parent)
    , d(new Private(document, pageIndex))
{
    LOG_DEBUG("PsPage created for index " << pageIndex);
}

PsPage::~PsPage()
{
    LOG_DEBUG("PsPage for index " << d->pageIndexVal << " destroyed.");
}

QImage PsPage::render(int width, int height, int dpi)
{
    // Create a cache key based on render parameters
    QString cacheKey = QString("%1x%2@%3dpi").arg(width).arg(height).arg(dpi);
    if (d->renderCache.contains(cacheKey)) {
        LOG_DEBUG("PsPage::render: Using cached image for " << cacheKey);
        return d->renderCache[cacheKey];
    }

    // Render using Ghostscript
    QImage image = renderWithGhostscript(width, height, dpi);
    if (!image.isNull()) {
        d->renderCache[cacheKey] = image; // Cache successful render
    }
    return image;
}

// ... (other PsPage methods can return placeholders for now) ...

QRectF PsPage::pageBoundingBox() const
{
    // For PS, the page bounding box is often the same as the document's,
    // or needs to be extracted per-page using Ghostscript's bbox device.
    // This is complex. For now, return the document's bounding box.
    // return d->document->documentBoundingBox(); // PsDocument method
    return QRectF(0, 0, 612, 792); // Default US Letter size in points as placeholder
}

QImage PsPage::renderWithGhostscript(int width, int height, int dpi)
{
    if (!d->document) {
        LOG_ERROR("PsPage::renderWithGhostscript: No parent document.");
        return QImage();
    }

    QString psFilePath = d->document->filePath();
    if (psFilePath.isEmpty()) {
        LOG_ERROR("PsPage::renderWithGhostscript: Parent document has no file path.");
        return QImage();
    }

    // Find Ghostscript executable
    QString gsPath = d->findGhostscriptExecutable();
    if (gsPath.isEmpty()) {
        LOG_ERROR("PsPage::renderWithGhostscript: Ghostscript executable not found. Please install Ghostscript.");
        return QImage();
    }

    // Create a temporary file for the output image (e.g., PNG)
    QTemporaryFile tempOutput;
    tempOutput.setFileTemplate(QDir::tempPath() + "/quantilyx_ps_render_XXXXXX.png");
    if (!tempOutput.open()) {
        LOG_ERROR("PsPage::renderWithGhostscript: Failed to create temporary output file.");
        return QImage();
    }
    QString outputPath = tempOutput.fileName();
    tempOutput.close(); // Close so Ghostscript can write to it

    // Build Ghostscript command arguments
    // Example: gs -dNOPAUSE -dBATCH -sDEVICE=png16m -r300 -g1024x768 -sOutputFile=output.png -f input.ps
    QStringList args;
    args << "-dNOPAUSE"
         << "-dBATCH"
         << "-dSAFER" // Security option
         << "-sDEVICE=png16m" // 32-bit RGBA PNG
         << QString("-r%1").arg(dpi) // Resolution
         << QString("-g%1x%2").arg(width).arg(height) // Output geometry (width x height in pixels)
         << QString("-sOutputFile=%1").arg(outputPath)
         << "-dFirstPage=%1".arg(d->pageIndexVal + 1) // Ghostscript pages are 1-based
         << "-dLastPage=%1".arg(d->pageIndexVal + 1)
         << "-f" << psFilePath;

    LOG_DEBUG("PsPage::renderWithGhostscript: Executing: " << gsPath << " " << args.join(" "));

    // Run Ghostscript process
    QProcess gsProcess;
    gsProcess.start(gsPath, args);
    bool finished = gsProcess.waitForFinished(-1); // Wait indefinitely, or set a timeout

    if (!finished) {
        LOG_ERROR("PsPage::renderWithGhostscript: Ghostscript process did not finish.");
        return QImage();
    }

    if (gsProcess.exitCode() != 0) {
        QString errorOutput = gsProcess.readAllStandardError();
        LOG_ERROR("PsPage::renderWithGhostscript: Ghostscript failed with exit code " << gsProcess.exitCode() << ". Error: " << errorOutput);
        return QImage();
    }

    // Check if output file was created
    if (!QFile::exists(outputPath)) {
        LOG_ERROR("PsPage::renderWithGhostscript: Ghostscript did not create output file: " << outputPath);
        return QImage();
    }

    // Load the rendered image
    QImage image(outputPath);
    if (image.isNull()) {
        LOG_ERROR("PsPage::renderWithGhostscript: Failed to load rendered image from: " << outputPath);
        QFile::remove(outputPath); // Clean up
        return QImage();
    }

    LOG_DEBUG("PsPage::renderWithGhostscript: Successfully rendered page " << d->pageIndexVal << " to " << outputPath);
    QFile::remove(outputPath); // Clean up temporary file
    return image;
}

} // namespace QuantilyxDoc