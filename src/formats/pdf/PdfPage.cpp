/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "PdfPage.h"
#include "PdfDocument.h"
#include "PdfAnnotation.h"
#include "PdfFormField.h"
#include "../../core/Logger.h"
#include <poppler-qt5.h>
#include <QImage>
#include <QPainter>
#include <QTransform>
#include <QRectF>
#include <QPointF>
#include <QList>
#include <QVariantMap>
#include <QDebug>

namespace QuantilyxDoc {

class PdfPage::Private {
public:
    Private(PdfDocument* doc, Poppler::Page* pPage, int pIndex)
        : document(doc), popplerPage(pPage), pdfPageIndex(pIndex) {}

    PdfDocument* document;
    Poppler::Page* popplerPage;
    int pdfPageIndex;
    mutable QList<std::unique_ptr<PdfAnnotation>> annotations; // Annotations for this page
    mutable bool annotationsLoaded; // Flag to avoid reloading
    mutable QList<std::unique_ptr<PdfFormField>> formFields; // Form fields for this page
    mutable bool formFieldsLoaded; // Flag to avoid reloading

    // Helper to load annotations from Poppler page
    void loadAnnotations() const {
        if (popplerPage && !annotationsLoaded) {
            auto popplerAnnots = popplerPage->annotations();
            annotations.reserve(popplerAnnots.size());
            for (auto* popplerAnnot : popplerAnnots) {
                // Create PdfAnnotation wrapper
                auto annot = std::make_unique<PdfAnnotation>(popplerAnnot, document, pdfPageIndex);
                annotations.append(std::move(annot));
            }
            annotationsLoaded = true;
            LOG_DEBUG("Loaded " << annotations.size() << " annotations for PDF page " << pdfPageIndex);
        }
    }

    // Helper to load form fields from the document that belong to this page
    void loadFormFields() const {
        if (document && !formFieldsLoaded) {
            // Get all form fields from the document
            auto docFormFields = document->formFields();
            formFields.clear();
            formFields.reserve(docFormFields.size());

            for (auto* docField : docFormFields) {
                // Check if this field belongs to this page index
                if (docField->pageIndex() == pdfPageIndex) {
                    // We might need to create a new PdfFormField wrapper pointing to the same Poppler object
                    // but associated with this specific page. Or PdfFormField just holds a reference.
                    // For now, let's assume PdfFormField can be re-wrapped or queried for page.
                    // This logic depends on how PdfFormField is designed to link to pages.
                    // If PdfFormField holds its page index, we can filter here.
                    // If PdfPage needs its own list of *pointers* to the document's fields, that's different.
                    // Let's assume PdfFormField has a method pageIndex().
                    if (docField->pageIndex() == pdfPageIndex) {
                        // Add a reference/pointer to the document's field list, or wrap again if necessary.
                        // For now, we'll just store pointers to the document's objects.
                        // formFields.append(std::make_unique<PdfFormField>(docField->popplerFormField())); // This creates a new wrapper
                        // Or just store the existing pointer from the document's list, which requires managing lifetimes carefully.
                        // A better approach might be to have PdfDocument own the wrappers and PdfPage just hold pointers/references.
                        // For simplicity in this stub, let's assume PdfFormField can be queried for its page.
                        formFields.append(std::make_unique<PdfFormField>(docField->popplerFormField())); // Create page-specific wrapper
                    }
                }
            }
            formFieldsLoaded = true;
            LOG_DEBUG("Loaded " << formFields.size() << " form fields for PDF page " << pdfPageIndex);
        }
    }
};

PdfPage::PdfPage(PdfDocument* document, Poppler::Page* popplerPage, int pageIndex, QObject* parent)
    : Page(document, parent) // Call base Page constructor, passing PdfDocument as Document*
    , d(new Private(document, popplerPage, pageIndex))
{
    if (popplerPage) {
        // Set base Page properties from Poppler page
        setSize(popplerPage->pageSizeF()); // Size in points
        // Rotation might be stored in Poppler page or PDF, set it
        // setPageRotation(...); // Need to get from Poppler::Page::rotation() or PDF spec
        // setTitle(...); // If available from Poppler
        // setLabel(...); // If available from Poppler (e.g., page labels in PDF)
        LOG_DEBUG("PdfPage created for index " << pageIndex << " in document: " << (document ? document->filePath() : "nullptr"));
    } else {
        LOG_ERROR("PdfPage created with null Poppler::Page for index " << pageIndex);
    }
}

PdfPage::~PdfPage()
{
    LOG_DEBUG("PdfPage for index " << d->pdfPageIndex << " destroyed.");
}

QImage PdfPage::render(int width, int height, int dpi)
{
    if (!d->popplerPage) {
        LOG_ERROR("Cannot render PdfPage " << d->pdfPageIndex << ": Poppler page is null.");
        return QImage(); // Return null image
    }

    // Calculate scale factors based on target pixel size and page size in points
    // Poppler's renderToImage takes a scale factor (e.g., 72 for 72 DPI).
    // We need to derive the scale from the desired pixel dimensions.
    QSizeF pageSizePoints = d->popplerPage->pageSizeF();
    qreal scaleX = (width > 0) ? (width / pageSizePoints.width()) * (dpi / 72.0) : 1.0;
    qreal scaleY = (height > 0) ? (height / pageSizePoints.height()) * (dpi / 72.0) : 1.0;
    // Use the smaller scale to fit within the bounds, or average/max depending on fit strategy
    qreal scale = qMin(scaleX, scaleY);

    // Render using Poppler
    QImage image = d->popplerPage->renderToImage(scale * 72.0 / dpi, 0, 0, width, height); // Pass calculated DPI-like scale

    if (image.isNull()) {
        LOG_ERROR("Poppler failed to render page " << d->pdfPageIndex);
        // Poppler doesn't give detailed error codes easily here.
        // Could check if page size is valid, etc.
    } else {
        LOG_DEBUG("Rendered PdfPage " << d->pdfPageIndex << " to image size " << image.size());
    }

    return image;
}

QString PdfPage::text() const
{
    if (!d->popplerPage) return QString();

    // Use Poppler's text extraction
    // The coordinates are in PDF's coordinate system (bottom-left origin).
    // Poppler::Page::text() can take a QRectF to get text from a specific area.
    // For the whole page, pass an empty rect or the mediaBox.
    QRectF pageRect = mediaBox(); // Use the full page area
    QString pageText = d->popplerPage->text(pageRect);
    LOG_DEBUG("Extracted text from PdfPage " << d->pdfPageIndex << ", length: " << pageText.length());
    return pageText;
}

QList<QRectF> PdfPage::searchText(const QString& text, bool caseSensitive, bool wholeWords) const
{
    QList<QRectF> results;
    if (!d->popplerPage || text.isEmpty()) return results;

    // Poppler doesn't have a direct "search and return rectangles" function.
    // We need to get the text layout (character positions) and match manually,
    // or use Poppler::Page::search() which updates the page object's highlight state
    // (not ideal for backend searching).
    // A common approach is to get the text and its bounding boxes using textList or similar.
    // Poppler::Page::textList() returns a list of text boxes with positions and text.
    // This is more reliable than trying to match against a single text string and guessing positions.

    auto textBoxes = d->popplerPage->textList(); // Returns QList<Poppler::TextBox*>
    Qt::CaseSensitivity cs = caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;

    for (auto* textBox : textBoxes) {
        if (textBox) {
            QString boxText = textBox->text();
            // Perform search within this text box
            int pos = 0;
            while ((pos = boxText.indexOf(text, pos, cs)) != -1) {
                // Calculate the approximate rectangle for the found text within the box
                // This is complex as Poppler::TextBox gives the overall box and character positions.
                // For simplicity, we'll add the whole box rectangle if it contains the text.
                // A more accurate implementation would use Poppler::TextBox::charX/charY arrays.
                if (wholeWords) {
                    // Check if match is at word boundary
                    bool isWordBoundaryBefore = (pos == 0) || boxText[pos - 1].isSpace();
                    bool isWordBoundaryAfter = (pos + text.length() == boxText.length()) || boxText[pos + text.length()].isSpace();
                    if (isWordBoundaryBefore && isWordBoundaryAfter) {
                        results.append(textBox->boundingBox()); // Add the box containing the match
                    }
                } else {
                    results.append(textBox->boundingBox()); // Add the box containing the match
                }
                pos += text.length(); // Move past the current match
            }
        }
    }
    LOG_DEBUG("Searched for text '" << text << "' on PdfPage " << d->pdfPageIndex << ", found " << results.size() << " matches.");
    return results;
}

QObject* PdfPage::hitTest(const QPointF& position) const
{
    // This is a generic hit test. It should check for links, annotations, text, images, etc.
    // based on the position in PDF coordinates.
    // First, check for annotations.
    QObject* hitAnnot = hitTestAnnotation(position);
    if (hitAnnot) return hitAnnot;

    // Then, check for links.
    QObject* hitLink = hitTestLink(position);
    if (hitLink) return hitLink;

    // Could add hit tests for text boxes, images, etc., if needed for selection.
    // For now, return nullptr if nothing specific was hit.
    return nullptr;
}

QList<QObject*> PdfPage::links() const
{
    QList<QObject*> linkObjects;
    if (!d->popplerPage) return linkObjects;

    // Poppler::Page::links() returns QList<Poppler::Link*>
    auto popplerLinks = d->popplerPage->links();
    linkObjects.reserve(popplerLinks.size());
    for (auto* popplerLink : popplerLinks) {
        if (popplerLink) {
            // Wrap Poppler::Link in a custom QObject if necessary for the UI
            // For now, we might just return the Poppler::Link pointer casted,
            // but it's not a QObject. We'd need a wrapper like PdfLink (hypothetical).
            // linkObjects.append(static_cast<QObject*>(popplerLink)); // This won't compile
            // Instead, create a wrapper object or return nullptrs/empty if not needed immediately.
            LOG_WARN("PdfPage::links(): Returning empty list. Requires PdfLink wrapper objects.");
        }
    }
    return linkObjects; // Currently empty
}

QVariantMap PdfPage::metadata() const
{
    QVariantMap map;
    if (d->popplerPage) {
        // Populate page-specific metadata
        map["Index"] = d->pdfPageIndex;
        map["SizePoints"] = d->popplerPage->pageSizeF();
        map["Rotation"] = d->popplerPage->rotation(); // Poppler::Page::Rotation enum or int?
        // Add more specific page metadata if available from Poppler
        // map["Label"] = ...; // If page labels are available
    }
    return map;
}

Poppler::Page* PdfPage::popplerPage() const
{
    return d->popplerPage;
}

QString PdfPage::label() const
{
    // PDFs can have custom page labels (e.g., "i", "ii", "1", "2", "A-1").
    // Poppler might expose these via a specific function or require parsing the page tree.
    // Let's assume a function exists or derive from index if not.
    // Poppler::Page doesn't seem to have a direct pageLabel() function.
    // Page labels are part of the PDF's Page Tree structure, not individual pages directly.
    // This might require accessing the Poppler::Document's catalog/page tree.
    // For now, return an empty string or the index as a fallback.
    LOG_WARN("PdfPage::label(): Poppler-Qt5 does not directly provide page labels. Requires parsing PDF page tree.");
    return QString(); // Or QString::number(d->pdfPageIndex + 1); as a fallback
}

int PdfPage::pdfRotation() const
{
    return d->popplerPage ? static_cast<int>(d->popplerPage->rotation()) : 0; // Poppler::Page::Rotation is an enum
}

QRectF PdfPage::cropBox() const
{
    return d->popplerPage ? d->popplerPage->cropBox() : QRectF();
}

QRectF PdfPage::mediaBox() const
{
    return d->popplerPage ? d->popplerPage->pageSizeF() : QRectF(); // Media box is often the same as page size in Poppler
}

bool PdfPage::hasAnnotations() const
{
    if (!d->popplerPage) return false;
    d->loadAnnotations(); // Ensure annotations are loaded
    return !d->annotations.isEmpty();
}

bool PdfPage::hasFormFields() const
{
    if (!d->document) return false;
    d->loadFormFields(); // Ensure form fields for this page are loaded from the document
    return !d->formFields.isEmpty();
}

QList<PdfAnnotation*> PdfPage::pdfAnnotations() const
{
    QList<PdfAnnotation*> ptrList;
    if (!d->popplerPage) return ptrList;

    d->loadAnnotations(); // Ensure annotations are loaded
    ptrList.reserve(d->annotations.size());
    for (const auto& annot : d->annotations) {
        ptrList.append(annot.get()); // Get raw pointer from unique_ptr
    }
    return ptrList;
}

QList<PdfFormField*> PdfPage::pdfFormFields() const
{
    QList<PdfFormField*> ptrList;
    if (!d->document) return ptrList;

    d->loadFormFields(); // Ensure form fields for this page are loaded
    ptrList.reserve(d->formFields.size());
    for (const auto& field : d->formFields) {
        ptrList.append(field.get()); // Get raw pointer from unique_ptr
    }
    return ptrList;
}

QList<QRectF> PdfPage::textLayout() const
{
    QList<QRectF> layout;
    if (!d->popplerPage) return layout;

    auto textBoxes = d->popplerPage->textList();
    layout.reserve(textBoxes.size());
    for (auto* textBox : textBoxes) {
        if (textBox) {
            layout.append(textBox->boundingBox());
        }
    }
    return layout;
}

QImage PdfPage::renderRectangle(const QRectF& rect, int width, int height, int dpi) const
{
    if (!d->popplerPage || rect.isEmpty()) return QImage();

    // Calculate scale based on target pixel dimensions and source rectangle dimensions
    qreal scaleX = (width > 0) ? (width / rect.width()) * (dpi / 72.0) : 1.0;
    qreal scaleY = (height > 0) ? (height / rect.height()) * (dpi / 72.0) : 1.0;
    qreal scale = qMin(scaleX, scaleY);

    // Poppler's renderToImage can render a cropped region, but the API is slightly different.
    // We render the full page scaled and then crop the QImage, or use Poppler's internal cropping.
    // Poppler::Page::renderToImage(scale, offsetX, offsetY, w, h)
    // To render *only* the rect, we calculate the offset from the page's origin to the rect's origin.
    QRectF pageMediaBox = mediaBox(); // Assuming media box origin is (0,0) in PDF space for this calculation
    int offsetX = qRound((rect.left() - pageMediaBox.left()) * scale * 72.0 / dpi);
    int offsetY = qRound((pageMediaBox.bottom() - rect.bottom()) * scale * 72.0 / dpi); // PDF Y is inverted compared to image Y
    int scaledW = qRound(rect.width() * scale * 72.0 / dpi);
    int scaledH = qRound(rect.height() * scale * 72.0 / dpi);

    QImage fullPageImage = d->popplerPage->renderToImage(scale * 72.0 / dpi);
    if (fullPageImage.isNull()) return QImage();

    // Crop the full page image to the requested rectangle
    QRect cropRect(offsetX, offsetY, scaledW, scaledH);
    if (cropRect.intersects(fullPageImage.rect())) {
        QImage croppedImage = fullPageImage.copy(cropRect);
        LOG_DEBUG("Rendered rectangle " << rect << " from PdfPage " << d->pdfPageIndex << " to image size " << croppedImage.size());
        return croppedImage;
    }
    return QImage(); // Return null if crop rect is outside the rendered page
}

QList<QRectF> PdfPage::imageLocations() const
{
    // Poppler::Page might have a function to get image locations, or this requires
    // more complex parsing of the page content stream.
    // Poppler::Page::images() might exist in newer versions or requires specific build flags.
    // For now, this is a stub.
    LOG_WARN("PdfPage::imageLocations(): Not implemented with Poppler-Qt5.");
    return QList<QRectF>();
}

QStringList PdfPage::fontsUsed() const
{
    // Poppler::Page might have a function to list fonts used on the page.
    // This often requires parsing the content stream.
    // For now, this is a stub.
    LOG_WARN("PdfPage::fontsUsed(): Not implemented with Poppler-Qt5.");
    return QStringList();
}

QObject* PdfPage::hitTestLink(const QPointF& point) const
{
    // Iterate through Poppler::Link objects and check if the point is inside their rectangle.
    if (!d->popplerPage) return nullptr;

    auto popplerLinks = d->popplerPage->links();
    for (auto* popplerLink : popplerLinks) {
        if (popplerLink && popplerLink->linkArea().contains(point)) { // linkArea() is in PDF coordinates
            // Return a wrapper object for the link, or the Poppler object if castable (it's not QObject)
            // Need PdfLink wrapper.
            LOG_WARN("PdfPage::hitTestLink: Requires PdfLink wrapper object.");
            return nullptr; // For now
        }
    }
    return nullptr;
}

QObject* PdfPage::hitTestAnnotation(const QPointF& point) const
{
    // Iterate through PdfAnnotation objects and check if the point is inside their rectangle/type-specific area.
    if (!d->popplerPage) return nullptr;

    d->loadAnnotations(); // Ensure annotations are loaded
    for (const auto& pdfAnnot : d->annotations) {
        if (pdfAnnot && pdfAnnot->bounds().contains(point)) { // Assuming bounds() returns QRectF in PDF coordinates
            // Return the annotation object itself (it inherits from QObject)
            return pdfAnnot.get();
        }
    }
    return nullptr;
}

QPointF PdfPage::pdfToPixel(const QPointF& pdfPoint, const QSize& renderSize) const
{
    if (!d->popplerPage || renderSize.isEmpty()) return QPointF();

    QSizeF pageSizePoints = d->popplerPage->pageSizeF();
    qreal scaleX = renderSize.width() / pageSizePoints.width();
    qreal scaleY = renderSize.height() / pageSizePoints.height();

    // PDF coordinates origin is bottom-left, image/pixel coordinates origin is top-left.
    // So Y needs to be inverted.
    qreal pixelX = pdfPoint.x() * scaleX;
    qreal pixelY = (pageSizePoints.height() - pdfPoint.y()) * scaleY;

    return QPointF(pixelX, pixelY);
}

QPointF PdfPage::pixelToPdf(const QPointF& pixelPoint, const QSize& renderSize) const
{
    if (!d->popplerPage || renderSize.isEmpty()) return QPointF();

    QSizeF pageSizePoints = d->popplerPage->pageSizeF();
    qreal scaleX = pageSizePoints.width() / renderSize.width();
    qreal scaleY = pageSizePoints.height() / renderSize.height();

    // Invert Y coordinate from pixel space (top-left) to PDF space (bottom-left)
    qreal pdfX = pixelPoint.x() * scaleX;
    qreal pdfY = pageSizePoints.height() - (pixelPoint.y() * scaleY);

    return QPointF(pdfX, pdfY);
}

} // namespace QuantilyxDoc