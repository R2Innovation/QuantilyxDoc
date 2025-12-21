/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "XpsDocument.h"
#include "XpsPage.h"
#include "../../core/Logger.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QXmlStreamReader>
#include <QZipReader> // Requires Qt5::Gui or Qt5::Core (Qt 5.12+)
#include <QDebug>

namespace QuantilyxDoc {

class XpsDocument::Private {
public:
    Private() : isLoaded(false), pageCountVal(0) {}
    ~Private() = default;

    bool isLoaded;
    int pageCountVal;
    QString title;
    QString author;
    QList<QString> keywords;
    bool hasSignatureVal = false;
    QList<std::unique_ptr<XpsPage>> pages;
    QZipReader zipReader; // Qt's built-in ZIP reader

    // Helper to parse the FixedDocumentSequence.fdseq file to get document structure and page count
    bool parseFixedDocSequence() {
        // XPS structure: ROOT/_rels/.rels -> FixedDocumentSequence.fdseq -> FixedDocument.fdoc -> FixedPage.fpage
        // Also contains Documents/X/X.fdoc, Pages/X-Y/X-Y.fpage, etc.

        QByteArray fdseqData = zipReader.fileData("_rels/.rels");
        if (fdseqData.isEmpty()) {
            LOG_ERROR("XpsDocument: Could not find _rels/.rels in XPS archive.");
            return false;
        }

        // Parse _rels/.rels to find FixedDocumentSequence.fdseq
        QXmlStreamReader relsReader(fdseqData);
        QString fdseqPath;
        while (!relsReader.atEnd()) {
            relsReader.readNext();
            if (relsReader.isStartElement() && relsReader.name() == "Relationship") {
                QXmlStreamAttributes attrs = relsReader.attributes();
                if (attrs.value("Type").toString().endsWith("/document") && attrs.value("Target").toString().endsWith(".fdseq")) {
                    fdseqPath = attrs.value("Target").toString();
                    break;
                }
            }
        }

        if (fdseqPath.isEmpty()) {
            LOG_ERROR("XpsDocument: Could not find FixedDocumentSequence in _rels/.rels.");
            return false;
        }

        QByteArray fdseqContent = zipReader.fileData(fdseqPath);
        if (fdseqContent.isEmpty()) {
            LOG_ERROR("XpsDocument: Could not read " << fdseqPath);
            return false;
        }

        // Parse FixedDocumentSequence.fdseq to find FixedDocument.fdoc files
        QXmlStreamReader fdseqReader(fdseqContent);
        QStringList fdocPaths;
        while (!fdseqReader.atEnd()) {
            fdseqReader.readNext();
            if (fdseqReader.isStartElement() && fdseqReader.name() == "DocumentReference") {
                QXmlStreamAttributes attrs = fdseqReader.attributes();
                QString fdocPath = attrs.value("Source").toString();
                if (!fdocPath.isEmpty()) {
                    fdocPaths.append(fdocPath);
                }
            }
        }

        if (fdocPaths.isEmpty()) {
            LOG_ERROR("XpsDocument: No FixedDocument found in " << fdseqPath);
            return false;
        }

        // Parse each FixedDocument.fdoc to count pages (each PageContent entry is a page)
        pageCountVal = 0;
        for (const QString& fdocPath : fdocPaths) {
            QByteArray fdocContent = zipReader.fileData(fdocPath);
            if (fdocContent.isEmpty()) {
                LOG_WARN("XpsDocument: Could not read " << fdocPath);
                continue;
            }

            QXmlStreamReader fdocReader(fdocContent);
            while (!fdocReader.atEnd()) {
                fdocReader.readNext();
                if (fdocReader.isStartElement() && fdocReader.name() == "PageContent") {
                    pageCountVal++;
                }
            }
        }

        // Parse DocumentMetadata or CoreProperties for title/author if available
        // This is complex and involves parsing OPC (Open Packaging Conventions) properties.
        // For now, skip.

        LOG_DEBUG("XpsDocument: Parsed " << fdocPaths.size() << " FixedDocuments with " << pageCountVal << " total pages.");
        return true;
    }
};

XpsDocument::XpsDocument(QObject* parent)
    : Document(parent)
    , d(new Private())
{
    LOG_INFO("XpsDocument created.");
}

XpsDocument::~XpsDocument()
{
    LOG_INFO("XpsDocument destroyed.");
}

bool XpsDocument::load(const QString& filePath, const QString& password)
{
    Q_UNUSED(password);
    d->isLoaded = false;
    d->pages.clear();

    // Open XPS as ZIP archive using Qt's QZipReader
    d->zipReader.setFileName(filePath);
    if (!d->zipReader.exists() || !d->zipReader.isOpen()) {
        setLastError(tr("Failed to open XPS file as ZIP archive."));
        LOG_ERROR(lastError());
        return false;
    }

    setFilePath(filePath);

    // Parse document structure to get page count and metadata
    if (!d->parseFixedDocSequence()) {
        setLastError(tr("Failed to parse XPS document structure."));
        LOG_ERROR(lastError());
        return false;
    }

    // Create page objects
    createPages();

    d->isLoaded = true;
    setState(Loaded);
    emit xpsLoaded();
    LOG_INFO("Successfully loaded XPS document: " << filePath << " (Pages: " << pageCount() << ")");
    return true;
}

bool XpsDocument::save(const QString& filePath)
{
    LOG_WARN("XpsDocument::save: Saving XPS is not implemented.");
    Q_UNUSED(filePath);
    setLastError(tr("Saving XPS documents is not supported."));
    return false;
}

Document::DocumentType XpsDocument::type() const
{
    return DocumentType::XPS;
}

int XpsDocument::pageCount() const
{
    return d->pageCountVal;
}

Page* XpsDocument::page(int index) const
{
    if (index >= 0 && index < d->pages.size()) {
         // return d->pages[index].get(); // Placeholder
         LOG_DEBUG("XpsDocument::page: Planned page " << index);
         return nullptr;
    }
    return nullptr;
}

bool XpsDocument::isLocked() const
{
    return false; // XPS can have security but not commonly file-locked
}

bool XpsDocument::isEncrypted() const
{
    return false;
}

QString XpsDocument::formatVersion() const
{
    return "XPS/OPC"; // Open Packaging Conventions
}

bool XpsDocument::supportsFeature(const QString& feature) const
{
    static const QSet<QString> supportedFeatures = {
        "VectorGraphics", "Text", "FixedLayout", "Hyperlinks", "EmbeddedFonts"
    };
    return supportedFeatures.contains(feature);
}

QString XpsDocument::documentTitle() const
{
    return d->title; // Might be empty if not parsed
}

QString XpsDocument::documentAuthor() const
{
    return d->author;
}

QList<QString> XpsDocument::documentKeywords() const
{
    return d->keywords;
}

bool XpsDocument::hasDigitalSignature() const
{
    return d->hasSignatureVal;
}

void XpsDocument::createPages()
{
    d->pages.clear();
    d->pages.reserve(d->pageCountVal);
    for (int i = 0; i < d->pageCountVal; ++i) {
        // pages.append(std::make_unique<XpsPage>(this, i)); // Placeholder
        LOG_DEBUG("XpsDocument: Planned page " << i);
    }
    LOG_INFO("XpsDocument: Created " << d->pages.size() << " page objects.");
}

} // namespace QuantilyxDoc