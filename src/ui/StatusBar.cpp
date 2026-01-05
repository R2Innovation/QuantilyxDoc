/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "StatusBar.h"
#include "../core/Document.h" // For document-specific state
#include "../core/Logger.h"
#include <QLabel>
#include <QProgressBar>
#include <QSpinBox>
#include <QSlider>
#include <QToolButton>
#include <QHBoxLayout>
#include <QTimer>
#include <QApplication>
#include <QDesktopWidget>
#include <QPainter>
#include <QStyleOption>
#include <QDebug>

namespace QuantilyxDoc {

class StatusBar::Private {
public:
    Private(StatusBar* q_ptr)
        : q(q_ptr), document(nullptr), currentPageIndex(-1), zoomLevelVal(1.0), rotationVal(0), progressVal(-1) {}

    StatusBar* q;
    QPointer<Document> document; // Use QPointer for safety

    // Status bar widgets
    QLabel* messageLabel; // For temporary messages (handled by base QStatusBar)
    QLabel* pageLabel; // Static "Page:" label
    QSpinBox* pageSpinBox; // For selecting page
    QLabel* pageCountLabel; // Displays total pages (e.g., "/ 10")
    QLabel* zoomLabel; // Static "Zoom:" label
    QSlider* zoomSlider; // For adjusting zoom (10% to 500%)
    QLabel* zoomPercentLabel; // Displays current zoom as percentage (e.g., "100%")
    QLabel* rotationLabel; // Static "Rotation:" label
    QToolButton* rotateLeftButton; // Rotate -90 degrees
    QToolButton* rotateRightButton; // Rotate +90 degrees
    QLabel* rotationValueLabel; // Displays current rotation (e.g., "0°")
    QProgressBar* progressBar; // For showing operation progress

    int currentPageIndex;
    qreal zoomLevelVal;
    int rotationVal;
    int progressVal; // -1 means hidden

    void createLayout() {
        // The status bar uses its own internal layout. We add widgets using addPermanentWidget/addWidget.
        // Labels
        pageLabel = new QLabel(tr("Page:"), q);
        pageLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        pageLabel->setMinimumWidth(40); // Ensure consistent width

        pageCountLabel = new QLabel(tr("/ 1"), q); // Will be updated by document
        pageCountLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        pageCountLabel->setMinimumWidth(40); // Ensure consistent width

        // SpinBox for page number
        pageSpinBox = new QSpinBox(q);
        pageSpinBox->setRange(1, 1); // Will be updated by document
        pageSpinBox->setValue(1);
        pageSpinBox->setMinimumWidth(60); // Ensure consistent width
        pageSpinBox->setAlignment(Qt::AlignCenter);
        // Connect spinbox signal to emit status bar signal
        QObject::connect(pageSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
                         q, [this](int value) { emit q->pageChanged(value - 1); }); // Emit 0-based index

        // Zoom Slider and Label
        zoomLabel = new QLabel(tr("Zoom:"), q);
        zoomLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        zoomLabel->setMinimumWidth(40);

        zoomSlider = new QSlider(Qt::Horizontal, q);
        zoomSlider->setRange(10, 500); // 10% to 500%
        zoomSlider->setValue(100); // Default 100%
        zoomSlider->setTickPosition(QSlider::TicksBelow);
        zoomSlider->setTickInterval(50);
        zoomSlider->setMinimumWidth(100); // Ensure consistent width
        QObject::connect(zoomSlider, &QSlider::valueChanged,
                         q, [this](int value) {
                             qreal newZoom = value / 100.0;
                             setZoomLevel(newZoom);
                             emit q->zoomLevelChanged(newZoom);
                         });

        zoomPercentLabel = new QLabel(tr("100%"), q);
        zoomPercentLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        zoomPercentLabel->setMinimumWidth(40);

        // Rotation Buttons and Label
        rotationLabel = new QLabel(tr("Rotation:"), q);
        rotationLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        rotationLabel->setMinimumWidth(60);

        rotateLeftButton = new QToolButton(q);
        rotateLeftButton->setText(tr("↺")); // Unicode for counter-clockwise arrow
        rotateLeftButton->setToolTip(tr("Rotate Left (90° CCW)"));
        rotateLeftButton->setAutoRaise(true);
        QObject::connect(rotateLeftButton, &QToolButton::clicked,
                         q, [this]() {
                             int newRotation = (rotationVal - 90 + 360) % 360; // Normalize to 0-359
                             setRotation(newRotation);
                             emit q->rotationChanged(newRotation);
                         });

        rotateRightButton = new QToolButton(q);
        rotateRightButton->setText(tr("↻")); // Unicode for clockwise arrow
        rotateRightButton->setToolTip(tr("Rotate Right (90° CW)"));
        rotateRightButton->setAutoRaise(true);
        QObject::connect(rotateRightButton, &QToolButton::clicked,
                         q, [this]() {
                             int newRotation = (rotationVal + 90) % 360; // Normalize to 0-359
                             setRotation(newRotation);
                             emit q->rotationChanged(newRotation);
                         });

        rotationValueLabel = new QLabel(tr("0°"), q);
        rotationValueLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        rotationValueLabel->setMinimumWidth(30);

        // Progress Bar (initially hidden)
        progressBar = new QProgressBar(q);
        progressBar->setVisible(false); // Hidden by default
        progressBar->setMaximumWidth(150); // Limit width
        progressBar->setTextVisible(true);

        // Add widgets to status bar using addPermanentWidget to place them on the right
        q->addPermanentWidget(pageLabel);
        q->addPermanentWidget(pageSpinBox);
        q->addPermanentWidget(pageCountLabel);

        q->addPermanentWidget(zoomLabel);
        q->addPermanentWidget(zoomSlider);
        q->addPermanentWidget(zoomPercentLabel);

        q->addPermanentWidget(rotationLabel);
        q->addPermanentWidget(rotateLeftButton);
        q->addPermanentWidget(rotateRightButton);
        q->addPermanentWidget(rotationValueLabel);

        // Add progress bar, potentially on the left if needed, or keep it flexible.
        // For now, let's add it as a permanent widget on the right, after rotation.
        // q->addPermanentWidget(progressBar); // Uncomment if desired on the right

        // Or, add it near the left, next to the default message area
        q->insertWidget(0, progressBar); // Insert at index 0, pushing other permanent widgets right

        LOG_DEBUG("StatusBar: Custom widgets initialized.");
    }

    void updatePageCountLabel() {
        if (document) {
            pageCountLabel->setText(tr("/ %1").arg(document->pageCount()));
        } else {
            pageCountLabel->setText(tr("/ 0"));
        }
    }

    void updateZoomLabel() {
        int percent = qRound(zoomLevelVal * 100);
        zoomPercentLabel->setText(tr("%1%").arg(percent));
        zoomSlider->setValue(percent); // Sync slider to value
    }

    void updateRotationLabel() {
        rotationValueLabel->setText(tr("%1°").arg(rotationVal));
    }

    void setDocumentInternal(Document* doc) {
        // Disconnect from old document signals if necessary
        if (document) {
            // Example disconnections if signals were connected:
            // disconnect(document, &Document::currentPageChanged, q, &StatusBar::setCurrentPage);
            // disconnect(document, &Document::pageCountChanged, q, [this](){ updatePageCountLabel(); });
        }

        document = doc; // Use QPointer assignment

        if (doc) {
            // Connect to new document signals
            // connect(doc, &Document::currentPageChanged, q, &StatusBar::setCurrentPage);
            // connect(doc, &Document::pageCountChanged, q, [this](){ updatePageCountLabel(); });

            // Update UI based on new document state
            pageSpinBox->setRange(1, doc->pageCount());
            pageSpinBox->setValue(doc->currentPageIndex() + 1); // 0-based index to 1-based spinbox
            updatePageCountLabel();
            q->showMessage(tr("Loaded: %1").arg(doc->filePath()), 3000); // Show for 3 seconds
        } else {
            // Clear UI if no document
            pageSpinBox->setRange(1, 1);
            pageSpinBox->setValue(1);
            updatePageCountLabel();
            q->showMessage(tr("Ready"), 2000);
        }
        LOG_DEBUG("StatusBar::setDocumentInternal: Set document to " << (doc ? doc->filePath() : "nullptr"));
    }

    void setCurrentPageInternal(int index) {
        if (document) {
            int pageCount = document->pageCount();
            if (index >= 0 && index < pageCount) {
                int oldPage = currentPageIndex;
                currentPageIndex = index;
                pageSpinBox->setValue(index + 1); // Update spinbox (1-based)

                if (oldPage != index) {
                    emit q->pageChanged(index);
                    LOG_DEBUG("StatusBar current page updated to " << index);
                }
            } else {
                LOG_WARN("StatusBar::setCurrentPageInternal: Index " << index << " is out of range for document with " << pageCount << " pages.");
            }
        } else {
            // If no document, still update the spinbox value and internal state
            currentPageIndex = index;
            pageSpinBox->setValue(index + 1); // Update spinbox (1-based)
        }
    }

    void setZoomLevelInternal(qreal zoom) {
        if (zoom > 0) {
            qreal oldZoom = zoomLevelVal;
            zoomLevelVal = zoom;
            updateZoomLabel(); // Update slider and label

            if (!qFuzzyCompare(oldZoom, zoom)) {
                emit q->zoomLevelChanged(zoom);
                LOG_DEBUG("StatusBar zoom level updated to " << zoom);
            }
        }
    }

    void setRotationInternal(int degrees) {
        if (degrees % 90 == 0) { // Only allow 90-degree increments
            int oldRotation = rotationVal;
            int normalizedDegrees = ((degrees % 360) + 360) % 360; // Normalize to 0-359
            rotationVal = normalizedDegrees;
            updateRotationLabel(); // Update label

            if (oldRotation != normalizedDegrees) {
                emit q->rotationChanged(normalizedDegrees);
                LOG_DEBUG("StatusBar rotation updated to " << normalizedDegrees);
            }
        }
    }

    void setProgressInternal(int value) {
        if (value >= 0 && value <= 100) {
            progressVal = value;
            progressBar->setValue(value);
            bool shouldBeVisible = (value < 100); // Hide when at 100% (operation finished)
            progressBar->setVisible(shouldBeVisible);
            if (value == 100) {
                 emit q->operationFinished(); // Emit if progress reaches 100%
            } else if (value == 0 && progressVal > 0) {
                 emit q->operationStarted(); // Emit if progress resets from a non-zero value
            }
            emit q->progressChanged(value); // Always emit progress
            LOG_DEBUG("StatusBar progress set to " << value << "%, visible: " << shouldBeVisible);
        } else if (value < 0) {
            // Negative value means hide the progress bar
            progressVal = -1;
            progressBar->setVisible(false);
            LOG_DEBUG("StatusBar progress bar hidden.");
        }
    }
};

StatusBar::StatusBar(QWidget* parent)
    : QStatusBar(parent)
    , d(new Private(this))
{
    d->createLayout();
    setDocument(nullptr); // Initialize with no document

    LOG_INFO("StatusBar initialized.");
}

StatusBar::~StatusBar()
{
    LOG_INFO("StatusBar destroyed.");
}

void StatusBar::setDocument(Document* doc)
{
    d->setDocumentInternal(doc);
}

Document* StatusBar::document() const
{
    return d->document.data(); // Returns nullptr if document was deleted
}

void StatusBar::setCurrentPage(int index)
{
    d->setCurrentPageInternal(index);
}

int StatusBar::currentPage() const
{
    return d->currentPageIndex;
}

void StatusBar::setZoomLevel(qreal zoom)
{
    if (zoom > 0) {
        d->setZoomLevelInternal(zoom);
    }
}

qreal StatusBar::zoomLevel() const
{
    return d->zoomLevelVal;
}

void StatusBar::setRotation(int degrees)
{
    if (degrees % 90 == 0) { // Only allow 90-degree increments
        d->setRotationInternal(degrees);
    }
}

int StatusBar::rotation() const
{
    return d->rotationVal;
}

void StatusBar::setProgress(int value)
{
    d->setProgressInternal(value);
}

int StatusBar::progress() const
{
    return d->progressVal;
}

void StatusBar::setProgressVisible(bool visible)
{
    d->progressBar->setVisible(visible);
    if (!visible && d->progressVal >= 0) {
        d->progressVal = -1; // Reset internal state if hidden
    }
    LOG_DEBUG("StatusBar progress bar visibility set to " << visible);
}

void StatusBar::setPageControlsVisible(bool visible)
{
    d->pageLabel->setVisible(visible);
    d->pageSpinBox->setVisible(visible);
    d->pageCountLabel->setVisible(visible);
    LOG_DEBUG("StatusBar page controls visibility set to " << visible);
}

void StatusBar::setZoomControlsVisible(bool visible)
{
    d->zoomLabel->setVisible(visible);
    d->zoomSlider->setVisible(visible);
    d->zoomPercentLabel->setVisible(visible);
    LOG_DEBUG("StatusBar zoom controls visibility set to " << visible);
}

void StatusBar::setRotationControlsVisible(bool visible)
{
    d->rotationLabel->setVisible(visible);
    d->rotateLeftButton->setVisible(visible);
    d->rotateRightButton->setVisible(visible);
    d->rotationValueLabel->setVisible(visible);
    LOG_DEBUG("StatusBar rotation controls visibility set to " << visible);
}

QString StatusBar::currentDocumentPath() const
{
    return d->document ? d->document->filePath() : QString();
}

int StatusBar::currentPageCount() const
{
    return d->document ? d->document->pageCount() : 0;
}

QString StatusBar::documentStatus() const
{
    // This could reflect the state of the current document (e.g., "Loading", "Rendering", "Ready", "Modified")
    // It might be updated by connecting to Document signals.
    if (d->document) {
        // Example: Check if document is loading, rendering, etc.
        // This requires signals from the Document or its associated View/RenderThread.
        // For now, a placeholder based on whether a document is loaded.
        return "Ready"; // Placeholder
    }
    return "No Document";
}

void StatusBar::setDocumentStatus(const QString& status)
{
    // This might set an internal status label or just influence the general message shown by the base QStatusBar.
    // For now, let's just log it or update a specific status label if one existed.
    LOG_DEBUG("StatusBar document status set to: " << status);
    // Could do: showMessage(status, 0); // Show indefinitely, or until another message
    // Or update an internal QLabel added specifically for status.
}

} // namespace QuantilyxDoc