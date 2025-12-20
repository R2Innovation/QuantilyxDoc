/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "DocumentView.h"
#include "../core/Document.h"
#include "../core/Page.h"
#include "../core/PageCache.h"
#include "../core/RenderThread.h"
#include "../core/Settings.h"
#include "../core/Selection.h"
#include "../core/UndoStack.h"
#include "../core/Logger.h"
#include "../core/Selection.h" // Assuming this exists or will be adapted
#include "../core/Clipboard.h" // Assuming this exists or we use QApplication::clipboard()
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QScrollBar>
#include <QTimer>
#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QMenu>
#include <QAction>
#include <QCursor>
#include <QDebug>

namespace QuantilyxDoc {

class DocumentView::Private {
public:
    Private(DocumentView* q_ptr)
        : q(q_ptr), document(nullptr), currentPageIndex(0), zoomLevel(1.0),
          zoomMode(FitPage), viewMode(SinglePage), rotation(0),
          pageSpacing(10), isPanning(false), lastPanPoint(0, 0),
          isSelecting(false), selectionStartPoint(0, 0), selectionEndPoint(0, 0),
          renderRequestCounter(0) {}

    DocumentView* q;
    QPointer<Document> document; // Use QPointer for safety
    int currentPageIndex;
    qreal zoomLevel;
    ZoomMode zoomMode;
    ViewMode viewMode;
    int rotation; // 0, 90, 180, 270
    int pageSpacing;
    QPoint documentOffset; // Offset to scroll the document content

    // Panning state
    bool isPanning;
    QPoint lastPanPoint;

    // Selection state
    bool isSelecting;
    QPoint selectionStartPoint;
    QPoint selectionEndPoint;
    QRectF currentSelectionRect; // Document coordinates (PDF points)
    QString selectedText; // Cached text for the current selection

    // Rendering
    quintptr currentRenderRequestId;
    int renderRequestCounter; // For generating unique IDs

    // Cached page sizes for layout calculations
    mutable QHash<int, QSize> cachedPageSizePixels;

    // Helper to calculate page size in pixels based on zoom/rotation
    QSize calculatePageSizePixels(int pageIndex) const {
        if (!document) return QSize();
        Page* page = document->page(pageIndex);
        if (!page) return QSize();

        QSizeF pageSizePoints = page->size();
        qreal dpi = 72.0; // Base DPI for calculations
        qreal scale = zoomLevel * (dpi / 72.0); // Adjust for zoom and target DPI
        QSize size = QSize(qRound(pageSizePoints.width() * scale), qRound(pageSizePoints.height() * scale));

        // Apply rotation effect on size
        if (rotation == 90 || rotation == 270) {
            size.transpose(); // Swap width/height
        }
        return size;
    }

    // Helper to get document size in pixels (total scrollable area)
    QSizeF documentSizePixels() const {
        if (!document || document->pageCount() == 0) return QSizeF();

        int pageCount = document->pageCount();
        int totalHeight = 0;
        int maxWidth = 0;

        for (int i = 0; i < pageCount; ++i) {
            QSize pageSize = calculatePageSizePixels(i);
            if (pageSize.width() > maxWidth) maxWidth = pageSize.width();
            totalHeight += pageSize.height();
            if (i < pageCount - 1) { // Add spacing between pages, except after the last one
                totalHeight += pageSpacing;
            }
        }

        return QSizeF(maxWidth, totalHeight);
    }

    // Helper to convert viewport coordinates to document coordinates
    QPointF viewportToDocument(const QPointF& viewportPos) const {
        return viewportPos - documentOffset;
    }

    // Helper to convert document coordinates to viewport coordinates
    QPointF documentToViewport(const QPointF& docPos) const {
        return docPos + documentOffset;
    }

    // Helper to schedule a repaint for a specific page rect
    void scheduleRepaintForPageRect(int pageIndex, const QRectF& pageRect) {
        QSize pageSize = calculatePageSizePixels(pageIndex);
        // Calculate the viewport rect for this page section
        QRect viewportRect = QRect(documentToViewport(pageRect.topLeft()).toPoint(),
                                   QSize(qRound(pageRect.width()), qRound(pageRect.height())));
        q->viewport()->update(viewportRect);
    }

    // Helper to update scrollbars based on document size and viewport size
    void updateScrollBars() {
        if (!document) {
            q->horizontalScrollBar()->setRange(0, 0);
            q->verticalScrollBar()->setRange(0, 0);
            return;
        }

        QSizeF docSize = documentSizePixels();
        QSize viewSize = q->viewport()->size();

        int maxH = qMax(0, qRound(docSize.width()) - viewSize.width());
        int maxV = qMax(0, qRound(docSize.height()) - viewSize.height());

        q->horizontalScrollBar()->setRange(0, maxH);
        q->verticalScrollBar()->setRange(0, maxV);

        // Ensure current offset is within bounds
        documentOffset.setX(qBound(0, documentOffset.x(), maxH));
        documentOffset.setY(qBound(0, documentOffset.y(), maxV));

        q->horizontalScrollBar()->setValue(documentOffset.x());
        q->verticalScrollBar()->setValue(documentOffset.y());
    }

    // Helper to update zoom level based on mode and current viewport size
    void updateZoomForMode() {
        if (!document || document->pageCount() <= currentPageIndex) return;

        Page* page = document->page(currentPageIndex);
        if (!page) return;

        QSizeF pageSizePoints = page->size();
        QSize viewSize = q->viewport()->size();

        switch (zoomMode) {
            case FitPage:
                // Calculate zoom to fit the current page height (and width if necessary) into the view
                qreal zoomH = static_cast<qreal>(viewSize.height()) / pageSizePoints.height();
                qreal zoomW = static_cast<qreal>(viewSize.width()) / pageSizePoints.width();
                zoomLevel = qMin(zoomH, zoomW);
                break;
            case FitWidth:
                // Calculate zoom to fit the current page width into the view
                zoomLevel = static_cast<qreal>(viewSize.width()) / pageSizePoints.width();
                break;
            case FitVisible:
                // This is more complex, depends on what's currently visible.
                // For now, treat as FitPage.
                zoomLevel = qMin(static_cast<qreal>(viewSize.height()) / pageSizePoints.height(),
                                 static_cast<qreal>(viewSize.width()) / pageSizePoints.width());
                break;
            case CustomZoom:
                // zoomLevel is already set by the user
                break;
        }
        LOG_DEBUG("Updated zoom level to " << zoomLevel << " for mode " << static_cast<int>(zoomMode));
    }

    // Helper to handle a completed render result
    void handleRenderResult(const RenderThread::RenderResult& result) {
        if (result.requestId != currentRenderRequestId) {
            // This result is for an old request, ignore it
            LOG_DEBUG("Ignoring stale render result for request ID: " << result.requestId);
            return;
        }

        if (result.success) {
            // Update the cached image for the page associated with this request
            // This requires knowing which page/region this request was for.
            // We need to store this info alongside the request ID.
            // For now, we'll assume a simple page-index-based request.
            // A more robust system would have a map requestId -> PageInfo.
            LOG_DEBUG("Render result received for request ID: " << result.requestId << ", updating view.");
            q->viewport()->update(); // Trigger repaint
        } else {
            LOG_ERROR("Render failed for request ID " << result.requestId << ": " << result.errorMessage);
            // Update status bar or show error?
        }
        // Reset request ID to indicate no active request for this page/region
        currentRenderRequestId = 0;
    }
};

DocumentView::DocumentView(QWidget* parent)
    : QAbstractScrollArea(parent)
    , d(new Private(this))
{
    // Set up scroll area
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setFocusPolicy(Qt::StrongFocus); // So it can receive keyboard events
    setMouseTracking(true); // Needed for hover effects, selection, etc.

    // Connect scrollbars to handle scrolling
    connect(horizontalScrollBar(), &QScrollBar::valueChanged,
            [this](int value) { d->documentOffset.setX(value); viewport()->update(); });
    connect(verticalScrollBar(), &QScrollBar::valueChanged,
            [this](int value) { d->documentOffset.setY(value); viewport()->update(); });

    // Connect to RenderThread to handle results
    connect(&RenderThread::instance(), &RenderThread::renderCompleted,
            [this](const RenderThread::RenderResult& result) {
                d->handleRenderResult(result);
            });

    // Connect to Settings to react to changes (e.g., background color)
    // connect(&Settings::instance(), &Settings::valueChanged, this, &DocumentView::onSettingsChanged);

    LOG_INFO("DocumentView initialized.");
}

DocumentView::~DocumentView()
{
    // Cancel any pending render requests associated with this view
    if (d->currentRenderRequestId != 0) {
        RenderThread::instance().cancelRequest(d->currentRenderRequestId);
    }
    LOG_INFO("DocumentView destroyed.");
}

void DocumentView::setDocument(Document* document)
{
    if (d->document == document) return;

    // Disconnect from old document signals if necessary
    if (d->document) {
        // disconnect(d->document, ...);
    }

    d->document = document; // Use QPointer
    d->currentPageIndex = 0; // Reset to first page

    if (document) {
        // Connect to new document signals
        connect(document, &Document::closed, this, [this]() {
            setDocument(nullptr); // Clear if document is closed elsewhere
        });
        connect(document, &Document::currentPageChanged, this, [this](int index) {
            d->currentPageIndex = index;
            goToPage(index); // Ensure view reflects the change
        });
        // Connect to page count changes to update scroll bars
        // connect(document, &Document::pageCountChanged, this, [this]() { d->updateScrollBars(); });

        // Update zoom mode if set to auto-fit
        if (d->zoomMode == FitPage || d->zoomMode == FitWidth) {
            d->updateZoomForMode();
        }
        LOG_INFO("DocumentView set to document: " << document->filePath());
    } else {
        LOG_INFO("DocumentView cleared (no document).");
    }

    d->updateScrollBars();
    viewport()->update(); // Repaint the view
    emit documentChanged(document);
}

Document* DocumentView::document() const
{
    return d->document.data(); // Returns nullptr if document was deleted
}

void DocumentView::setViewMode(ViewMode mode)
{
    if (d->viewMode != mode) {
        d->viewMode = mode;
        LOG_DEBUG("View mode changed to " << static_cast<int>(mode));
        d->updateScrollBars();
        viewport()->update();
        emit viewModeChanged(mode);
    }
}

DocumentView::ViewMode DocumentView::viewMode() const
{
    return d->viewMode;
}

void DocumentView::setZoomMode(ZoomMode mode)
{
    if (d->zoomMode != mode) {
        d->zoomMode = mode;
        if (d->document) {
            d->updateZoomForMode(); // Recalculate zoom based on new mode
            d->updateScrollBars();
            viewport()->update();
        }
        LOG_DEBUG("Zoom mode changed to " << static_cast<int>(mode));
        emit zoomModeChanged(mode);
    }
}

void DocumentView::setZoomLevel(qreal zoom)
{
    if (zoom <= 0) return; // Invalid zoom
    if (!qFuzzyCompare(d->zoomLevel, zoom)) {
        d->zoomLevel = zoom;
        d->zoomMode = CustomZoom; // Explicitly set zoom means custom mode now
        if (d->document) {
            d->updateScrollBars();
            viewport()->update();
        }
        LOG_DEBUG("Zoom level set to " << zoom);
        emit zoomLevelChanged(zoom);
    }
}

qreal DocumentView::zoomLevel() const
{
    return d->zoomLevel;
}

void DocumentView::goToPage(int pageIndex)
{
    if (!d->document) return;
    int pageCount = d->document->pageCount();
    if (pageIndex < 0 || pageIndex >= pageCount) return;

    int oldPageIndex = d->currentPageIndex;
    d->currentPageIndex = pageIndex;

    // Update document's current page index
    d->document->setCurrentPageIndex(pageIndex);

    // Update zoom if in FitPage/Width mode
    if (d->zoomMode == FitPage || d->zoomMode == FitWidth) {
        d->updateZoomForMode();
    }

    // Scroll to the new page
    QSize pageSize = d->calculatePageSizePixels(pageIndex);
    int targetY = 0;
    for (int i = 0; i < pageIndex; ++i) {
        targetY += d->calculatePageSizePixels(i).height() + d->pageSpacing;
    }
    // Calculate the target position to center the page vertically if possible
    int viewHeight = viewport()->height();
    int scrollY = targetY - (viewHeight - pageSize.height()) / 2;
    scrollY = qMax(0, scrollY); // Ensure not negative

    verticalScrollBar()->setValue(scrollY);

    LOG_DEBUG("Navigated to page " << pageIndex);
    emit currentPageChanged(pageIndex);
    viewport()->update(); // Repaint
}

int DocumentView::currentPageIndex() const
{
    return d->currentPageIndex;
}

int DocumentView::pageCount() const
{
    return d->document ? d->document->pageCount() : 0;
}

void DocumentView::rotateView(int degrees)
{
    if (degrees % 90 != 0) return; // Only allow 90-degree increments
    int normalizedDegrees = ((degrees % 360) + 360) % 360; // Normalize to 0-359
    if (d->rotation != normalizedDegrees) {
        d->rotation = normalizedDegrees;
        LOG_DEBUG("View rotation changed to " << degrees << " degrees.");
        d->updateScrollBars(); // Rotation changes page sizes
        viewport()->update();
        emit viewRotated(degrees);
    }
}

void DocumentView::setPageSpacing(int spacing)
{
    if (d->pageSpacing != spacing) {
        d->pageSpacing = spacing;
        LOG_DEBUG("Page spacing changed to " << spacing << " pixels.");
        d->updateScrollBars(); // Spacing affects total document size
        viewport()->update();
        emit pageSpacingChanged(spacing);
    }
}

void DocumentView::paintEvent(QPaintEvent* event)
{
    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    if (!d->document) {
        // Draw a blank background
        painter.fillRect(event->rect(), palette().window());
        return;
    }

    // Draw background
    painter.fillRect(painter.viewport(), Settings::instance().value<QColor>("Display/BackgroundColor", Qt::white));

    // Iterate through visible pages and draw them
    int pageCount = d->document->pageCount();
    int firstVisiblePage = -1;
    int lastVisiblePage = -1;

    // Determine which pages are visible based on scroll offset and viewport size
    QRectF viewportRect = QRectF(d->documentOffset, viewport()->size());
    int currentY = 0;
    
    // --- Draw Selection Rectangle ---
    if (!d->currentSelectionRect.isEmpty()) {
        // The selection rect is in document coordinates. Transform it to viewport coordinates for painting.
        QRectF viewportSelectionRect = d->currentSelectionRect.translated(-d->documentOffset.x(), -d->documentOffset.y());

        // Draw a semi-transparent overlay for the selection
        painter.save();
        painter.setPen(QPen(Qt::blue, 1, Qt::DashLine));
        painter.setBrush(QColor(0, 0, 255, 50)); // Semi-transparent blue
        painter.drawRect(viewportSelectionRect);
        painter.restore();
    }

    for (int i = 0; i < pageCount; ++i) {
        QSize pageSize = d->calculatePageSizePixels(i);
        QRectF pageRect(0, currentY, pageSize.width(), pageSize.height());

        if (pageRect.intersects(viewportRect)) {
            if (firstVisiblePage == -1) firstVisiblePage = i;
            lastVisiblePage = i;

            // Draw page background
            painter.fillRect(pageRect.translated(-d->documentOffset), Qt::lightGray);

            // --- Attempt to Render Page Content ---
            // This is where the core rendering logic connects to the UI.
            // We need to get the page image.
            // 1. Check PageCache
            PageCache::CacheKey cacheKey;
            cacheKey.documentId = reinterpret_cast<quintptr>(d->document.data());
            cacheKey.pageIndex = i;
            cacheKey.zoomLevel = d->zoomLevel;
            cacheKey.rotation = d->rotation;
            cacheKey.targetSize = pageSize;

            QImage cachedImage = PageCache::instance().get(cacheKey);
            if (!cachedImage.isNull()) {
                // Use cached image
                painter.drawImage(pageRect.translated(-d->documentOffset).topLeft().toPoint(), cachedImage);
            } else {
                // No cache hit, request render via RenderThread
                // Check if a request is already pending for this exact state
                if (PageCache::instance().contains(cacheKey)) {
                     // A request might be in progress, draw a placeholder or wait indicator
                     painter.fillRect(pageRect.translated(-d->documentOffset), Qt::darkGray);
                     painter.setPen(Qt::white);
                     painter.drawText(pageRect.translated(-d->documentOffset), Qt::AlignCenter, tr("Loading..."));
                } else {
                     // Submit a new render request
                     RenderThread::RenderRequest request;
                     request.page = d->document->page(i);
                     request.targetSize = pageSize;
                     request.zoomLevel = d->zoomLevel;
                     request.rotation = d->rotation;
                     request.clipRect = QRectF(); // Render full page for now
                     request.highQuality = true; // Or determine based on zoom level
                     request.requestId = ++(d->renderRequestCounter); // Generate unique ID

                     // Store the request ID so we know which result is ours
                     d->currentRenderRequestId = request.requestId;

                     RenderThread::instance().submitRequest(request);
                     LOG_DEBUG("Submitted render request for page " << i << ", Request ID: " << request.requestId);

                     // Draw a placeholder while rendering
                     painter.fillRect(pageRect.translated(-d->documentOffset), Qt::darkGray);
                     painter.setPen(Qt::white);
                     painter.drawText(pageRect.translated(-d->documentOffset), Qt::AlignCenter, tr("Rendering..."));
                }
            }
            // --- End Rendering Logic ---
        }

        currentY += pageSize.height() + d->pageSpacing; // Move to next page position
    }

    // Draw selection rectangle if active
    if (d->isSelecting) {
        QRectF selRect = QRectF(d->selectionStartPoint, d->selectionEndPoint).normalized();
        selRect.translate(-d->documentOffset); // Adjust for scroll offset
        painter.setPen(QPen(Qt::blue, 1, Qt::DashLine));
        painter.setBrush(QColor(0, 0, 255, 50)); // Semi-transparent blue
        painter.drawRect(selRect);
    }

    // Draw page borders (optional, for debugging or visual clarity)
    // painter.setPen(Qt::gray);
    // for (int i = firstVisiblePage; i <= lastVisiblePage; ++i) {
    //     QSize pageSize = d->calculatePageSizePixels(i);
    //     int y = 0; // Recalculate Y position for page i
    //     for (int j = 0; j < i; ++j) y += d->calculatePageSizePixels(j).height() + d->pageSpacing;
    //     QRectF pageRect(0, y, pageSize.width(), pageSize.height());
    //     painter.drawRect(pageRect.translated(-d->documentOffset));
    // }
}

void DocumentView::resizeEvent(QResizeEvent* event)
{
    QAbstractScrollArea::resizeEvent(event);
    // Update scrollbars based on new viewport size
    d->updateScrollBars();
    // If zoom mode is FitPage/Width, recalculate zoom
    if (d->zoomMode == FitPage || d->zoomMode == FitWidth) {
        d->updateZoomForMode();
    }
    LOG_DEBUG("DocumentView resized to " << event->size());
}

void DocumentView::wheelEvent(QWheelEvent* event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        // Zoom with Ctrl + Wheel
        qreal delta = event->angleDelta().y() / 120.0; // Usually +/- 1
        qreal factor = 1.1; // Zoom factor per step
        if (delta < 0) factor = 1.0 / factor; // Zoom out

        setZoomLevel(zoomLevel() * factor);
        event->accept(); // Handle the event here
    } else {
        // Default scroll behavior
        QAbstractScrollArea::wheelEvent(event);
    }
}

void DocumentView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        if (event->modifiers() & Qt::ShiftModifier) {
            // Start selection
            d->isSelecting = true;
            d->selectionStartPoint = d->selectionEndPoint = event->pos();
            viewport()->update(); // Trigger repaint to show selection rect
        } else {
            // Start panning
            d->isPanning = true;
            d->lastPanPoint = event->pos();
            setCursor(Qt::ClosedHandCursor);
        }
    } else if (event->button() == Qt::MiddleButton) {
        // Start panning with middle mouse
        d->isPanning = true;
        d->lastPanPoint = event->pos();
        setCursor(Qt::ClosedHandCursor);
    } else if (event->button() == Qt::BackButton) {
        // Go to previous page
        if (d->currentPageIndex > 0) {
            goToPage(d->currentPageIndex - 1);
        }
    } else if (event->button() == Qt::ForwardButton) {
        // Go to next page
        if (d->document && d->currentPageIndex < d->document->pageCount() - 1) {
            goToPage(d->currentPageIndex + 1);
        }
    } else if (event->button() == Qt::LeftButton) {
        if (event->modifiers() & Qt::ShiftModifier) {
            // Start selection (could be text or rect, refine based on hit test)
            d->isSelecting = true;
            d->selectionStartPoint = event->pos(); // Store viewport coordinates
            d->selectionEndPoint = event->pos();
            // Convert viewport point to document coordinates (PDF points)
            d->currentSelectionRect = QRectF(d->viewportToDocument(event->pos()), QSizeF(0, 0));
            viewport()->update(); // Trigger repaint to show selection rect
        } else {
            // Start panning (existing logic)
            d->isPanning = true;
            d->lastPanPoint = event->pos();
            setCursor(Qt::ClosedHandCursor);
        }
    }
    event->accept();
}

void DocumentView::mouseMoveEvent(QMouseEvent* event)
{
    if (d->isPanning) {
        QPoint delta = event->pos() - d->lastPanPoint;
        d->documentOffset += delta;
        d->documentOffset.setX(qMax(0, d->documentOffset.x()));
        d->documentOffset.setY(qMax(0, d->documentOffset.y()));
        // Update scrollbars to reflect new offset
        horizontalScrollBar()->setValue(d->documentOffset.x());
        verticalScrollBar()->setValue(d->documentOffset.y());
        d->lastPanPoint = event->pos();
        viewport()->update(); // Repaint
    } else if (d->isSelecting) {
        d->selectionEndPoint = event->pos(); // Update endpoint in viewport coordinates
        // Calculate selection rectangle in document coordinates
        QPointF startDoc = d->viewportToDocument(d->selectionStartPoint);
        QPointF endDoc = d->viewportToDocument(event->pos());
        d->currentSelectionRect = QRectF(startDoc, endDoc).normalized();

        // --- Text Selection Logic ---
        if (d->document && d->currentPageIndex >= 0 && d->currentPageIndex < d->document->pageCount()) {
            Page* currentPage = d->document->page(d->currentPageIndex);
            if (currentPage) {
                // Cast to PdfPage to access text functions (or use a generic text interface if available on Page)
                // For now, assume PdfPage or that Page has a text() method (it does now via PdfPage implementation).
                // The challenge is mapping the *rectangular* selection to *text*.
                // Poppler::Page::textList() gives text boxes. We need to find boxes overlapping the selection rect.
                // This is complex. A simpler approach for now is to get ALL text from the page
                // and maybe highlight the selected area visually without extracting specific text words yet.
                // Or, use Poppler::Page::text(const QRectF&) which gets text from a specific area.
                // This is more promising for rectangular selection.

                // Convert the selection rectangle (in document/PDF coordinates) to the page's coordinate system
                // (which for PdfPage, should be the same as document coordinates if page is at origin).
                // If pages are offset within a spread view mode, this conversion gets trickier.
                // For SinglePage mode, this should be fine.
                QRectF pageSelectionRect = d->currentSelectionRect; // Assuming 1:1 mapping for single page view

                // Get text from the selected rectangle on the current page
                // This requires casting to PdfPage to access its popplerPage()
                if (auto* pdfPage = dynamic_cast<PdfPage*>(currentPage)) {
                    Poppler::Page* popplerPage = pdfPage->popplerPage();
                    if (popplerPage) {
                        // Use Poppler's text extraction for the rectangle
                        d->selectedText = popplerPage->text(pageSelectionRect);
                        LOG_DEBUG("Selected text from rect: " << pageSelectionRect << ", length: " << d->selectedText.length());
                    }
                } else {
                    // Fallback if not a PdfPage, use the generic Page::text() and maybe filter based on bounds
                    // This is less accurate for rectangular selection.
                    LOG_WARN("DocumentView: Current page is not a PdfPage, using generic text method for selection.");
                    d->selectedText = currentPage->text(); // Gets all text, not just from rect
                }
            }
        }
        viewport()->update(); // Repaint to update selection rect
    }
    event->accept();
}

void DocumentView::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        if (d->isPanning) {
            d->isPanning = false;
            setCursor(Qt::ArrowCursor);
        } else if (d->isSelecting) {
            d->isSelecting = false;
            // Finalize selection based on the drawn rectangle
            QRectF finalSelectionRect = d->currentSelectionRect;
            QString finalSelectedText = d->selectedText;

            // Check if the selection is large enough to be meaningful
            if (finalSelectionRect.width() > 2.0 || finalSelectionRect.height() > 2.0) { // Arbitrary small threshold
                if (!finalSelectedText.isEmpty()) {
                    // Copy the selected text to the clipboard
                    QClipboard* clipboard = QApplication::clipboard();
                    if (clipboard) {
                        // Use the central Clipboard manager if available, or standard QApplication clipboard
                        // Clipboard::instance().setText(finalSelectedText); // If using our manager
                        clipboard->setText(finalSelectedText); // Standard Qt clipboard
                        LOG_INFO("Copied selected text to clipboard (length: " << finalSelectedText.length() << ").");
                    }
                } else {
                    LOG_DEBUG("Selection rect was drawn but no text found within it.");
                }
            } else {
                LOG_DEBUG("Selection rect was too small, ignoring.");
            }

            // Clear selection visuals
            d->currentSelectionRect = QRectF();
            d->selectedText.clear();
            viewport()->update();
        }
    }
    event->accept();
}

void DocumentView::keyPressEvent(QKeyEvent* event)
{
    bool handled = false;
    switch (event->key()) {
        case Qt::Key_Plus:
        case Qt::Key_Equal:
            if (event->modifiers() & Qt::ControlModifier) {
                zoomIn();
                handled = true;
            }
            break;
        case Qt::Key_Minus:
            if (event->modifiers() & Qt::ControlModifier) {
                zoomOut();
                handled = true;
            }
            break;
        case Qt::Key_0:
            if (event->modifiers() & Qt::ControlModifier) {
                setZoomMode(FitPage);
                handled = true;
            }
            break;
        case Qt::Key_1:
            if (event->modifiers() & Qt::ControlModifier) {
                setZoomMode(FitWidth);
                handled = true;
            }
            break;
        case Qt::Key_Space:
            // Toggle panning mode? Or just scroll down
            // For now, let's scroll down
            verticalScrollBar()->setValue(verticalScrollBar()->value() + viewport()->height() / 2);
            handled = true;
            break;
        case Qt::Key_PageUp:
            if (d->currentPageIndex > 0) {
                goToPage(d->currentPageIndex - 1);
            }
            handled = true;
            break;
        case Qt::Key_PageDown:
        case Qt::Key_Space: // If not handled as scroll above
            if (d->document && d->currentPageIndex < d->document->pageCount() - 1) {
                goToPage(d->currentPageIndex + 1);
            }
            handled = true;
            break;
        case Qt::Key_Home:
            if (d->document) {
                goToPage(0);
            }
            handled = true;
            break;
        case Qt::Key_End:
            if (d->document) {
                goToPage(d->document->pageCount() - 1);
            }
            handled = true;
            break;
        // Add more keys as needed
    }
    
    // Add copy command
    if (event->matches(QKeySequence::Copy)) { // Checks for Ctrl+C, Cmd+C, etc.
        if (!d->selectedText.isEmpty()) {
            QClipboard* clipboard = QApplication::clipboard();
            if (clipboard) {
                clipboard->setText(d->selectedText);
                LOG_INFO("Copied selected text to clipboard via keyboard shortcut (length: " << d->selectedText.length() << ").");
            }
            event->accept();
            return;
        }
    }

    if (!handled) {
        QAbstractScrollArea::keyPressEvent(event); // Let base class handle other keys
    } else {
        event->accept();
    }
}

void DocumentView::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu contextMenu(this);

    QAction* zoomInAction = contextMenu.addAction(tr("Zoom In"));
    QAction* zoomOutAction = contextMenu.addAction(tr("Zoom Out"));
    contextMenu.addSeparator();
    QAction* fitPageAction = contextMenu.addAction(tr("Fit Page"));
    QAction* fitWidthAction = contextMenu.addAction(tr("Fit Width"));
    contextMenu.addSeparator();
    QAction* copyAction = contextMenu.addAction(tr("Copy Selection")); // Only if selection exists
    QAction* selectAllAction = contextMenu.addAction(tr("Select All")); // Select entire page/view?

    QAction* selectedAction = contextMenu.exec(event->globalPos());

    if (selectedAction == zoomInAction) {
        zoomIn();
    } else if (selectedAction == zoomOutAction) {
        zoomOut();
    } else if (selectedAction == fitPageAction) {
        setZoomMode(FitPage);
    } else if (selectedAction == fitWidthAction) {
        setZoomMode(FitWidth);
    } else if (selectedAction == copyAction) {
        // Copy current selection
        // Selection::instance().copyToClipboard(); // Hypothetical call
    } else if (selectedAction == selectAllAction) {
        // Select all content in view or current page
        // Selection::instance().selectAllInView(); // Hypothetical call
    }
}

void DocumentView::zoomIn()
{
    setZoomLevel(zoomLevel() * 1.2); // Increase by ~20%
}

void DocumentView::zoomOut()
{
    setZoomLevel(zoomLevel() / 1.2); // Decrease by ~20%
}

} // namespace QuantilyxDoc