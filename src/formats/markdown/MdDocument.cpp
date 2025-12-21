/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "MdDocument.h"
#include "MdPage.h"
#include "../../core/Logger.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <QDebug>
// #include "markdown/parser.h" // Hypothetical Markdown parser library

namespace QuantilyxDoc {

class MdDocument::Private {
public:
    Private() : isLoaded(false) {}
    ~Private() = default;

    bool isLoaded;
    QString markdownContentVal;
    QString renderedHtmlCache;
    std::unique_ptr<MdPage> singlePage;

    // Helper to render Markdown to HTML
    QString renderMarkdownToHtml(const QString& markdown) const {
        // This requires a Markdown parser library like Hoedown, Discount, or QTextDocument's limited support.
        // QTextDocument doc;
        // doc.setMarkdown(markdown); // Qt 5.14+
        // return doc.toHtml();
        LOG_WARN("MdDocument::renderMarkdownToHtml: Requires Markdown parser library.");
        return "<html><body><p>Markdown rendering not available.</p></body></html>"; // Placeholder
    }
};

MdDocument::MdDocument(QObject* parent)
    : Document(parent)
    , d(new Private())
{
    LOG_INFO("MdDocument created.");
}

MdDocument::~MdDocument()
{
    LOG_INFO("MdDocument destroyed.");
}

bool MdDocument::load(const QString& filePath, const QString& password)
{
    Q_UNUSED(password);
    d->isLoaded = false;

    QFile mdFile(filePath);
    if (!mdFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setLastError(tr("Failed to open Markdown file."));
        LOG_ERROR(lastError());
        return false;
    }

    d->markdownContentVal = mdFile.readAll();
    mdFile.close();

    // Render to HTML cache
    d->renderedHtmlCache = d->renderMarkdownToHtml(d->markdownContentVal);

    setFilePath(filePath);
    createPages();

    d->isLoaded = true;
    setState(Loaded);
    emit mdLoaded();
    LOG_INFO("Successfully loaded Markdown document: " << filePath);
    return true;
}

bool MdDocument::save(const QString& filePath)
{
    // Saving Markdown is just writing the text content back to a file.
    QFile outputFile(filePath.isEmpty() ? this->filePath() : filePath);
    if (!outputFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        setLastError(tr("Failed to save Markdown file."));
        LOG_ERROR(lastError());
        return false;
    }

    outputFile.write(d->markdownContentVal.toUtf8());
    outputFile.close();
    setFilePath(filePath.isEmpty() ? this->filePath() : filePath);
    setModified(false);
    LOG_INFO("Successfully saved Markdown document: " << outputFile.fileName());
    return true;
}

Document::DocumentType MdDocument::type() const
{
    return DocumentType::MARKDOWN;
}

int MdDocument::pageCount() const
{
    return 1; // Markdown is typically a single-page document
}

Page* MdDocument::page(int index) const
{
    if (index == 0) {
        // return d->singlePage.get(); // Placeholder
        LOG_DEBUG("MdDocument::page: Requested single page.");
        return nullptr;
    }
    return nullptr;
}

bool MdDocument::isLocked() const
{
    return false;
}

bool MdDocument::isEncrypted() const
{
    return false;
}

QString MdDocument::formatVersion() const
{
    return "Markdown"; // Could detect specific flavors later
}

bool MdDocument::supportsFeature(const QString& feature) const
{
    static const QSet<QString> supportedFeatures = {
        "PlainText", "TextEditing", "Hyperlinks", "SimpleFormat"
    };
    return supportedFeatures.contains(feature);
}

QString MdDocument::markdownContent() const
{
    return d->markdownContentVal;
}

QString MdDocument::renderedHtml() const
{
    return d->renderedHtmlCache;
}

void MdDocument::createPages()
{
    // Create a single page for the Markdown content
    // d->singlePage = std::make_unique<MdPage>(this, 0); // Placeholder
    LOG_INFO("MdDocument: Created single page object.");
}

} // namespace QuantilyxDoc