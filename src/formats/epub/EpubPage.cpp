/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "EpubPage.h"
#include "EpubDocument.h"
#include "../../core/Logger.h"
#include <QTextDocument> // For basic HTML rendering/text extraction (Qt's built-in)
#include <QTextFrame>    // For iterating document structure
#include <QTextBlock>    // For iterating text blocks
#include <QTextFragment> // For iterating text fragments within blocks
#include <QPainter>      // For rendering QTextDocument to QImage
#include <QRegularExpression>
#include <QUrl>
#include <QDebug>
#include <QBuffer>
#include <QImage>
#include <QSvgRenderer> // If handling SVG rendering directly

namespace QuantilyxDoc {

class EpubPage::Private {
public:
    Private(EpubDocument* doc, int pIndex, const QString& htmlPath)
        : document(doc), pageIndexVal(pIndex), htmlFilePathVal(htmlPath) {}

    EpubDocument* document;
    int pageIndexVal;
    QString htmlFilePathVal;
    QString htmlContentVal;
    mutable QSizeF pageSize; // Cached size, calculated from HTML content or default
    mutable bool sizeCalculated = false;
    // Add members for parsed content like hyperlinks, image paths, etc., if needed for performance

    // Helper to load HTML content from the parent document's archive
    void loadHtmlContent() {
        if (document && !htmlFilePathVal.isEmpty()) {
            QByteArray contentBytes = document->getFileContent(htmlFilePathVal);
            if (!contentBytes.isEmpty()) {
                htmlContentVal = QString::fromUtf8(contentBytes);
                LOG_DEBUG("EpubPage: Loaded HTML content for page " << pageIndexVal << ", size: " << htmlContentVal.size() << " chars.");
                // Optionally, parse hyperlinks, images here to cache them
                parseHyperlinksAndImages();
            } else {
                LOG_ERROR("EpubPage: Failed to load HTML content for page " << pageIndexVal << " from path: " << htmlFilePathVal);
                htmlContentVal = "<html><body><p>Error: Could not load content.</p></body></html>";
            }
        }
    }

    // Helper to parse hyperlinks and images from the loaded HTML content
    void parseHyperlinksAndImages() {
        // Use QRegularExpression or QDomDocument to parse links and images
        // Example using regex for links:
        QRegularExpression linkRegex(R"(<a\s+[^>]*href\s*=\s*["']([^"']*)["'][^>]*>)", QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatchIterator linkIter = linkRegex.globalMatch(htmlContentVal);
        while (linkIter.hasNext()) {
            QRegularExpressionMatch match = linkIter.next();
            QString href = match.captured(1);
            // Validate and store QUrl
            QUrl url(href);
            if (url.isValid()) {
                // Store in a member variable if needed for performance
                // linksList.append(url);
            }
        }

        // Example for images:
        QRegularExpression imgRegex(R"(<img\s+[^>]*src\s*=\s*["']([^"']*)["'][^>]*>)", QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatchIterator imgIter = imgRegex.globalMatch(htmlContentVal);
        while (imgIter.hasNext()) {
            QRegularExpressionMatch match = imgIter.next();
            QString src = match.captured(1);
            // Validate and store image path
            // imagePathsList.append(src);
        }
    }

    // Helper to calculate page size based on HTML content (approximate)
    QSizeF calculateSize() const {
        if (sizeCalculated) return pageSize;

        // This is a very rough estimate. A real implementation would require
        // a full HTML layout engine (like WebEngine) to determine the actual rendered size.
        // For now, we'll use QTextDocument which handles basic HTML but not CSS layout well.
        QTextDocument doc;
        doc.setHtml(htmlContentVal);
        // This gives the ideal size based on content, not a fixed page size like PDF
        pageSize = doc.size(); // Returns QSizeF
        sizeCalculated = true;
        LOG_DEBUG("EpubPage: Calculated approximate size for page " << pageIndexVal << ": " << pageSize);
        return pageSize;
    }
};

EpubPage::EpubPage(EpubDocument* document, int pageIndex, const QString& htmlFilePath, QObject* parent)
    : Page(document, parent) // Pass EpubDocument as the core Document*
    , d(new Private(document, pageIndex, htmlFilePath))
{
    // Load HTML content on construction
    d->loadHtmlContent();

    // Set initial size based on content
    setSize(d->calculateSize());

    LOG_DEBUG("EpubPage created for index " << pageIndex << " from file: " << htmlFilePath);
}

EpubPage::~EpubPage()
{
    LOG_DEBUG("EpubPage for index " << d->pageIndexVal << " destroyed.");
}

QImage EpubPage::render(int width, int height, int dpi)
{
    // Rendering EPUB pages is complex due to HTML/CSS nature.
    // Qt's QTextDocument can render basic HTML, but it's not a full browser engine.
    // For high-fidelity rendering, Qt WebEngine/WebKit would be needed, which adds significant dependencies.
    // For Phase 4, let's use QTextDocument for basic rendering.

    if (d->htmlContentVal.isEmpty()) {
        LOG_WARN("EpubPage::render: No HTML content to render for page " << d->pageIndexVal);
        return QImage(); // Return null image
    }

    QTextDocument doc;
    doc.setHtml(d->htmlContentVal);
    // Set the page size for the document to match the requested render size
    // This affects how the content is laid out and rendered.
    doc.setPageSize(QSizeF(width, height));

    // Create an image buffer
    QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white); // Or a background color from CSS?

    QPainter painter(&image);
    if (!painter.isActive()) {
        LOG_ERROR("EpubPage::render: Failed to initialize painter for page " << d->pageIndexVal);
        return QImage(); // Return null image
    }

    // Render the QTextDocument onto the image
    // The painter needs to be set up correctly for the document's context
    // This is a simplified approach. QTextDocument's drawContents might not handle all HTML/CSS perfectly.
    // It might clip content or not render background images correctly without more setup.
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    // Save the painter state, translate, and scale if necessary based on DPI
    // For now, assume width/height already account for DPI
    painter.save();
    // painter.scale(scaleX, scaleY); // If needed based on target DPI vs. logical size
    doc.drawContents(&painter); // Draw the document content starting at painter's current transform (usually 0,0)
    painter.restore();

    painter.end(); // Explicitly end painting

    LOG_DEBUG("EpubPage::render: Rendered page " << d->pageIndexVal << " to image size " << image.size());
    return image;
}

QString EpubPage::text() const
{
    // Extract plain text from the HTML content.
    // QTextDocument is good for this.
    QTextDocument doc;
    doc.setHtml(d->htmlContentVal);
    QString plainText = doc.toPlainText();
    LOG_DEBUG("EpubPage::text: Extracted " << plainText.length() << " characters from page " << d->pageIndexVal);
    return plainText;
}

QList<QRectF> EpubPage::searchText(const QString& text, bool caseSensitive, bool wholeWords) const
{
    QList<QRectF> results;
    if (text.isEmpty() || d->htmlContentVal.isEmpty()) return results;

    // For EPUB, searching might be more complex than PDF because of HTML structure.
    // QTextDocument can highlight text, but getting the *exact* pixel coordinates
    // of matches within the HTML layout rendered by QTextDocument is difficult
    // without manually parsing the layout (QTextBlock, QTextFragment, etc.).
    // A more robust approach might involve using WebEngine for rendering and searching,
    // or using a text extraction tool that preserves position information.

    // For now, a simple approach using the plain text extracted by QTextDocument.
    // This loses positional accuracy relative to the HTML/CSS layout.
    QTextDocument doc;
    doc.setHtml(d->htmlContentVal);
    QString plainText = doc.toPlainText();

    Qt::CaseSensitivity cs = caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
    int pos = 0;
    while ((pos = plainText.indexOf(text, pos, cs)) != -1) {
        // This finds the character index in the *plain text*.
        // To get a meaningful rectangle, we would need to map this back to the HTML layout.
        // QTextDocument's find() could be used, but it returns cursors, and getting
        // the bounding rectangle of a cursor's selection is still tied to the QTextDocument's
        // simple layout, not the full HTML/CSS one.
        // For a placeholder, we return an empty rectangle or a rough estimate.
        // A more accurate implementation requires complex layout mapping.
        // Let's return a placeholder rectangle for now, indicating a match was found.
        results.append(QRectF(0, 0, 1, 1)); // Placeholder
        pos += text.length(); // Move past the current match
    }

    LOG_DEBUG("EpubPage::searchText: Found " << results.size() << " matches for '" << text << "' on page " << d->pageIndexVal);
    return results; // Placeholder implementation
}

QObject* EpubPage::hitTest(const QPointF& position) const
{
    // Hit testing on HTML content rendered via QTextDocument is limited.
    // QTextDocument doesn't provide easy access to the underlying HTML elements
    // or their rendered bounding boxes for precise hit testing.
    // WebEngine would be needed for accurate hit testing against links, images, etc.
    // For now, return nullptr or maybe check if the position is within the page bounds.
    Q_UNUSED(position);
    LOG_WARN("EpubPage::hitTest: Not implemented for HTML content. Requires WebEngine or complex layout mapping.");
    return nullptr;
}

QList<QObject*> EpubPage::links() const
{
    // Extracting links as QObject* is tricky. QTextDocument doesn't directly expose
    // link objects as QObjects. The hyperlinks are part of the HTML structure.
    // We could potentially parse the HTML directly using QDomDocument or regex
    // (as done in Private::parseHyperlinksAndImages) and return custom QObject wrappers
    // for each link, but that's complex.
    // For now, return an empty list or log the limitation.
    LOG_WARN("EpubPage::links: Returning empty list. Requires parsing HTML and creating link object wrappers.");
    return QList<QObject*>(); // Placeholder
}

QVariantMap EpubPage::metadata() const
{
    QVariantMap map;
    map["Index"] = d->pageIndexVal;
    map["HtmlFilePath"] = d->htmlFilePathVal;
    map["ContentSizeChars"] = d->htmlContentVal.size();
    // Add more specific page metadata if parsed from HTML content
    // map["Title"] = ...; // Extracted from <title> or <h1> tag?
    // map["Headings"] = ...; // Extracted from <h1>, <h2>, etc.?
    return map;
}

QString EpubPage::htmlFilePath() const
{
    return d->htmlFilePathVal;
}

QString EpubPage::htmlContent() const
{
    return d->htmlContentVal;
}

QStringList EpubPage::imagePaths() const
{
    // This would return the list parsed in Private::parseHyperlinksAndImages
    // For now, we'll parse it on demand or cache it. Let's parse on demand for simplicity here.
    QStringList paths;
    QRegularExpression imgRegex(R"(<img\s+[^>]*src\s*=\s*["']([^"']*)["'][^>]*>)", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatchIterator imgIter = imgRegex.globalMatch(d->htmlContentVal);
    while (imgIter.hasNext()) {
        QRegularExpressionMatch match = imgIter.next();
        QString src = match.captured(1);
        paths.append(src);
    }
    return paths;
}

QList<QUrl> EpubPage::hyperlinks() const
{
    // Similar to imagePaths, parse hyperlinks on demand.
    QList<QUrl> urls;
    QRegularExpression linkRegex(R"(<a\s+[^>]*href\s*=\s*["']([^"']*)["'][^>]*>)", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatchIterator linkIter = linkRegex.globalMatch(d->htmlContentVal);
    while (linkIter.hasNext()) {
        QRegularExpressionMatch match = linkIter.next();
        QString href = match.captured(1);
        QUrl url(href);
        if (url.isValid()) {
            urls.append(url);
        }
    }
    return urls;
}

bool EpubPage::hasMathMl() const
{
    return d->htmlContentVal.contains(QLatin1String("<math"), Qt::CaseInsensitive);
}

bool EpubPage::hasSvg() const
{
    return d->htmlContentVal.contains(QLatin1String("<svg"), Qt::CaseInsensitive);
}

} // namespace QuantilyxDoc