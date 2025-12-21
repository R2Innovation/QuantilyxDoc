/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "DocxDocument.h"
#include "DocxPage.h"
#include "../../core/Logger.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QXmlStreamReader>
#include <QZipReader> // Qt 5.12+
#include <QDebug>

namespace QuantilyxDoc {

class DocxDocument::Private {
public:
    Private() : isLoaded(false), pageCountVal(0), hasTrackChangesVal(false) {}
    ~Private() = default;

    bool isLoaded;
    int pageCountVal;
    QString title;
    QString author;
    QList<QString> keywords;
    QStringList styles;
    QList<QString> embeddedObjects;
    bool hasTrackChangesVal;
    QList<std::unique_ptr<DocxPage>> pages;
    QZipReader zipReader;

    // Helper to parse document.xml and core properties from DOCX
    bool parseDocxContent() {
        // DOCX structure: [Content_Types].xml, _rels/.rels, word/document.xml, word/_rels/document.xml.rels, etc.
        // Core properties are in docProps/core.xml

        QByteArray coreXml = zipReader.fileData("docProps/core.xml");
        if (!coreXml.isEmpty()) {
            QXmlStreamReader coreReader(coreXml);
            while (!coreReader.atEnd()) {
                coreReader.readNext();
                if (coreReader.isStartElement()) {
                    QString tagName = coreReader.name().toString();
                    if (tagName == "title") {
                        title = coreReader.readElementText();
                    } else if (tagName == "creator") {
                        author = coreReader.readElementText();
                    } else if (tagName == "keywords") {
                        QString kwStr = coreReader.readElementText();
                        keywords = kwStr.split(',', Qt::SkipEmptyParts);
                    }
                }
            }
        }

        // Parse word/document.xml to estimate page count or check for track changes
        QByteArray documentXml = zipReader.fileData("word/document.xml");
        if (!documentXml.isEmpty()) {
            QXmlStreamReader docReader(documentXml);
            int paraCount = 0;
            while (!docReader.atEnd()) {
                docReader.readNext();
                if (docReader.isStartElement()) {
                    QString tagName = docReader.name().toString();
                    if (tagName == "p") {
                        paraCount++;
                    }
                    // Check for track changes elements like <w:ins>, <w:del>
                    if (tagName == "ins" || tagName == "del") {
                        hasTrackChangesVal = true;
                    }
                }
            }
            pageCountVal = qMax(1, paraCount / 20); // Rough estimate for Word documents
        } else {
            pageCountVal = 1;
        }

        LOG_DEBUG("DocxDocument: Parsed DOCX with title '" << title << "', author '" << author << "', estimated pages: " << pageCountVal << ", Track Changes: " << hasTrackChangesVal);
        return true;
    }
};

DocxDocument::DocxDocument(QObject* parent)
    : Document(parent)
    , d(new Private())
{
    LOG_INFO("DocxDocument created.");
}

DocxDocument::~DocxDocument()
{
    LOG_INFO("DocxDocument destroyed.");
}

bool DocxDocument::load(const QString& filePath, const QString& password)
{
    Q_UNUSED(password);
    d->isLoaded = false;
    d->pages.clear();

    d->zipReader.setFileName(filePath);
    if (!d->zipReader.exists() || !d->zipReader.isOpen()) {
        setLastError(tr("Failed to open DOCX file as ZIP archive."));
        LOG_ERROR(lastError());
        return false;
    }

    setFilePath(filePath);

    if (!d->parseDocxContent()) {
        setLastError(tr("Failed to parse DOCX content."));
        LOG_ERROR(lastError());
        return false;
    }

    createPages();

    d->isLoaded = true;
    setState(Loaded);
    emit docxLoaded();
    LOG_INFO("Successfully loaded DOCX document: " << filePath);
    return true;
}

bool DocxDocument::save(const QString& filePath)
{
    LOG_WARN("DocxDocument::save: Saving DOCX is not implemented.");
    Q_UNUSED(filePath);
    setLastError(tr("Saving DOCX documents is not supported."));
    return false;
}

// ... (Rest of DocxDocument methods similar to others) ...

void DocxDocument::createPages()
{
    d->pages.clear();
    d->pages.reserve(d->pageCountVal);
    for (int i = 0; i < d->pageCountVal; ++i) {
        // pages.append(std::make_unique<DocxPage>(this, i)); // Placeholder
        LOG_DEBUG("DocxDocument: Planned page " << i);
    }
    LOG_INFO("DocxDocument: Created " << d->pages.size() << " page objects.");
}

} // namespace QuantilyxDoc