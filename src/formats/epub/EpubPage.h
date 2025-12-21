/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#ifndef QUANTILYX_EPUBPAGE_H
#define QUANTILYX_EPUBPAGE_H

#include "../../core/Page.h"
#include <memory>
#include <QSizeF>
#include <QRectF>
#include <QImage>

namespace QuantilyxDoc {

class EpubDocument;

/**
 * @brief EPUB page implementation.
 * 
 * Represents a single content document (usually an XHTML file) within an EPUB.
 * Handles rendering the HTML content and potentially extracting text.
 */
class EpubPage : public Page
{
    Q_OBJECT

public:
    /**
     * @brief Constructor.
     * @param document The parent EpubDocument this page belongs to.
     * @param pageIndex The 0-based index of this page within the document's spine order.
     * @param htmlFilePath The path to the HTML file inside the EPUB archive corresponding to this page.
     * @param parent Parent object.
     */
    explicit EpubPage(EpubDocument* document, int pageIndex, const QString& htmlFilePath, QObject* parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~EpubPage() override;

    // --- Page Interface Implementation ---
    QImage render(int width, int height, int dpi = 72) override;
    QString text() const override;
    QList<QRectF> searchText(const QString& text, bool caseSensitive = false, bool wholeWords = false) const override;
    QObject* hitTest(const QPointF& position) const override; // Might be complex for HTML
    QList<QObject*> links() const override; // Extract hyperlinks from the HTML content
    QVariantMap metadata() const override;

    // --- EPUB-Specific Page Properties ---
    /**
     * @brief Get the path to the HTML file inside the archive for this page.
     * @return File path string.
     */
    QString htmlFilePath() const;

    /**
     * @brief Get the raw HTML content of this page.
     * @return HTML content as QString.
     */
    QString htmlContent() const;

    /**
     * @brief Get the list of image paths referenced in this page's HTML.
     * @return List of image file paths.
     */
    QStringList imagePaths() const;

    /**
     * @brief Get the list of hyperlinks present in this page's HTML.
     * @return List of QUrl objects.
     */
    QList<QUrl> hyperlinks() const;

    /**
     * @brief Check if this page contains MathML.
     * @return True if MathML is present.
     */
    bool hasMathMl() const;

    /**
     * @brief Check if this page contains SVG graphics.
     * @return True if SVG is present.
     */
    bool hasSvg() const;

signals:
    /**
     * @brief Emitted when the page's content (HTML, images, etc.) is loaded or changes significantly.
     * This might be triggered by external edits or updates to the source EPUB file.
     */
    void contentChanged();

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace QuantilyxDoc

#endif // QUANTILYX_EPUBPAGE_H