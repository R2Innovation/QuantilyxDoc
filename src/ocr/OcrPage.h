/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#ifndef QUANTILYX_OCRPAGE_H
#define QUANTILYX_OCRPAGE_H

#include <QObject>
#include <QImage>
#include <QRectF>
#include <QList>
#include <QMap>
#include <QFuture>
#include <memory>

namespace QuantilyxDoc {

class Document; // Forward declaration
class Page; // Forward declaration

/**
 * @brief Manages OCR results for a specific page within a document.
 * 
 * Stores the recognized text, bounding boxes, and confidences for a page.
 * Can trigger OCR on the page's content if it hasn't been processed yet.
 */
class OcrPage : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructor.
     * @param document The parent Document this OCR data belongs to.
     * @param page The Page object this OCR data corresponds to.
     * @param parent Parent object.
     */
    explicit OcrPage(Document* document, Page* page, QObject* parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~OcrPage() override;

    /**
     * @brief Get the associated document.
     * @return Pointer to the document.
     */
    Document* document() const;

    /**
     * @brief Get the associated page.
     * @return Pointer to the page.
     */
    Page* page() const;

    /**
     * @brief Check if OCR has been performed on this page.
     * @return True if OCR data is available.
     */
    bool isProcessed() const;

    /**
     * @brief Get the full text recognized on the page.
     * @return Recognized text string.
     */
    QString fullText() const;

    /**
     * @brief Get the list of recognized text elements with their bounding boxes.
     * @return List of (text, bounding box) pairs.
     */
    QList<QPair<QString, QRectF>> textElements() const;

    /**
     * @brief Get the confidence level for the entire page.
     * @return Average confidence (0.0 - 1.0).
     */
    float averageConfidence() const;

    /**
     * @brief Get the confidence levels for individual text elements.
     * @return List of confidence values corresponding to textElements().
     */
    QList<float> elementConfidences() const;

    /**
     * @brief Perform OCR on the page's content synchronously.
     * @param force If true, re-perform OCR even if already processed.
     * @return True if OCR was successful.
     */
    bool performOcr(bool force = false);

    /**
     * @brief Perform OCR on the page's content asynchronously.
     * @param force If true, re-perform OCR even if already processed.
     * @return A QFuture that will be ready when OCR completes.
     */
    QFuture<bool> performOcrAsync(bool force = false);

    /**
     * @brief Search for text within the OCR results on this page.
     * @param searchText The text to search for.
     * @param caseSensitive Whether the search is case-sensitive.
     * @param wholeWords Whether to match whole words only.
     * @return List of rectangles where the text was found.
     */
    QList<QRectF> searchText(const QString& searchText, bool caseSensitive = false, bool wholeWords = false) const;

    /**
     * @brief Get the OCR result for a specific region of the page.
     * @param region The region to get OCR data for.
     * @return Detailed OCR result for the region.
     */
    OcrResult ocrResultForRegion(const QRectF& region) const;

    /**
     * @brief Get the list of all recognized words on the page with their bounding boxes.
     * @return List of (word, bounding box) pairs.
     */
    QList<QPair<QString, QRectF>> words() const;

    /**
     * @brief Get the list of all recognized lines on the page with their bounding boxes.
     * @return List of (line, bounding box) pairs.
     */
    QList<QPair<QString, QRectF>> lines() const;

    /**
     * @brief Get the list of all recognized paragraphs on the page with their bounding boxes.
     * @return List of (paragraph, bounding box) pairs.
     */
    QList<QPair<QString, QRectF>> paragraphs() const;

signals:
    /**
     * @brief Emitted when OCR processing starts on the page.
     */
    void ocrStarted();

    /**
     * @brief Emitted when OCR processing finishes successfully on the page.
     */
    void ocrFinished();

    /**
     * @brief Emitted when OCR processing fails on the page.
     * @param error Error message.
     */
    void ocrFailed(const QString& error);

    /**
     * @brief Emitted when the OCR text content changes.
     */
    void textChanged();

private:
    class Private;
    std::unique_ptr<Private> d;

    // Helper to store OCR results internally
    void storeOcrResults(const OcrResult& result);
};

} // namespace QuantilyxDoc

#endif // QUANTILYX_OCRPAGE_H