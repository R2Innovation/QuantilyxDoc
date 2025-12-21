/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "OcrPage.h"
#include "OcrEngine.h"
#include "../core/Document.h"
#include "../core/Page.h"
#include "../core/Logger.h"
#include <QImage>
#include <QRectF>
#include <QPair>
#include <QList>
#include <QFuture>
#include <QtConcurrent/QtConcurrent>
#include <QMutex>
#include <QMutexLocker>
#include <QDebug>

namespace QuantilyxDoc {

class OcrPage::Private {
public:
    Private(Document* doc, Page* p) : document(doc), page(p), processed(false), avgConfidence(0.0f) {}

    Document* document;
    Page* page;
    bool processed;
    QString fullText;
    QList<QPair<QString, QRectF>> elements;
    QList<float> confidences;
    float avgConfidence;
    mutable QMutex mutex; // Protect access to the stored OCR data
};

OcrPage::OcrPage(Document* document, Page* page, QObject* parent)
    : QObject(parent)
    , d(new Private(document, page))
{
    LOG_DEBUG("OcrPage created for document page.");
}

OcrPage::~OcrPage()
{
    LOG_DEBUG("OcrPage destroyed.");
}

Document* OcrPage::document() const
{
    return d->document;
}

Page* OcrPage::page() const
{
    return d->page;
}

bool OcrPage::isProcessed() const
{
    QMutexLocker locker(&d->mutex);
    return d->processed;
}

QString OcrPage::fullText() const
{
    QMutexLocker locker(&d->mutex);
    return d->fullText;
}

QList<QPair<QString, QRectF>> OcrPage::textElements() const
{
    QMutexLocker locker(&d->mutex);
    return d->elements;
}

float OcrPage::averageConfidence() const
{
    QMutexLocker locker(&d->mutex);
    return d->avgConfidence;
}

QList<float> OcrPage::elementConfidences() const
{
    QMutexLocker locker(&d->mutex);
    return d->confidences;
}

bool OcrPage::performOcr(bool force)
{
    if (!force && isProcessed()) {
        LOG_DEBUG("OcrPage::performOcr: Page already processed, skipping.");
        return true;
    }

    if (!OcrEngine::instance().isReady()) {
        LOG_ERROR("OcrPage::performOcr: OcrEngine is not ready.");
        emit ocrFailed("OCR Engine not initialized.");
        return false;
    }

    emit ocrStarted();

    // Get the page image from the Page object
    // This might involve rendering the page at a high resolution.
    int dpi = OcrEngine::instance().resolution(); // Use engine's resolution setting
    // Assume Page::render(width, height, dpi) exists and returns a QImage
    // QImage pageImage = d->page->render(0, 0, dpi); // 0, 0 means render entire page
    QImage pageImage; // Placeholder

    if (pageImage.isNull()) {
        LOG_ERROR("OcrPage::performOcr: Failed to render page image for OCR.");
        emit ocrFailed("Could not render page image.");
        return false;
    }

    // Perform OCR using the engine
    OcrResult result = OcrEngine::instance().recognizeDetailed(pageImage);

    if (result.text.isEmpty()) {
        LOG_WARN("OcrPage::performOcr: OCR returned no text for page.");
        // Still consider it processed, even if no text was found.
    }

    // Store the results
    storeOcrResults(result);

    emit ocrFinished();
    emit textChanged(); // Notify that text content has changed/updated
    LOG_DEBUG("OcrPage::performOcr: Completed OCR for page, text length: " << result.text.length());
    return true;
}

QFuture<bool> OcrPage::performOcrAsync(bool force)
{
    // Use QtConcurrent to run performOcr in a separate thread
    // This emits signals from the worker thread, so UI should connect with Qt::QueuedConnection.
    return QtConcurrent::run([this, force]() {
        return this->performOcr(force);
    });
}

QList<QRectF> OcrPage::searchText(const QString& searchText, bool caseSensitive, bool wholeWords) const
{
    QList<QRectF> results;
    if (!isProcessed() || searchText.isEmpty()) return results;

    QMutexLocker locker(&d->mutex);
    Qt::CaseSensitivity cs = caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;

    for (int i = 0; i < d->elements.size(); ++i) {
        const auto& element = d->elements[i];
        QString elementText = element.first;
        QRectF elementRect = element.second;

        int pos = 0;
        while ((pos = elementText.indexOf(searchText, pos, cs)) != -1) {
            // Calculate the relative position of the match within the element's text
            // and estimate its bounding box within the element's overall rectangle.
            // This is an approximation.
            float matchStartFraction = static_cast<float>(pos) / elementText.length();
            float matchEndFraction = static_cast<float>(pos + searchText.length()) / elementText.length();

            float elementWidth = elementRect.width();
            float matchX = elementRect.x() + elementWidth * matchStartFraction;
            float matchWidth = elementWidth * (matchEndFraction - matchStartFraction);

            QRectF matchRect(matchX, elementRect.y(), matchWidth, elementRect.height());

            // Apply wholeWords check if needed
            if (wholeWords) {
                // Check if the match is surrounded by word boundaries in the original text element
                bool isStartBoundary = (pos == 0) || elementText[pos - 1].isSpace();
                bool isEndBoundary = (pos + searchText.length() == elementText.length()) || elementText[pos + searchText.length()].isSpace();
                if (isStartBoundary && isEndBoundary) {
                    results.append(matchRect);
                }
            } else {
                results.append(matchRect);
            }

            pos += searchText.length(); // Move past the current match
        }
    }

    LOG_DEBUG("OcrPage::searchText: Found " << results.size() << " matches for '" << searchText << "'");
    return results;
}

OcrResult OcrPage::ocrResultForRegion(const QRectF& region) const
{
    // This would involve performing OCR specifically on the given region of the page image.
    // Get page image, crop to region, call OcrEngine::recognizeDetailed(image, region).
    // QImage pageImage = d->page->render(0, 0, OcrEngine::instance().resolution());
    // QImage regionImage = pageImage.copy(region.toRect());
    // return OcrEngine::instance().recognizeDetailed(regionImage, QRectF()); // Pass empty rect as region is already applied

    LOG_WARN("OcrPage::ocrResultForRegion: Not implemented. Requires rendering region and calling OcrEngine.");
    return OcrResult(); // Placeholder
}

QList<QPair<QString, QRectF>> OcrPage::words() const
{
    // If OcrEngine::recognizeDetailed returns word-level bounding boxes, extract them.
    // Otherwise, this might require splitting the full text/elements further based on spaces.
    // For now, assume elements are words or split them.
    QList<QPair<QString, QRectF>> wordList;
    if (isProcessed()) {
        QMutexLocker locker(&d->mutex);
        for (const auto& element : d->elements) {
            // If element.first is a single word, add it directly.
            // If it's a phrase, split it.
            QStringList words = element.first.split(' ', Qt::SkipEmptyParts);
            if (words.size() == 1) {
                wordList.append(element); // Already a single word
            } else {
                // Approximate bounding boxes for individual words within the phrase's box
                QRectF elementRect = element.second;
                float totalWidth = elementRect.width();
                float currentX = elementRect.x();
                float elementTextWidth = element.first.size(); // Very rough estimation
                for (const QString& word : words) {
                    float wordWidthFraction = static_cast<float>(word.size()) / elementTextWidth;
                    float wordWidth = totalWidth * wordWidthFraction;
                    wordList.append(qMakePair(word, QRectF(currentX, elementRect.y(), wordWidth, elementRect.height())));
                    currentX += wordWidth;
                }
            }
        }
    }
    return wordList;
}

QList<QPair<QString, QRectF>> OcrPage::lines() const
{
    // Lines are usually represented as elements in the OCR result if using HOCR/Line segmentation.
    // If elements are phrases/words, group them by Y-coordinate proximity to form lines.
    // This requires more complex spatial clustering.
    LOG_WARN("OcrPage::lines: Requires spatial clustering of elements or HOCR line-level results.");
    return QList<QPair<QString, QRectF>>(); // Placeholder
}

QList<QPair<QString, QRectF>> OcrPage::paragraphs() const
{
    // Paragraphs are even higher-level groupings.
    // Requires spatial clustering of lines or HOCR paragraph-level results.
    LOG_WARN("OcrPage::paragraphs: Requires spatial clustering of lines or HOCR paragraph-level results.");
    return QList<QPair<QString, QRectF>>(); // Placeholder
}

void OcrPage::storeOcrResults(const OcrResult& result)
{
    QMutexLocker locker(&d->mutex);
    d->fullText = result.text;
    d->elements.clear();
    d->confidences.clear();

    // This is a simplification. OcrResult.boundingBoxes and OcrResult.text need to be mapped correctly.
    // Tesseract's HOCR output provides structured text with bounding boxes for words/phrases/lines.
    // Parsing HOCR or the detailed box output is needed to populate elements correctly.
    // For now, assume result.text is a single element (which is often not the case).
    if (!result.text.isEmpty()) {
        QRectF pageRect; // Need page dimensions to map relative box coordinates if needed
        if (d->page) {
            // pageRect = QRectF(QPointF(0, 0), d->page->size()); // Assuming Page::size() returns QSizeF
        }
        // Use the overall bounding box from result or derive from page size if not available
        QRectF elementBox = result.boundingBoxes.isEmpty() ? pageRect : result.boundingBoxes.first();
        d->elements.append(qMakePair(result.text, elementBox));
        d->confidences.append(result.confidence);
    }

    // Calculate average confidence
    float sumConf = 0.0f;
    for (float conf : d->confidences) {
        sumConf += conf;
    }
    d->avgConfidence = d->confidences.isEmpty() ? 0.0f : sumConf / d->confidences.size();

    d->processed = true;
    LOG_DEBUG("OcrPage::storeOcrResults: Stored OCR data for page, elements: " << d->elements.size());
}

} // namespace QuantilyxDoc