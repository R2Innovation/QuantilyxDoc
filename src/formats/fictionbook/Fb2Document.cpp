/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "Fb2Document.h"
#include "Fb2Page.h"
#include "../../core/Logger.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QXmlStreamReader>
#include <QBuffer>
#include <QBase64Decoder>
#include <QDebug>

namespace QuantilyxDoc {

class Fb2Document::Private {
public:
    Private() : isLoaded(false), pageCountVal(0) {}
    ~Private() = default;

    bool isLoaded;
    int pageCountVal;
    QString title;
    QStringList authors;
    QString genre;
    QString bookId;
    QMap<QString, QByteArray> embeddedImages; // ID -> image data
    QList<std::unique_ptr<Fb2Page>> pages;
    QString fb2Content;

    // Helper to parse the FB2 XML structure
    bool parseFb2Xml(const QByteArray& xmlData) {
        QXmlStreamReader xmlReader(xmlData);
        QString currentTag;

        while (!xmlReader.atEnd()) {
            xmlReader.readNext();

            if (xmlReader.isStartElement()) {
                QString tagName = xmlReader.name().toString();
                currentTag = tagName;

                if (tagName == "title") {
                    // Parse book title from title section
                    title = xmlReader.readElementText();
                } else if (tagName == "author") {
                    // Parse author(s)
                    QString author;
                    while (!(xmlReader.isEndElement() && xmlReader.name() == "author") && !xmlReader.atEnd()) {
                        xmlReader.readNext();
                        if (xmlReader.isStartElement()) {
                            author += xmlReader.readElementText() + " ";
                        }
                    }
                    if (!author.trimmed().isEmpty()) {
                        authors.append(author.trimmed());
                    }
                } else if (tagName == "genre") {
                    genre = xmlReader.readElementText();
                } else if (tagName == "id") {
                    bookId = xmlReader.readElementText();
                } else if (tagName == "section") {
                    // Count sections as pages
                    pageCountVal++;
                } else if (tagName == "binary") {
                    // Parse embedded images
                    QXmlStreamAttributes attrs = xmlReader.attributes();
                    QString imageId = attrs.value("id").toString();
                    if (!imageId.isEmpty()) {
                        QString imageDataB64 = xmlReader.readElementText().trimmed();
                        QByteArray imageData = QByteArray::fromBase64(imageDataB64.toUtf8());
                        embeddedImages.insert(imageId, imageData);
                        LOG_DEBUG("Fb2Document: Parsed embedded image: " << imageId);
                    }
                }
            }
        }

        if (xmlReader.hasError()) {
            LOG_ERROR("Fb2Document: XML parsing error: " << xmlReader.errorString());
            return false;
        }

        LOG_DEBUG("Fb2Document: Parsed FB2 with title '" << title << "', " << authors.size() << " authors, " << pageCountVal << " sections.");
        return true;
    }
};

Fb2Document::Fb2Document(QObject* parent)
    : Document(parent)
    , d(new Private())
{
    LOG_INFO("Fb2Document created.");
}

Fb2Document::~Fb2Document()
{
    LOG_INFO("Fb2Document destroyed.");
}

bool Fb2Document::load(const QString& filePath, const QString& password)
{
    Q_UNUSED(password);
    d->isLoaded = false;
    d->pages.clear();

    QFile fb2File(filePath);
    if (!fb2File.open(QIODevice::ReadOnly)) {
        setLastError(tr("Failed to open FB2 file."));
        LOG_ERROR(lastError());
        return false;
    }

    QByteArray fb2Data = fb2File.readAll();
    fb2File.close();

    // Parse the FB2 XML
    if (!d->parseFb2Xml(fb2Data)) {
        setLastError(tr("Failed to parse FB2 XML structure."));
        LOG_ERROR(lastError());
        return false;
    }

    setFilePath(filePath);
    d->fb2Content = QString::fromUtf8(fb2Data); // Store for potential rendering/save
    createPages();

    d->isLoaded = true;
    setState(Loaded);
    emit fb2Loaded();
    LOG_INFO("Successfully loaded FB2 document: " << filePath);
    return true;
}

bool Fb2Document::save(const QString& filePath)
{
    // Saving FB2 involves writing the XML back to a file, potentially with modifications.
    QFile outputFile(filePath.isEmpty() ? this->filePath() : filePath);
    if (!outputFile.open(QIODevice::WriteOnly)) {
        setLastError(tr("Failed to save FB2 file."));
        LOG_ERROR(lastError());
        return false;
    }

    outputFile.write(d->fb2Content.toUtf8());
    outputFile.close();
    setFilePath(filePath.isEmpty() ? this->filePath() : filePath);
    setModified(false);
    LOG_INFO("Successfully saved FB2 document: " << outputFile.fileName());
    return true;
}

// ... (Rest of Fb2Document methods similar to others) ...

void Fb2Document::createPages()
{
    // Create one page per 'section' element in the FB2
    d->pages.clear();
    d->pages.reserve(d->pageCountVal);
    for (int i = 0; i < d->pageCountVal; ++i) {
        // pages.append(std::make_unique<Fb2Page>(this, i)); // Placeholder
        LOG_DEBUG("Fb2Document: Planned page " << i);
    }
    LOG_INFO("Fb2Document: Created " << d->pages.size() << " page objects.");
}

} // namespace QuantilyxDoc