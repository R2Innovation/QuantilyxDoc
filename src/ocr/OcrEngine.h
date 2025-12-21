/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#ifndef QUANTILYX_OCRENGINE_H
#define QUANTILYX_OCRENGINE_H

#include <QObject>
#include <QImage>
#include <QRectF>
#include <QList>
#include <QFuture>
#include <QFutureWatcher>
#include <memory>
#include <QThread>

namespace QuantilyxDoc {

/**
 * @brief Structure holding the result of an OCR operation on a specific region of an image.
 */
struct OcrResult {
    QString text;                 // The recognized text
    QList<QRectF> boundingBoxes;  // Bounding boxes for individual words/lines within the text
    float confidence;             // Confidence level (0.0 to 1.0)
    QString language;             // Language detected or used for recognition
};

/**
 * @brief Manages OCR operations using an underlying OCR library (e.g., Tesseract).
 * 
 * Provides methods for performing OCR on images and text regions.
 * Can operate synchronously or asynchronously.
 */
class OcrEngine : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructor.
     * @param parent Parent object.
     */
    explicit OcrEngine(QObject* parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~OcrEngine() override;

    /**
     * @brief Get singleton instance.
     * @return Reference to the global OcrEngine instance.
     */
    static OcrEngine& instance();

    /**
     * @brief Initialize the OCR engine.
     * Loads language data and sets up the underlying library.
     * @param language Language code to use (e.g., "eng", "deu", "fra").
     * @param datapath Path to Tesseract data files (tessdata).
     * @return True if initialization was successful.
     */
    bool initialize(const QString& language = "eng", const QString& datapath = QString());

    /**
     * @brief Check if the OCR engine is initialized and ready.
     * @return True if ready.
     */
    bool isReady() const;

    /**
     * @brief Perform OCR on an entire image synchronously.
     * @param image The image to recognize text from.
     * @return The recognized text.
     */
    QString recognizeText(const QImage& image) const;

    /**
     * @brief Perform OCR on a specific region of an image synchronously.
     * @param image The image to recognize text from.
     * @param region The region within the image to process.
     * @return The recognized text.
     */
    QString recognizeText(const QImage& image, const QRectF& region) const;

    /**
     * @brief Perform OCR on an image asynchronously.
     * @param image The image to recognize text from.
     * @return A QFuture that will hold the recognized text upon completion.
     */
    QFuture<QString> recognizeTextAsync(const QImage& image) const;

    /**
     * @brief Perform detailed OCR on an image synchronously.
     * Provides text along with bounding boxes for individual elements.
     * @param image The image to recognize text from.
     * @return Detailed OCR result structure.
     */
    OcrResult recognizeDetailed(const QImage& image) const;

    /**
     * @brief Perform detailed OCR on a specific region of an image synchronously.
     * @param image The image to recognize text from.
     * @param region The region within the image to process.
     * @return Detailed OCR result structure.
     */
    OcrResult recognizeDetailed(const QImage& image, const QRectF& region) const;

    /**
     * @brief Get the list of supported languages.
     * @return List of language codes (e.g., "eng", "deu").
     */
    QStringList supportedLanguages() const;

    /**
     * @brief Get the currently active language.
     * @return Language code.
     */
    QString currentLanguage() const;

    /**
     * @brief Set the language for subsequent OCR operations.
     * @param language Language code.
     * @return True if the language was successfully set.
     */
    bool setLanguage(const QString& language);

    /**
     * @brief Get the path to the Tesseract data directory.
     * @return Datapath string.
     */
    QString datapath() const;

    /**
     * @brief Set the path to the Tesseract data directory.
     * @param path Datapath string.
     */
    void setDatapath(const QString& path);

    /**
     * @brief Get the current resolution (DPI) used for OCR.
     * @return Resolution in DPI.
     */
    int resolution() const;

    /**
     * @brief Set the resolution (DPI) used for OCR.
     * @param dpi Resolution in DPI.
     */
    void setResolution(int dpi);

    /**
     * @brief Get the confidence threshold for OCR results.
     * @return Threshold value (0.0 - 1.0).
     */
    float confidenceThreshold() const;

    /**
     * @brief Set the confidence threshold for OCR results.
     * Results below this threshold might be filtered out.
     * @param threshold Threshold value (0.0 - 1.0).
     */
    void setConfidenceThreshold(float threshold);

signals:
    /**
     * @brief Emitted when OCR initialization is complete.
     * @param success True if initialization was successful.
     */
    void initializationComplete(bool success);

    /**
     * @brief Emitted when an async OCR task starts.
     */
    void recognitionStarted();

    /**
     * @brief Emitted when an async OCR task finishes.
     */
    void recognitionFinished();

    /**
     * @brief Emitted when an async OCR task fails.
     * @param error Error message.
     */
    void recognitionFailed(const QString& error);

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace QuantilyxDoc

#endif // QUANTILYX_OCRENGINE_H