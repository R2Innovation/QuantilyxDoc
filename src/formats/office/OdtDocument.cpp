/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "OdtDocument.h"
#include "OdtPage.h"
#include "../../core/Logger.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QXmlStreamReader>
#include <QZipReader> // Qt 5.12+
#include <QDebug>

namespace QuantilyxDoc {

class OdtDocument::Private {
public:
    Private() : isLoaded(false), pageCountVal(0) {}
    ~Private() = default;

    bool isLoaded;
    int pageCountVal;
    QString title;
    QString author;
    QList<QString> keywords;
    QStringList styles;
    QList<QString> embeddedObjects;
    QList<std::unique_ptr<OdtPage>> pages;
    QZipReader zipReader;

    // Helper to parse content.xml and meta.xml from ODT
    bool parseOdtContent() {
        // ODT structure: META-INF/manifest.xml, content.xml, meta.xml, styles.xml, etc.
        QByteArray metaXml = zipReader.fileData("meta.xml");
        if (!metaXml.isEmpty()) {
            QXmlStreamReader metaReader(metaXml);
            while (!metaReader.atEnd()) {
                metaReader.readNext();
                if (metaReader.isStartElement()) {
                    QString tagName = metaReader.name().toString();
                    if (tagName == "title") {
                        title = metaReader.readElementText();
                    } else if (tagName == "creator") {
                        author = metaReader.readElementText();
                    } else if (tagName == "keywords") {
                        QString kwStr = metaReader.readElementText();
                        keywords = kwStr.split(',', Qt::SkipEmptyParts);
                    }
                }
            }
        }

        // Parse content.xml to estimate page count or get structure
        QByteArray contentXml = zipReader.fileData("content.xml");
        if (!contentXml.isEmpty()) {
            // Count sections, paragraphs, or use other heuristics for page count.
            // A real implementation would be more complex.
            QXmlStreamReader contentReader(contentXml);
            int paraCount = 0;
            while (!contentReader.atEnd()) {
                contentReader.readNext();
                if (contentReader.isStartElement() && contentReader.name() == "p") {
                    paraCount++;
                }
            }
            pageCountVal = qMax(1, paraCount / 25); // Rough estimate: 25 paragraphs per page
        } else {
            pageCountVal = 1;
        }

        LOG_DEBUG("OdtDocument: Parsed ODT with title '" << title << "', author '" << author << "', estimated pages: " << pageCountVal);
        return true;
    }
};

OdtDocument::OdtDocument(QObject* parent)
    : Document(parent)
    , d(new Private())
{
    LOG_INFO("OdtDocument created.");
}

OdtDocument::~OdtDocument()
{
    LOG_INFO("OdtDocument destroyed.");
}

bool OdtDocument::load(const QString& filePath, const QString& password)
{
    Q_UNUSED(password);
    d->isLoaded = false;
    d->pages.clear();

    d->zipReader.setFileName(filePath);
    if (!d->zipReader.exists() || !d->zipReader.isOpen()) {
        setLastError(tr("Failed to open ODT file as ZIP archive."));
        LOG_ERROR(lastError());
        return false;
    }

    setFilePath(filePath);

    if (!d->parseOdtContent()) {
        setLastError(tr("Failed to parse ODT content."));
        LOG_ERROR(lastError());
        return false;
    }

    createPages();

    d->isLoaded = true;
    setState(Loaded);
    emit odtLoaded();
    LOG_INFO("Successfully loaded ODT document: " << filePath);
    return true;
}

bool OdtDocument::save(const QString& filePath)
{
    LOG_WARN("OdtDocument::save: Saving ODT is not implemented.");
    Q_UNUSED(filePath);
    setLastError(tr("Saving ODT documents is not supported."));
    return false;
}

// ... (Rest of OdtDocument methods similar to others) ...

void OdtDocument::createPages()
{
    d->pages.clear();
    d->pages.reserve(d->pageCountVal);
    for (int i = 0; i < d->pageCountVal; ++i) {
        // pages.append(std::make_unique<OdtPage>(this, i)); // Placeholder
        LOG_DEBUG("OdtDocument: Planned page " << i);
    }
    LOG_INFO("OdtDocument: Created " << d->pages.size() << " page objects.");
}

} // namespace QuantilyxDoc