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
#include "../core/Document.h"
#include "../core/Logger.h"
#include <QLabel>
#include <QProgressBar>
#include <QSpinBox>
#include <QSlider>
#include <QToolButton>
#include <QHBoxLayout>
#include <QTimer>
#include <QApplication>
#include <QPainter>
#include <QDebug>

namespace QuantilyxDoc {

class StatusBar::Private {
public:
    Private(StatusBar* q_ptr) : q(q_ptr), document(nullptr), currentPageIndex(0), zoomLevel(1.0), rotation(0) {}

    StatusBar* q;
    QPointer<Document> document; // Use QPointer for safety

    // Status bar widgets
    QLabel* messageLabel; // For temporary messages
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

    int currentPageIndex;
    qreal zoomLevel;
    int rotation; // 0, 90, 180, 270

    // Temporary message timer
    QTimer* messageTimer;

    // Helper to setup initial widgets
    void setupWidgets();
};

void StatusBar::Private::setupWidgets()
{
    // --- Message Label (Standard QStatusBar item) ---
    // This is handled by the base QStatusBar class, but we can customize its behavior if needed.
    // For now, we'll just use the standard showMessage/clearMessage.

    // --- Page Controls ---
    pageLabel = new QLabel(tr("Page:"), q);
    pageLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    pageLabel->setMinimumWidth(40); // Ensure consistent width

    pageSpinBox = new QSpinBox(q);
    pageSpinBox->setRange(1, 1); // Will be updated by document
    pageSpinBox->setValue(1);
    pageSpinBox->setMinimumWidth(60); // Ensure consistent width
    pageSpinBox->setAlignment(Qt::AlignCenter);

    pageCountLabel = new QLabel(tr("/ 1"), q); // Will be updated by document
    pageCountLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    pageCountLabel->setMinimumWidth(40); // Ensure consistent width

    // --- Zoom Controls ---
    zoomLabel = new QLabel(tr("Zoom:"), q);
    zoomLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    zoomLabel->setMinimumWidth(40);

    zoomSlider = new QSlider(Qt::Horizontal, q);
    zoomSlider->setRange(10, 500); // 10% to 500%
    zoomSlider->setValue(100); // Default 100%
    zoomSlider->setTickPosition(QSlider::TicksBelow);
    zoomSlider->setTickInterval(50);
    zoomSlider->setMinimumWidth(100); // Ensure consistent width

    zoomPercentLabel = new QLabel(tr("100%"), q);
    zoomPercentLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    zoomPercentLabel->setMinimumWidth(40);

    // --- Rotation Controls ---
    rotationLabel = new QLabel(tr("Rotation:"), q);
    rotationLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    rotationLabel->setMinimumWidth(60);

    rotateLeftButton = new QToolButton(q);
    rotateLeftButton->setText(tr("↺")); // Unicode for counter-clockwise arrow
    rotateLeftButton->setToolTip(tr("Rotate Left (90° CCW)"));
    rotateLeftButton->setAutoRaise(true);

    rotateRightButton = new QToolButton(q);
    rotateRightButton->setText(tr("↻")); // Unicode for clockwise arrow
    rotateRightButton->setToolTip(tr("Rotate Right (90° CW)"));
    rotateRightButton->setAutoRaise(true);

    rotationValueLabel = new QLabel(tr("0°"), q);
    rotationValueLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    rotationValueLabel->setMinimumWidth(30);

    // Add widgets to status bar using insertWidget/addPermanentWidget
    // Permanent widgets stay on the right
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

    // Connect signals
    QObject::connect(pageSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
                     q, &StatusBar::onPageSpinBoxValueChanged);

    QObject::connect(zoomSlider, &QSlider::valueChanged,
                     q, &StatusBar::onZoomSliderValueChanged);

    QObject::connect(rotateLeftButton, &QToolButton::clicked,
                     q, &StatusBar::onRotateLeftClicked);

    QObject::connect(rotateRightButton, &QToolButton::clicked,
                     q, &StatusBar::onRotateRightClicked);

    LOG_DEBUG("StatusBar widgets initialized.");
}

StatusBar::StatusBar(QWidget* parent)
    : QStatusBar(parent)
    , d(new Private(this))
{
    d->setupWidgets();

    // Initialize with no document
    setDocument(nullptr);

    LOG_INFO("StatusBar initialized.");
}

StatusBar::~StatusBar()
{
    LOG_INFO("StatusBar destroyed.");
}

void StatusBar::setDocument(Document* doc)
{
    if (d->document == doc) return;

    // Disconnect from old document signals if necessary
    if (d->document) {
        // disconnect(d->document, ...);
    }

    d->document = doc; // Use QPointer

    if (doc) {
        // Connect to new document signals
        connect(doc, &Document::closed, this, [this]() {
            setDocument(nullptr); // Clear if document is closed elsewhere
        });
        connect(doc, &Document::currentPageChanged, this, [this](int index) {
            setCurrentPage(index);
        });
        // Connect to page count changes to update spinbox range
        // connect(doc, &Document::pageCountChanged, this, [this]() { updatePageCountLabel(); });

        // Update UI based on new document state
        d->pageSpinBox->setRange(1, doc->pageCount());
        d->pageSpinBox->setValue(doc->currentPageIndex() + 1); // 0-based to 1-based
        updatePageCountLabel();
        showMessage(tr("Loaded: %1").arg(doc->filePath()), 3000); // Show for 3 seconds
    } else {
        d->pageSpinBox->setRange(1, 1);
        d->pageSpinBox->setValue(1);
        updatePageCountLabel();
        showMessage(tr("Ready"), 2000);
    }

    updateStatus();
    LOG_DEBUG("StatusBar set to document: " << (doc ? doc->filePath() : "nullptr"));
}

Document* StatusBar::document() const
{
    return d->document.data(); // Returns nullptr if document was deleted
}

void StatusBar::setCurrentPage(int index)
{
    if (d->document) {
        int pageCount = d->document->pageCount();
        if (index >= 0 && index < pageCount) {
            int oldPage = d->currentPageIndex;
            d->currentPageIndex = index;
            d->pageSpinBox->setValue(index + 1); // Update spinbox (1-based)

            if (oldPage != index) {
                emit pageChanged(index);
                LOG_DEBUG("StatusBar current page updated to " << index);
            }
        }
    } else {
        d->currentPageIndex = index;
        d->pageSpinBox->setValue(index + 1); // Update spinbox (1-based)
    }
}

int StatusBar::currentPage() const
{
    return d->currentPageIndex;
}

void StatusBar::setZoomLevel(qreal zoom)
{
    if (zoom > 0) {
        qreal oldZoom = d->zoomLevel;
        d->zoomLevel = zoom;
        int zoomPercent = qRound(zoom * 100);
        d->zoomSlider->setValue(zoomPercent);
        updateZoomLabel();

        if (!qFuzzyCompare(oldZoom, zoom)) {
            emit zoomLevelChanged(zoom);
            LOG_DEBUG("StatusBar zoom level updated to " << zoom);
        }
    }
}

qreal StatusBar::zoomLevel() const
{
    return d->zoomLevel;
}

void StatusBar::setRotation(int degrees)
{
    if (degrees % 90 == 0) { // Only allow 90-degree increments
        int oldRotation = d->rotation;
        int normalizedDegrees = ((degrees % 360) + 360) % 360; // Normalize to 0-359
        d->rotation = normalizedDegrees;
        updateRotationLabel();

        if (oldRotation != normalizedDegrees) {
            emit rotationChanged(normalizedDegrees);
            LOG_DEBUG("StatusBar rotation updated to " << normalizedDegrees);
        }
    }
}

int StatusBar::rotation() const
{
    return d->rotation;
}

void StatusBar::showMessage(const QString& message, int timeoutMs)
{
    QStatusBar::showMessage(message, timeoutMs);
    LOG_DEBUG("StatusBar message: " << message << " (timeout: " << timeoutMs << "ms)");
}

void StatusBar::clearMessage()
{
    QStatusBar::clearMessage();
    LOG_DEBUG("StatusBar message cleared.");
}

void StatusBar::setPageControlsVisible(bool visible)
{
    d->pageLabel->setVisible(visible);
    d->pageSpinBox->setVisible(visible);
    d->pageCountLabel->setVisible(visible);
    LOG_DEBUG("StatusBar page controls set to " << (visible ? "visible" : "hidden"));
}

void StatusBar::setZoomControlsVisible(bool visible)
{
    d->zoomLabel->setVisible(visible);
    d->zoomSlider->setVisible(visible);
    d->zoomPercentLabel->setVisible(visible);
    LOG_DEBUG("StatusBar zoom controls set to " << (visible ? "visible" : "hidden"));
}

void StatusBar::setRotationControlsVisible(bool visible)
{
    d->rotationLabel->setVisible(visible);
    d->rotateLeftButton->setVisible(visible);
    d->rotateRightButton->setVisible(visible);
    d->rotationValueLabel->setVisible(visible);
    LOG_DEBUG("StatusBar rotation controls set to " << (visible ? "visible" : "hidden"));
}

void StatusBar::updateStatus()
{
    if (d->document) {
        // Update page count and current page display
        d->pageSpinBox->setRange(1, d->document->pageCount());
        d->pageSpinBox->setValue(d->document->currentPageIndex() + 1);
        updatePageCountLabel();

        // Other status updates could happen here if needed
    }
    LOG_DEBUG("StatusBar status updated.");
}

void StatusBar::onPageSpinBoxValueChanged(int value)
{
    // Value is 1-based, emit 0-based index
    emit pageChanged(value - 1);
    LOG_DEBUG("StatusBar page spinbox changed to " << value);
}

void StatusBar::onZoomSliderValueChanged(int value)
{
    // Value is percentage, convert to factor
    qreal newZoom = value / 100.0;
    setZoomLevel(newZoom);
    updateZoomLabel(); // Ensure label is updated
    LOG_DEBUG("StatusBar zoom slider changed to " << value << "%");
}

void StatusBar::onRotateLeftClicked()
{
    int newRotation = (d->rotation - 90 + 360) % 360;
    setRotation(newRotation);
    LOG_DEBUG("StatusBar rotate left clicked, new rotation: " << newRotation);
}

void StatusBar::onRotateRightClicked()
{
    int newRotation = (d->rotation + 90) % 360;
    setRotation(newRotation);
    LOG_DEBUG("StatusBar rotate right clicked, new rotation: " << newRotation);
}

void StatusBar::updatePageCountLabel()
{
    int count = d->document ? d->document->pageCount() : 0;
    d->pageCountLabel->setText(tr("/ %1").arg(count));
    LOG_DEBUG("StatusBar page count label updated to / " << count);
}

void StatusBar::updateZoomLabel()
{
    int zoomPercent = qRound(d->zoomLevel * 100);
    d->zoomPercentLabel->setText(tr("%1%").arg(zoomPercent));
    LOG_DEBUG("StatusBar zoom label updated to " << zoomPercent << "%");
}

void StatusBar::updateRotationLabel()
{
    d->rotationValueLabel->setText(tr("%1°").arg(d->rotation));
    LOG_DEBUG("StatusBar rotation label updated to " << d->rotation << "°");
}

} // namespace QuantilyxDoc