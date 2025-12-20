/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#ifndef QUANTILYX_STATUSBAR_H
#define QUANTILYX_STATUSBAR_H

#include <QStatusBar>
#include <QLabel>
#include <QProgressBar>
#include <QSpinBox>
#include <QSlider>
#include <QToolButton>
#include <QTimer>
#include <memory>

namespace QuantilyxDoc {

class Document;
class Page;

/**
 * @brief Custom status bar displaying document information, page numbers, zoom level, etc.
 *
 * Provides real-time feedback on document state, current page, zoom level,
 * rendering progress, and other relevant information. Integrates with
 * Document, DocumentView, and other core systems.
 */
class StatusBar : public QStatusBar
{
    Q_OBJECT

public:
    /**
     * @brief Constructor.
     * @param parent Parent widget (usually the MainWindow).
     */
    explicit StatusBar(QWidget* parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~StatusBar() override;

    /**
     * @brief Set the current document associated with the status bar.
     * Updates displayed information like page count, path, etc.
     * @param doc The document, or nullptr to clear.
     */
    void setDocument(Document* doc);

    /**
     * @brief Get the currently associated document.
     * @return Pointer to the document, or nullptr.
     */
    Document* document() const;

    /**
     * @brief Set the current page index.
     * Updates the page number display.
     * @param index The 0-based page index.
     */
    void setCurrentPage(int index);

    /**
     * @brief Get the current page index.
     * @return The 0-based page index.
     */
    int currentPage() const;

    /**
     * @brief Set the current zoom level (e.g., 1.0 for 100%, 1.5 for 150%).
     * Updates the zoom display.
     * @param zoom The zoom factor.
     */
    void setZoomLevel(qreal zoom);

    /**
     * @brief Get the current zoom level.
     * @return The zoom factor.
     */
    qreal zoomLevel() const;

    /**
     * @brief Set the current rotation (0, 90, 180, 270).
     * Updates the rotation display.
     * @param degrees The rotation in degrees.
     */
    void setRotation(int degrees);

    /**
     * @brief Get the current rotation.
     * @return The rotation in degrees.
     */
    int rotation() const;

    /**
     * @brief Show a temporary message on the status bar.
     * Overrides QStatusBar::showMessage to potentially add custom formatting/timeouts.
     * @param message The message to show.
     * @param timeoutMs How long to show the message in milliseconds (0 for no timeout).
     */
    void showMessage(const QString& message, int timeoutMs = 0) override;

    /**
     * @brief Clear the status bar message.
     * Overrides QStatusBar::clearMessage.
     */
    void clearMessage() override;

    /**
     * @brief Show or hide the page number controls (spinbox, label).
     * @param visible Whether to show the controls.
     */
    void setPageControlsVisible(bool visible);

    /**
     * @brief Show or hide the zoom controls (slider, label).
     * @param visible Whether to show the controls.
     */
    void setZoomControlsVisible(bool visible);

    /**
     * @brief Show or hide the rotation controls (button, label).
     * @param visible Whether to show the controls.
     */
    void setRotationControlsVisible(bool visible);

    /**
     * @brief Update the status bar based on the current state of the associated document and view.
     * This is called internally when states change, but can be called externally if needed.
     */
    void updateStatus();

signals:
    /**
     * @brief Emitted when the user changes the current page via the status bar controls.
     * @param pageIndex The new 0-based page index.
     */
    void pageChanged(int pageIndex);

    /**
     * @brief Emitted when the user changes the zoom level via the status bar controls.
     * @param zoomLevel The new zoom factor.
     */
    void zoomLevelChanged(qreal zoomLevel);

    /**
     * @brief Emitted when the user changes the rotation via the status bar controls.
     * @param degrees The new rotation in degrees.
     */
    void rotationChanged(int degrees);

private slots:
    void onPageSpinBoxValueChanged(int value);
    void onZoomSliderValueChanged(int value);
    void onRotateLeftClicked();
    void onRotateRightClicked();

private:
    class Private;
    std::unique_ptr<Private> d;

    // Helper to update the text of the page count label
    void updatePageCountLabel();
    // Helper to update the text of the zoom label
    void updateZoomLabel();
    // Helper to update the text of the rotation label
    void updateRotationLabel();
};

} // namespace QuantilyxDoc

#endif // QUANTILYX_STATUSBAR_H