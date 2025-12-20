/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#ifndef QUANTILYX_PDFPAGE_H
#define QUANTILYX_PDFPAGE_H

#include "../../core/Page.h"
#include <poppler-qt5.h>
#include <memory>
#include <QList>

namespace QuantilyxDoc {

class PdfDocument;
class PdfAnnotation;
class PdfFormField; // For form fields on this page

/**
 * @brief PDF page implementation using Poppler.
 * 
 * Concrete implementation of the Page interface specifically for a page within a PDF document.
 * Uses Poppler-Qt5 to handle rendering, text extraction, and annotation access for this page.
 */
class PdfPage : public Page
{
    Q_OBJECT

public:
    /**
     * @brief Constructor.
     * @param document The parent PdfDocument this page belongs to.
     * @param popplerPage The underlying Poppler page object.
     * @param pageIndex The 0-based index of this page within the document.
     * @param parent Parent object.
     */
    explicit PdfPage(PdfDocument* document, Poppler::Page* popplerPage, int pageIndex, QObject* parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~PdfPage() override;

    // --- Page Interface Implementation ---
    QImage render(int width, int height, int dpi = 72) override;
    QString text() const override;
    QList<QRectF> searchText(const QString& text, bool caseSensitive = false, bool wholeWords = false) const override;
    QObject* hitTest(const QPointF& position) const override;
    QList<QObject*> links() const override;
    QVariantMap metadata() const override;

    // --- PDF-Specific Page Properties ---
    /**
     * @brief Get the underlying Poppler page object.
     * @return Poppler page object or nullptr.
     */
    Poppler::Page* popplerPage() const;

    /**
     * @brief Get the page label (custom page number string).
     * @return Page label or empty string if none.
     */
    QString label() const override; // Override to get from Poppler

    /**
     * @brief Get the page rotation as defined in the PDF.
     * @return Rotation in degrees (0, 90, 180, 270).
     */
    int pdfRotation() const;

    /**
     * @brief Get the page crop box (the visible area after applying crop/trim/bleed).
     * @return Crop box in PDF coordinates (points).
     */
    QRectF cropBox() const;

    /**
     * @brief Get the page media box (the full page size).
     * @return Media box in PDF coordinates (points).
     */
    QRectF mediaBox() const;

    /**
     * @brief Check if the page has annotations.
     * @return True if annotations exist.
     */
    bool hasAnnotations() const;

    /**
     * @brief Check if the page has form fields.
     * @return True if form fields exist.
     */
    bool hasFormFields() const;

    /**
     * @brief Get the list of annotations on this page.
     * @return List of PdfAnnotation objects.
     */
    QList<PdfAnnotation*> pdfAnnotations() const;

    /**
     * @brief Get the list of form fields on this page.
     * @return List of PdfFormField objects.
     */
    QList<PdfFormField*> pdfFormFields() const;

    /**
     * @brief Get the text layout information for this page.
     * @return List of QRectF representing text boxes.
     */
    QList<QRectF> textLayout() const;

    /**
     * @brief Render a specific part of the page (e.g., for thumbnails or annotations).
     * @param rect The rectangle in PDF coordinates to render.
     * @param width Target width in pixels for the rendered rectangle.
     * @param height Target height in pixels for the rendered rectangle.
     * @param dpi DPI for rendering.
     * @return Rendered image of the specified rectangle.
     */
    QImage renderRectangle(const QRectF& rect, int width, int height, int dpi = 72) const;

    /**
     * @brief Get the list of images on this page.
     * @return List of image rectangles and potentially metadata.
     */
    QList<QRectF> imageLocations() const;

    /**
     * @brief Get the list of fonts used on this page.
     * @return List of font names.
     */
    QStringList fontsUsed() const;

    /**
     * @brief Check if a specific point (in PDF coordinates) is within a link.
     * @param point The point to check.
     * @return The link object if found, otherwise nullptr.
     */
    QObject* hitTestLink(const QPointF& point) const;

    /**
     * @brief Check if a specific point (in PDF coordinates) is within an annotation.
     * @param point The point to check.
     * @return The annotation object if found, otherwise nullptr.
     */
    QObject* hitTestAnnotation(const QPointF& point) const;

    /**
     * @brief Convert a point from PDF coordinates to pixel coordinates based on the current render size.
     * @param pdfPoint The point in PDF coordinates (1/72 inch).
     * @param renderSize The size the page was rendered at (in pixels).
     * @return The point in pixel coordinates.
     */
    QPointF pdfToPixel(const QPointF& pdfPoint, const QSize& renderSize) const;

    /**
     * @brief Convert a point from pixel coordinates to PDF coordinates.
     * @param pixelPoint The point in pixel coordinates.
     * @param renderSize The size the page was rendered at (in pixels).
     * @return The point in PDF coordinates (1/72 inch).
     */
    QPointF pixelToPdf(const QPointF& pixelPoint, const QSize& renderSize) const;

signals:
    /**
     * @brief Emitted when the page's annotations list changes.
     */
    void annotationsChanged();

    /**
     * @brief Emitted when the page's form fields list changes.
     */
    void formFieldsChanged();

    /**
     * @brief Emitted when the page's content (text, images, etc.) changes significantly.
     * This might be triggered by editing operations if/when supported.
     */
    void contentChanged();

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace QuantilyxDoc

#endif // QUANTILYX_PDFPAGE_H