/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#ifndef QUANTILYX_PROGRESSIVERENDERER_H
#define QUANTILYX_PROGRESSIVERENDERER_H

#include <QObject>
#include <QImage>
#include <QSize>
#include <QRectF>
#include <QHash>
#include <QQueue>
#include <QMutex>
#include <QElapsedTimer>
#include <memory>
#include <functional>

namespace QuantilyxDoc {

class Page;
class Document;
class RenderTask; // Forward declaration for internal task structure

/**
 * @brief Renders document pages progressively, starting with low quality and refining.
 * 
 * Loads and renders pages in multiple passes, starting with a fast, low-resolution
 * preview and then progressively increasing the quality. This provides a faster
 * initial visual feedback to the user compared to rendering the final quality
 * directly.
 */
class ProgressiveRenderer : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Structure holding details for a progressive rendering pass.
     */
    struct RenderPass {
        int passNumber;             // Pass index (0 = lowest quality, higher = better)
        QSize targetSize;           // Target size for this pass
        qreal zoomLevel;            // Zoom level for this pass
        int rotation;               // Rotation (0, 90, 180, 270)
        QRectF clipRect;            // Optional clipping rectangle
        bool isFinalPass;           // Is this the final, highest quality pass?
        QImage intermediateImage;   // Image buffer for this pass's output
        QElapsedTimer timer;        // Timer to track pass duration
    };

    /**
     * @brief Structure holding the result of a rendering pass.
     */
    struct PassResult {
        int passNumber;             // Which pass this result is from
        QImage image;               // The rendered image for this pass
        bool success;               // Whether the pass was successful
        QString errorMessage;       // Error message if success is false
        qint64 durationMs;          // Time taken for this pass in milliseconds
        bool isFinal;               // Is this the final pass result?
    };

    /**
     * @brief Constructor.
     * @param parent Parent object.
     */
    explicit ProgressiveRenderer(QObject* parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~ProgressiveRenderer() override;

    /**
     * @brief Get singleton instance.
     * @return Reference to the global ProgressiveRenderer instance.
     */
    static ProgressiveRenderer& instance();

    /**
     * @brief Request progressive rendering of a page.
     * @param page The page to render.
     * @param initialSize Initial low-quality size hint.
     * @param finalSize Target final size.
     * @param zoomLevel Zoom level.
     * @param rotation Page rotation.
     * @param clipRect Optional clipping rectangle.
     * @param qualityLevels Number of quality levels/steps to render (default 3).
     * @return A unique request ID.
     */
    quintptr requestRender(Page* page, const QSize& initialSize, const QSize& finalSize,
                           qreal zoomLevel, int rotation, const QRectF& clipRect = QRectF(),
                           int qualityLevels = 3);

    /**
     * @brief Cancel a pending progressive render request.
     * @param requestId The ID of the request to cancel.
     */
    void cancelRequest(quintptr requestId);

    /**
     * @brief Cancel all pending progressive render requests.
     */
    void cancelAllRequests();

    /**
     * @brief Get the number of requests currently queued.
     * @return Queued request count.
     */
    int queuedRequestCount() const;

    /**
     * @brief Get the number of requests currently being processed.
     * @return Active request count.
     */
    int activeRequestCount() const;

    /**
     * @brief Set the maximum number of concurrent progressive rendering tasks.
     * @param count Maximum concurrent tasks.
     */
    void setMaxConcurrentRenders(int count);

    /**
     * @brief Get the maximum number of concurrent progressive rendering tasks.
     * @return Maximum concurrent tasks.
     */
    int maxConcurrentRenders() const;

    /**
     * @brief Check if progressive rendering is enabled globally.
     * @return True if enabled.
     */
    bool isEnabled() const;

    /**
     * @brief Enable or disable progressive rendering globally.
     * @param enabled Whether to enable progressive rendering.
     */
    void setEnabled(bool enabled);

    /**
     * @brief Get the default number of quality levels used if not specified in request.
     * @return Default quality levels.
     */
    int defaultQualityLevels() const;

    /**
     * @brief Set the default number of quality levels.
     * @param levels New default level count.
     */
    void setDefaultQualityLevels(int levels);

    /**
     * @brief Get statistics about rendering performance.
     * @return Map containing stats like average pass time, success rate, etc.
     */
    QVariantMap statistics() const;

signals:
    /**
     * @brief Emitted when a rendering pass completes.
     * @param requestId The ID of the original request.
     * @param passResult The result of the completed pass.
     */
    void passCompleted(quintptr requestId, const QuantilyxDoc::ProgressiveRenderer::PassResult& passResult);

    /**
     * @brief Emitted when the final pass of a request completes.
     * @param requestId The ID of the request.
     * @param finalImage The final, fully rendered image.
     */
    void renderCompleted(quintptr requestId, const QImage& finalImage);

    /**
     * @brief Emitted when a render request is canceled.
     * @param requestId The ID of the canceled request.
     */
    void renderCanceled(quintptr requestId);

    /**
     * @brief Emitted when a render request fails.
     * @param requestId The ID of the failed request.
     * @param error Error message.
     */
    void renderFailed(quintptr requestId, const QString& error);

    /**
     * @brief Emitted when the queue status changes.
     * @param queuedCount Number of queued requests.
     * @param activeCount Number of active requests.
     */
    void queueStatusChanged(int queuedCount, int activeCount);

private slots:
    void processNextRequest(); // Internal slot to handle the queue

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace QuantilyxDoc

#endif // QUANTILYX_PROGRESSIVERENDERER_H