/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "ProgressiveRenderer.h"
#include "Page.h"
#include "Document.h"
#include "Logger.h"
#include "ThreadPool.h" // Use our custom ThreadPool for passes
#include "Task.h"       // Use our custom Task
#include <QMutex>
#include <QMutexLocker>
#include <QDateTime>
#include <QImage>
#include <QPainter>
#include <QCoreApplication>
#include <QThread>
#include <QDebug>
#include <algorithm> // For std::find, std::remove

namespace QuantilyxDoc {

struct RenderRequestInternal {
    quintptr id;
    QPointer<Page> page; // Use QPointer for safety
    QSize initialSize;
    QSize finalSize;
    qreal zoomLevel;
    int rotation;
    QRectF clipRect;
    int qualityLevels;
    QList<RenderPass> passes;
    bool canceled;
    QDateTime requestTime;

    RenderRequestInternal(quintptr reqId) : id(reqId), canceled(false) {}
};

class ProgressiveRenderer::Private {
public:
    Private(ProgressiveRenderer* q_ptr)
        : q(q_ptr), maxConcurrent(2), enabled(true), defaultQualityLvls(3), activeCount(0) {}

    ProgressiveRenderer* q;
    mutable QMutex mutex; // Protect access to queues and maps
    QHash<quintptr, RenderRequestInternal> requestMap; // All requests (queued, active)
    QQueue<quintptr> requestQueue; // IDs of queued requests
    QSet<quintptr> activeRequestIds; // IDs of currently processing requests
    int maxConcurrent;
    bool enabled;
    int defaultQualityLvls;
    int activeCount;

    // Helper to generate passes for a request
    void generatePasses(RenderRequestInternal& request) {
        // Calculate intermediate sizes between initial and final
        qreal initialArea = static_cast<qreal>(request.initialSize.width()) * request.initialSize.height();
        qreal finalArea = static_cast<qreal>(request.finalSize.width()) * request.finalSize.height();
        if (initialArea >= finalArea) {
            // If initial is already larger or equal, just do one final pass
            RenderPass pass;
            pass.passNumber = 0;
            pass.targetSize = request.finalSize;
            pass.zoomLevel = request.zoomLevel;
            pass.rotation = request.rotation;
            pass.clipRect = request.clipRect;
            pass.isFinalPass = true;
            request.passes.append(pass);
            return;
        }

        // Distribute quality levels across the area range
        for (int i = 0; i < request.qualityLevels; ++i) {
            qreal t = static_cast<qreal>(i) / (request.qualityLevels - 1); // 0.0 to 1.0
            qreal areaRatio = initialArea + t * (finalArea - initialArea);
            qreal scale = qSqrt(areaRatio / initialArea);
            QSize size = QSize(qRound(request.initialSize.width() * scale),
                               qRound(request.initialSize.height() * scale));
            size = size.boundedTo(request.finalSize); // Don't exceed final size

            RenderPass pass;
            pass.passNumber = i;
            pass.targetSize = size;
            pass.zoomLevel = request.zoomLevel;
            pass.rotation = request.rotation;
            pass.clipRect = request.clipRect;
            pass.isFinalPass = (i == request.qualityLevels - 1);
            request.passes.append(pass);
        }
    }

    // Helper to find next request to process
    quintptr getNextRequestIdToProcess() {
        while (!requestQueue.isEmpty()) {
            quintptr id = requestQueue.dequeue();
            auto it = requestMap.find(id);
            if (it != requestMap.end() && !it->canceled) {
                return id; // Found a valid, non-canceled request
            } else {
                // Request was canceled or removed, remove from active tracking if needed and continue
                activeRequestIds.remove(id);
                requestMap.remove(id);
            }
        }
        return 0; // No valid request found
    }
};

// Static instance pointer
ProgressiveRenderer* ProgressiveRenderer::s_instance = nullptr;

ProgressiveRenderer& ProgressiveRenderer::instance()
{
    if (!s_instance) {
        s_instance = new ProgressiveRenderer();
    }
    return *s_instance;
}

ProgressiveRenderer::ProgressiveRenderer(QObject* parent)
    : QObject(parent)
    , d(new Private(this))
{
    LOG_INFO("ProgressiveRenderer initialized with max concurrent: " << d->maxConcurrent);
}

ProgressiveRenderer::~ProgressiveRenderer()
{
    cancelAllRequests(); // Attempt to cancel pending requests
}

quintptr ProgressiveRenderer::requestRender(Page* page, const QSize& initialSize, const QSize& finalSize,
                                           qreal zoomLevel, int rotation, const QRectF& clipRect,
                                           int qualityLevels)
{
    if (!page || !enabled()) return 0;

    QMutexLocker locker(&d->mutex);
    quintptr requestId = reinterpret_cast<quintptr>(this) ^ QDateTime::currentMSecsSinceEpoch() ^ d->requestMap.size(); // Simple ID generation

    RenderRequestInternal request(requestId);
    request.page = page;
    request.initialSize = initialSize;
    request.finalSize = finalSize;
    request.zoomLevel = zoomLevel;
    request.rotation = rotation;
    request.clipRect = clipRect;
    request.qualityLevels = (qualityLevels > 0) ? qualityLevels : d->defaultQualityLvls;
    request.requestTime = QDateTime::currentDateTime();

    d->generatePasses(request); // Calculate the rendering passes needed

    d->requestMap.insert(requestId, request);
    d->requestQueue.enqueue(requestId);

    LOG_DEBUG("Queued progressive render request: " << requestId << " for page " << page->pageIndex());

    emit queueStatusChanged(d->requestQueue.size(), d->activeCount);

    // Potentially trigger processing
    QMetaObject::invokeMethod(this, &ProgressiveRenderer::processNextRequest, Qt::QueuedConnection);

    return requestId;
}

void ProgressiveRenderer::cancelRequest(quintptr requestId)
{
    QMutexLocker locker(&d->mutex);
    auto it = d->requestMap.find(requestId);
    if (it != d->requestMap.end()) {
        it->canceled = true; // Mark for cancellation
        // If it's active, the running task should check this flag.
        // If it's queued, it will be skipped during processing.
        if (d->activeRequestIds.contains(requestId)) {
            LOG_DEBUG("Marked active request for cancellation: " << requestId);
        } else {
            // Remove from queue if it's still there
            d->requestQueue.removeAll(requestId);
            LOG_DEBUG("Removed queued request for cancellation: " << requestId);
        }
        emit renderCanceled(requestId);
        emit queueStatusChanged(d->requestQueue.size(), d->activeCount);
    } else {
        LOG_DEBUG("Request to cancel not found: " << requestId);
    }
}

void ProgressiveRenderer::cancelAllRequests()
{
    QMutexLocker locker(&d->mutex);
    int count = d->requestMap.size();
    for (auto& request : d->requestMap) {
        request.canceled = true;
    }
    d->requestQueue.clear();
    LOG_DEBUG("Marked all " << count << " requests for cancellation.");
    emit queueStatusChanged(0, d->activeCount);
}

int ProgressiveRenderer::queuedRequestCount() const
{
    QMutexLocker locker(&d->mutex);
    return d->requestQueue.size();
}

int ProgressiveRenderer::activeRequestCount() const
{
    QMutexLocker locker(&d->mutex);
    return d->activeCount; // Or d->activeRequestIds.size();
}

void ProgressiveRenderer::setMaxConcurrentRenders(int count)
{
    if (count < 1) return;
    QMutexLocker locker(&d->mutex);
    if (d->maxConcurrent != count) {
        d->maxConcurrent = count;
        LOG_INFO("Max concurrent progressive renders set to " << count);
        // Potentially trigger more processing if limit increased
        QMetaObject::invokeMethod(this, &ProgressiveRenderer::processNextRequest, Qt::QueuedConnection);
    }
}

int ProgressiveRenderer::maxConcurrentRenders() const
{
    QMutexLocker locker(&d->mutex);
    return d->maxConcurrent;
}

bool ProgressiveRenderer::isEnabled() const
{
    QMutexLocker locker(&d->mutex);
    return d->enabled;
}

void ProgressiveRenderer::setEnabled(bool enabled)
{
    QMutexLocker locker(&d->mutex);
    if (d->enabled != enabled) {
        d->enabled = enabled;
        if (!enabled) {
            cancelAllRequests(); // Cancel all if disabled
        }
        LOG_INFO("ProgressiveRenderer is now " << (enabled ? "enabled" : "disabled"));
    }
}

int ProgressiveRenderer::defaultQualityLevels() const
{
    QMutexLocker locker(&d->mutex);
    return d->defaultQualityLvls;
}

void ProgressiveRenderer::setDefaultQualityLevels(int levels)
{
    if (levels < 1) return;
    QMutexLocker locker(&d->mutex);
    if (d->defaultQualityLvls != levels) {
        d->defaultQualityLvls = levels;
        LOG_INFO("Default quality levels for progressive render set to " << levels);
    }
}

QVariantMap ProgressiveRenderer::statistics() const
{
    QVariantMap stats;
    stats["queuedRequestCount"] = queuedRequestCount();
    stats["activeRequestCount"] = activeRequestCount();
    stats["maxConcurrentRenders"] = maxConcurrentRenders();
    stats["isEnabled"] = isEnabled();
    stats["defaultQualityLevels"] = defaultQualityLevels();
    // More stats could be added based on historical data of pass times, success rates, etc.
    return stats;
}

void ProgressiveRenderer::processNextRequest()
{
    QMutexLocker locker(&d->mutex);

    // Check if we can start another request
    if (d->activeCount >= d->maxConcurrent || !d->enabled) {
        emit queueStatusChanged(d->requestQueue.size(), d->activeCount);
        return;
    }

    quintptr requestId = d->getNextRequestIdToProcess();
    if (requestId == 0) {
        // No valid request to process
        emit queueStatusChanged(d->requestQueue.size(), d->activeCount);
        return;
    }

    auto requestIt = d->requestMap.find(requestId);
    if (requestIt == d->requestMap.end()) {
        // Should not happen if getNextRequestIdToProcess is correct
        LOG_WARN("processNextRequest: Request ID " << requestId << " not found in map!");
        emit queueStatusChanged(d->requestQueue.size(), d->activeCount);
        return;
    }

    RenderRequestInternal& request = *requestIt;
    d->activeRequestIds.insert(requestId);
    d->activeCount++;

    LOG_DEBUG("Starting progressive render request: " << requestId << " with " << request.passes.size() << " passes.");

    // Create a Task that will handle all passes for this request sequentially
    Task* renderTask = new Task([this, requestId, request]() {
        // Capture request by value to avoid lifetime issues with the main thread's map
        // In a real implementation, you'd need a more robust way to ensure the Page object is valid.
        if (!request.page || request.canceled) {
             LOG_DEBUG("Render task started but request was canceled or page invalid: " << requestId);
             // Report cancellation/failure on main thread
             QMetaObject::invokeMethod(this, [this, requestId]() {
                 QMutexLocker resLocker(&d->mutex); // Lock to update active count
                 d->activeRequestIds.remove(requestId);
                 d->activeCount--;
                 if (request.canceled) {
                     emit renderCanceled(requestId);
                 } else {
                     emit renderFailed(requestId, "Page became invalid");
                 }
                 // Process next
                 QMetaObject::invokeMethod(this, &ProgressiveRenderer::processNextRequest, Qt::QueuedConnection);
             }, Qt::QueuedConnection);
             return;
        }

        Page* page = request.page.data();
        QImage finalImage; // To hold the result of the final pass
        bool overallSuccess = true;
        QString overallError;

        for (const auto& pass : request.passes) {
            if (request.canceled) {
                LOG_DEBUG("Render request " << requestId << " was canceled during pass " << pass.passNumber);
                overallSuccess = false;
                overallError = "Request canceled";
                break;
            }

            // --- Simulated Rendering Pass ---
            // In a real implementation, this would call the page's renderer with the specific pass parameters.
            // It might involve scaling, applying filters, rendering specific layers, etc.
            // For this stub, we'll create a placeholder image based on the pass size and page info.
            QThread::msleep(50 + (pass.passNumber * 20)); // Simulate increasing time for higher quality passes

            QImage image(pass.targetSize, QImage::Format_ARGB32_Premultiplied);
            if (image.isNull()) {
                overallSuccess = false;
                overallError = "Failed to create image buffer for pass " + QString::number(pass.passNumber);
                LOG_ERROR(overallError);
                break;
            }
            image.fill(Qt::lightGray); // Placeholder background
            QPainter painter(&image);
            if (!painter.isActive()) {
                 overallSuccess = false;
                 overallError = "Failed to initialize painter for pass " + QString::number(pass.passNumber);
                 LOG_ERROR(overallError);
                 break;
            }
            // Draw a simple representation of the page content
            painter.fillRect(5, 5, image.width() - 10, 20, QColor(200, 220, 255)); // Header
            painter.setPen(Qt::black);
            painter.drawText(QRect(10, 10, image.width() - 20, 15), Qt::AlignLeft, QString("Page %1 - Pass %2").arg(page->pageIndex()).arg(pass.passNumber));
            painter.fillRect(5, 35, image.width() - 10, image.height() - 40, QColor(240, 240, 240)); // Body
            painter.end();
            // --- End Simulated Pass ---

            PassResult result;
            result.passNumber = pass.passNumber;
            result.image = image;
            result.success = true; // Assume success in this stub
            result.errorMessage = "";
            result.durationMs = pass.timer.elapsed(); // Timer not actually started in stub
            result.isFinal = pass.isFinalPass;

            if (result.success) {
                finalImage = result.image; // Update final image if this was the last pass
                LOG_DEBUG("Completed render pass " << pass.passNumber << " for request " << requestId);
            } else {
                overallSuccess = false;
                overallError = result.errorMessage;
                LOG_ERROR("Render pass " << pass.passNumber << " failed for request " << requestId << ": " << result.errorMessage);
                break; // Stop further passes on failure
            }

            // Emit pass completion on main thread
            QMetaObject::invokeMethod(this, [this, requestId, result]() {
                 emit passCompleted(requestId, result);
            }, Qt::QueuedConnection);

            // Small delay between passes to simulate real rendering and allow UI updates
            // QThread::msleep(10); // Maybe not needed if rendering is already slow enough
        }

        // Report final result on main thread
        QMetaObject::invokeMethod(this, [this, requestId, finalImage, overallSuccess, overallError]() {
             QMutexLocker resLocker(&d->mutex); // Lock to update active count
             d->activeRequestIds.remove(requestId);
             d->activeCount--;

             // Remove the request from the map as it's done
             d->requestMap.remove(requestId);

             if (overallSuccess) {
                 emit renderCompleted(requestId, finalImage);
                 LOG_DEBUG("Successfully completed progressive render request: " << requestId);
             } else {
                 emit renderFailed(requestId, overallError);
                 LOG_WARN("Progressive render request failed: " << requestId << ", Error: " << overallError);
             }

             // Process the next request in the queue
             QMetaObject::invokeMethod(this, &ProgressiveRenderer::processNextRequest, Qt::QueuedConnection);
        }, Qt::QueuedConnection);

    }, "ProgressiveRenderTask_" + QString::number(requestId), Task::Priority::Normal);

    // Submit the task to the global ThreadPool
    ThreadPool::instance().submitTask(renderTask);

    // Update queue status after moving request to active
    emit queueStatusChanged(d->requestQueue.size(), d->activeCount);
}

} // namespace QuantilyxDoc