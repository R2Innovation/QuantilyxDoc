/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#ifndef QUANTILYX_PSPAGE_H
#define QUANTILYX_PSPAGE_H

#include "../../core/Page.h"
#include <memory>
#include <QSizeF>
#include <QRectF>
#include <QImage>
#include <QProcess>

namespace QuantilyxDoc {

class PsDocument;

/**
 * @brief PostScript page implementation.
 * 
 * Represents a single page within a PostScript document.
 * Renders the page using Ghostscript command-line tool.
 */
class PsPage : public Page
{
    Q_OBJECT

public:
    /**
     * @brief Constructor.
     * @param document The parent PsDocument this page belongs to.
     * @param pageIndex The 0-based index of this page within the document.
     * @param parent Parent object.
     */
    explicit PsPage(PsDocument* document, int pageIndex, QObject* parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~PsPage() override;

    // --- Page Interface Implementation ---
    QImage render(int width, int height, int dpi = 72) override;
    QString text() const override; // Requires OCR or text extraction from PS
    QList<QRectF> searchText(const QString& text, bool caseSensitive = false, bool wholeWords = false) const override;
    QObject* hitTest(const QPointF& position) const override;
    QList<QObject*> links() const override;
    QVariantMap metadata() const override;

    // --- PS-Specific Page Properties ---
    /**
     * @brief Get the bounding box for this specific page.
     * @return Page bounding box.
     */
    QRectF pageBoundingBox() const;

private:
    class Private;
    std::unique_ptr<Private> d;

    // Helper to render the page using Ghostscript and return QImage
    QImage renderWithGhostscript(int width, int height, int dpi);
};

} // namespace QuantilyxDoc

#endif // QUANTILYX_PSPAGE_H