/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#ifndef QUANTILYX_PDFDOCUMENT_H
#define QUANTILYX_PDFDOCUMENT_H

#include "../../core/Document.h"
#include <poppler-qt5.h>
#include <memory>
#include <QMap>
#include <QList>

namespace QuantilyxDoc {

class PdfPage;
class PdfAnnotation;
class PdfFormField; // For form filling

/**
 * @brief PDF document implementation using Poppler.
 * 
 * Concrete implementation of the Document interface specifically for PDF files.
 * Uses Poppler-Qt5 to handle loading, parsing, and interacting with PDF content.
 */
class PdfDocument : public Document
{
    Q_OBJECT

public:
    /**
     * @brief Constructor.
     * @param parent Parent object.
     */
    explicit PdfDocument(QObject* parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~PdfDocument() override;

    // --- Document Interface Implementation ---
    bool load(const QString& filePath, const QString& password = QString()) override;
    bool save(const QString& filePath = QString()) override;
    DocumentType type() const override;
    int pageCount() const override;
    Page* page(int index) const override;
    bool isLocked() const override;
    bool isEncrypted() const override;
    QString formatVersion() const override;
    bool supportsFeature(const QString& feature) const override;

    // --- PDF-Specific Metadata ---
    /**
     * @brief Get the PDF version string (e.g., "1.4", "1.7").
     * @return PDF version.
     */
    QString pdfVersion() const;

    /**
     * @brief Check if the PDF is linearized (web optimized).
     * @return true if linearized.
     */
    bool isLinearized() const;

    /**
     * @brief Get the page layout mode.
     * @return Page layout mode.
     */
    Poppler::Document::PageLayout pageLayout() const;

    /**
     * @brief Get the page mode.
     * @return Page mode.
     */
    Poppler::Document::PageMode pageMode() const;

    /**
     * @brief Get the PDF producer.
     * @return Producer application name.
     */
    QString producer() const;

    /**
     * @brief Get the PDF creator.
     * @return Creator application name.
     */
    QString creator() const;

    /**
     * @brief Get the PDF subject.
     * @return Subject.
     */
    QString subject() const override;

    /**
     * @brief Get the PDF keywords.
     * @return Keywords.
     */
    QStringList keywords() const override;

    // --- PDF-Specific Functionality ---
    /**
     * @brief Check if the document has forms.
     * @return true if contains forms.
     */
    bool hasForms() const;

    /**
     * @brief Check if the document has annotations.
     * @return true if contains annotations.
     */
    bool hasAnnotations() const;

    /**
     * @brief Check if the document has embedded files.
     * @return true if contains embedded files.
     */
    bool hasEmbeddedFiles() const;

    /**
     * @brief Get the PDF metadata as XML (XMP).
     * @return XMP metadata.
     */
    QString xmpMetadata() const;

    /**
     * @brief Get the PDF metadata as a map.
     * @return Metadata map.
     */
    QVariantMap metadata() const override;

    /**
     * @brief Get the underlying Poppler document.
     * @return Poppler document or nullptr.
     */
    Poppler::Document* popplerDocument() const;

    /**
     * @brief Get the list of all form fields in the document.
     * @return List of form fields.
     */
    QList<PdfFormField*> formFields() const;

    /**
     * @brief Get the list of all embedded files.
     * @return List of embedded file names/paths.
     */
    QStringList embeddedFiles() const;

    /**
     * @brief Extract an embedded file by name.
     * @param fileName Name of the embedded file.
     * @param outputPath Path to save the extracted file.
     * @return True if extraction was successful.
     */
    bool extractEmbeddedFile(const QString& fileName, const QString& outputPath) const;

    /**
     * @brief Remove a password from an encrypted document.
     * @param password The current password.
     * @return True if removal was successful (document becomes unlocked).
     */
    bool removePassword(const QString& password);

    /**
     * @brief Check if the document has copy/print restrictions.
     * @return True if restrictions are present.
     */
    bool hasRestrictions() const;

    /**
     * @brief Remove copy/print restrictions (if possible).
     * @return True if restrictions were removed or didn't exist.
     */
    bool removeRestrictions();

    /**
     * @brief Get the list of annotations for a specific page.
     * @param pageIndex Index of the page.
     * @return List of annotations.
     */
    QList<PdfAnnotation*> annotationsForPage(int pageIndex) const;

    /**
     * @brief Add an annotation to a specific page.
     * @param pageIndex Index of the page.
     * @param annotation The annotation to add.
     * @return True if addition was successful.
     */
    bool addAnnotationToPage(int pageIndex, PdfAnnotation* annotation);

    /**
     * @brief Remove an annotation from a specific page.
     * @param pageIndex Index of the page.
     * @param annotation The annotation to remove.
     * @return True if removal was successful.
     */
    bool removeAnnotationFromPage(int pageIndex, PdfAnnotation* annotation);

    // --- Table of Contents / Bookmarks ---
    /**
     * @brief Check if the document has a table of contents.
     * @return True if TOC exists.
     */
    bool hasTableOfContents() const override;

    /**
     * @brief Get the table of contents as a nested structure.
     * @return TOC structure (e.g., QList<QVariantMap> where map contains "title", "destination", "children").
     */
    QVariantList tableOfContents() const override;

    /**
     * @brief Get the list of named destinations.
     * @return Map of name -> destination.
     */
    QMap<QString, QVariant> namedDestinations() const;

    /**
     * @brief Navigate to a named destination.
     * @param name Name of the destination.
     * @return True if navigation was successful.
     */
    bool navigateToDestination(const QString& name);

signals:
    /**
     * @brief Emitted when form fields are loaded or changed.
     */
    void formFieldsChanged();

    /**
     * @brief Emitted when embedded files list is updated.
     */
    void embeddedFilesChanged();

    /**
     * @brief Emitted when restrictions are removed.
     */
    void restrictionsRemoved();

protected:
    /**
     * @brief Populate document metadata from Poppler after loading.
     */
    void populateMetadata();

private:
    class Private;
    std::unique_ptr<Private> d;

    // Helper to create PdfPage objects
    std::unique_ptr<PdfPage> createPdfPage(int index) const;
    QPDFObjectHandle findQpdfAnnotationHandle(const QPDFObjectHandle& pageObj, PdfAnnotation* pdfAnnot) const;
    QString getQpdfAnnotationContents(const QPDFObjectHandle& annotObj) const;
    QColor getQpdfAnnotationColor(const QPDFObjectHandle& annotObj) const;

    // Add a method to mark internal state as modified (called by AnnotationManager or setters)
    void setInMemoryStateModifiedFlag(bool modified);
    bool isInMemoryStateModified() const; // Getter if needed elsewhere
};

} // namespace QuantilyxDoc

#endif // QUANTILYX_PDFDOCUMENT_H