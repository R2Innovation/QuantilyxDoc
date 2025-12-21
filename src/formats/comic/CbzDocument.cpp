/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "CbzDocument.h"
#include "ComicPage.h" // Assuming this handles image-based pages
#include "../../core/Logger.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>
#include <QTextStream>
#include <QDebug>
#include <zip.h> // Use libzip for CBZ (which is ZIP)

namespace QuantilyxDoc {

class CbzDocument::Private {
public:
    Private() : zipArchive(nullptr), isLoaded(false) {}
    ~Private() {
        if (zipArchive) {
            zip_close(zipArchive);
        }
    }

    zip_t* zipArchive;
    bool isLoaded;
    QStringList imagePathsList;
    QStringList otherFilesList;
    QString comicInfoContent;
    QList<std::unique_ptr<ComicPage>> pages; // Own the page objects

    // Helper to read a file from the ZIP archive
    QByteArray readFileFromZip(const QString& filePath) const {
        if (!zipArchive) return QByteArray();

        QString pathInZip = filePath;
        if (pathInZip.startsWith("/")) pathInZip.remove(0, 1);

        zip_stat_t stat;
        int result = zip_stat(zipArchive, pathInZip.toUtf8().constData(), 0, &stat);
        if (result < 0) {
            LOG_ERROR("CbzDocument: Failed to stat file in archive: " << filePath);
            return QByteArray();
        }

        zip_file_t* file = zip_fopen(zipArchive, pathInZip.toUtf8().constData(), 0);
        if (!file) {
            LOG_ERROR("CbzDocument: Failed to open file in archive: " << filePath);
            return QByteArray();
        }

        QByteArray data(stat.size, 0);
        zip_int64_t bytesRead = zip_fread(file, data.data(), stat.size);
        zip_fclose(file);

        if (bytesRead != static_cast<zip_int64_t>(stat.size)) {
            LOG_ERROR("CbzDocument: Failed to read full file content: " << filePath);
            return QByteArray();
        }

        return data;
    }

    // Helper to list all files in the archive and categorize them
    void listAndCategorizeFiles() {
        if (!zipArchive) return;

        int numFiles = zip_get_num_entries(zipArchive, 0);
        QRegularExpression imageRegex(R"(\.(jpg|jpeg|png|gif|webp|bmp|tiff|tif)$)", QRegularExpression::CaseInsensitiveOption);
        for (int i = 0; i < numFiles; ++i) {
            const char* fileNameC = zip_get_name(zipArchive, i, 0);
            if (fileNameC) {
                QString fileName = QString::fromUtf8(fileNameC);
                if (imageRegex.match(fileName).hasMatch()) {
                    imagePathsList.append(fileName);
                } else {
                    otherFilesList.append(fileName);
                }
            }
        }
        // Sort image paths to ensure correct page order (often relies on filename sorting)
        std::sort(imagePathsList.begin(), imagePathsList.end());
        LOG_DEBUG("CbzDocument: Found " << imagePathsList.size() << " image files and " << otherFilesList.size() << " other files.");
    }
};

CbzDocument::CbzDocument(QObject* parent)
    : Document(parent)
    , d(new Private())
{
    LOG_INFO("CbzDocument created.");
}

CbzDocument::~CbzDocument()
{
    LOG_INFO("CbzDocument destroyed.");
}

bool CbzDocument::load(const QString& filePath, const QString& password)
{
    Q_UNUSED(password); // CBZs typically don't use archive-level passwords

    // Close any previously loaded archive
    if (d->zipArchive) {
        zip_close(d->zipArchive);
        d->zipArchive = nullptr;
    }
    d->isLoaded = false;
    d->pages.clear();
    d->imagePathsList.clear();
    d->otherFilesList.clear();

    // Open the CBZ file as a ZIP archive
    int zipError;
    d->zipArchive = zip_open(filePath.toUtf8().constData(), ZIP_RDONLY, &zipError);
    if (!d->zipArchive) {
        char errorBuffer[256];
        zip_error_to_str(errorBuffer, sizeof(errorBuffer), zipError, errno);
        setLastError(tr("Failed to open CBZ file as ZIP archive: %1").arg(errorBuffer));
        LOG_ERROR(lastError());
        return false;
    }

    // Set file path and update file size
    setFilePath(filePath);

    // List and categorize files
    d->listAndCategorizeFiles();

    // Parse ComicInfo.xml if present
    if (d->otherFilesList.contains("ComicInfo.xml")) {
        parseComicInfo();
    }

    // Create ComicPage objects based on image list
    createPages();

    d->isLoaded = true;
    setState(Loaded);
    emit cbzLoaded(); // Emit specific signal for CBZ loading
    LOG_INFO("Successfully loaded CBZ document: " << filePath << " (Images: " << pageCount() << ", Other files: " << d->otherFilesList.size() << ")");
    return true;
}

bool CbzDocument::save(const QString& filePath)
{
    // Saving CBZ requires reconstructing the ZIP archive.
    // This is complex, especially if images/pages were modified.
    LOG_WARN("CbzDocument::save: Saving modified CBZs is not yet implemented.");
    Q_UNUSED(filePath);
    setLastError(tr("Saving modified CBZs is not yet supported."));
    return false;
}

Document::DocumentType CbzDocument::type() const
{
    return DocumentType::CBZ;
}

int CbzDocument::pageCount() const
{
    return d->imagePathsList.size(); // Number of images = number of pages
}

Page* CbzDocument::page(int index) const
{
    if (index >= 0 && index < d->pages.size()) {
        // Return raw pointer managed by unique_ptr.
        // return d->pages[index].get(); // Placeholder - requires ComicPage implementation
        LOG_DEBUG("CbzDocument::page: Requested page " << index << ", but ComicPage not yet implemented.");
        return nullptr; // For now, return null until ComicPage is ready
    }
    return nullptr;
}

bool CbzDocument::isLocked() const
{
    return false; // CBZ files are not locked
}

bool CbzDocument::isEncrypted() const
{
    return false; // CBZ files are not encrypted
}

QString CbzDocument::formatVersion() const
{
    // Could return the ZIP format version or a generic "CBZ" string
    return "ZIP (Comic Book Archive)";
}

bool CbzDocument::supportsFeature(const QString& feature) const
{
    static const QSet<QString> supportedFeatures = {
        "Images", "SequentialReading", "MetadataFile" // Basic CBZ features
    };
    return supportedFeatures.contains(feature);
}

// --- CBZ-Specific Getters ---
QStringList CbzDocument::imagePaths() const
{
    return d->imagePathsList;
}

QStringList CbzDocument::otherFiles() const
{
    return d->otherFilesList;
}

bool CbzDocument::hasComicInfo() const
{
    return !d->comicInfoContent.isEmpty();
}

QString CbzDocument::comicInfoXml() const
{
    return d->comicInfoContent;
}

bool CbzDocument::extractImage(const QString& imagePath, const QString& outputPath) const
{
    QByteArray imageData = d->readFileFromZip(imagePath);
    if (imageData.isEmpty()) {
        LOG_ERROR("CbzDocument::extractImage: Failed to read image data from archive: " << imagePath);
        return false;
    }

    QFile outputFile(outputPath);
    if (!outputFile.open(QIODevice::WriteOnly)) {
        LOG_ERROR("CbzDocument::extractImage: Failed to open output file: " << outputPath);
        return false;
    }

    bool writeSuccess = (outputFile.write(imageData) == imageData.size());
    outputFile.close();

    if (writeSuccess) {
        LOG_INFO("CbzDocument::extractImage: Extracted image to: " << outputPath);
    } else {
        LOG_ERROR("CbzDocument::extractImage: Failed to write image data to: " << outputPath);
    }
    return writeSuccess;
}

// --- Helpers ---
void CbzDocument::parseComicInfo()
{
    QByteArray xmlData = d->readFileFromZip("ComicInfo.xml");
    if (!xmlData.isEmpty()) {
        d->comicInfoContent = QString::fromUtf8(xmlData);
        LOG_DEBUG("CbzDocument: Parsed ComicInfo.xml");
    } else {
        LOG_WARN("CbzDocument: Failed to read ComicInfo.xml");
    }
}

void CbzDocument::createPages()
{
    d->pages.clear();
    d->pages.reserve(d->imagePathsList.size());
    for (int i = 0; i < d->imagePathsList.size(); ++i) {
        // Create the page object. This requires ComicPage to be implemented.
        // pages.append(std::make_unique<ComicPage>(this, i, d->imagePathsList[i])); // Placeholder
        LOG_DEBUG("CbzDocument: Planned page " << i << " from image: " << d->imagePathsList[i]);
    }
    LOG_INFO("CbzDocument: Created " << d->pages.size() << " page objects.");
}

} // namespace QuantilyxDoc