/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#ifndef QUANTILYX_ANNOTATIONMANAGER_H
#define QUANTILYX_ANNOTATIONMANAGER_H

#include <QObject>
#include <QHash>
#include <QList>
#include <QPointer>
#include <memory>

namespace QuantilyxDoc {

class Document;
class Page;
class Annotation; // Assuming a base Annotation class exists

/**
 * @brief Manages annotations across all open documents.
 * 
 * Provides a central interface for adding, removing, finding, and modifying annotations.
 * Handles the complexity of different document types (PDF, EPUB, etc.) having different
 * annotation implementations (e.g., PdfAnnotation). It also manages the lifecycle
 * and persistence of annotations, especially considering the read-only nature of
 * many format-specific annotation objects (like those from Poppler).
 */
class AnnotationManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructor.
     * @param parent Parent object.
     */
    explicit AnnotationManager(QObject* parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~AnnotationManager() override;

    /**
     * @brief Get singleton instance.
     * @return Reference to the global AnnotationManager instance.
     */
    static AnnotationManager& instance();

    /**
     * @brief Register a document with the annotation manager.
     * This allows the manager to track annotations for this document.
     * @param doc The document to register.
     */
    void registerDocument(Document* doc);

    /**
     * @brief Unregister a document from the annotation manager.
     * This removes the document and its associated annotations from management.
     * @param doc The document to unregister.
     */
    void unregisterDocument(Document* doc);

    /**
     * @brief Add an annotation to a specific document and page.
     * @param doc The document to add the annotation to.
     * @param pageIndex The 0-based index of the page.
     * @param annotation The annotation object to add.
     * @return True if the annotation was added successfully.
     */
    bool addAnnotation(Document* doc, int pageIndex, Annotation* annotation);

    /**
     * @brief Remove an annotation from a document.
     * @param doc The document the annotation belongs to.
     * @param annotation The annotation object to remove.
     * @return True if the annotation was removed successfully.
     */
    bool removeAnnotation(Document* doc, Annotation* annotation);

    /**
     * @brief Get all annotations for a specific document.
     * @param doc The document.
     * @return List of annotation objects.
     */
    QList<Annotation*> annotationsForDocument(Document* doc) const;

    /**
     * @brief Get all annotations for a specific page within a document.
     * @param doc The document containing the page.
     * @param pageIndex The 0-based index of the page.
     * @return List of annotation objects.
     */
    QList<Annotation*> annotationsForPage(Document* doc, int pageIndex) const;

    /**
     * @brief Find annotations intersecting a specific rectangle on a page.
     * @param doc The document containing the page.
     * @param pageIndex The 0-based index of the page.
     * @param rect The rectangle in page coordinates to search within.
     * @return List of annotation objects whose bounds intersect the rectangle.
     */
    QList<Annotation*> findAnnotationsInRect(Document* doc, int pageIndex, const QRectF& rect) const;

    /**
     * @brief Get the total number of annotations managed.
     * @return Count of all annotations.
     */
    int totalAnnotationCount() const;

    /**
     * @brief Get the number of annotations for a specific document.
     * @param doc The document.
     * @return Count of annotations for the document.
     */
    int annotationCountForDocument(Document* doc) const;

    /**
     * @brief Mark a document as having modified annotations that need saving.
     * This is used to track which documents require a save operation to persist annotation changes.
     * @param doc The document with modified annotations.
     */
    void markDocumentAsModified(Document* doc);

    /**
     * @brief Check if a document has annotations that have been modified since the last save.
     * @param doc The document to check.
     * @return True if the document has modified annotations.
     */
    bool isDocumentModified(Document* doc) const;

    /**
     * @brief Prepare all modified annotations for saving.
     * This function should be called before saving a document to ensure any changes
     * made to annotations (e.g., via the UI) are synchronized with the format-specific
     * objects (e.g., Poppler::Annotation) or prepared for external writing tools.
     * @param doc The document whose annotations need preparation.
     * @return True if preparation was successful.
     */
    bool prepareAnnotationsForSave(Document* doc);

signals:
    /**
     * @brief Emitted when an annotation is added.
     * @param doc The document the annotation was added to.
     * @param pageIndex The page index the annotation was added to.
     * @param annotation The added annotation object.
     */
    void annotationAdded(QuantilyxDoc::Document* doc, int pageIndex, QuantilyxDoc::Annotation* annotation);

    /**
     * @brief Emitted when an annotation is removed.
     * @param doc The document the annotation was removed from.
     * @param annotation The removed annotation object.
     */
    void annotationRemoved(QuantilyxDoc::Document* doc, QuantilyxDoc::Annotation* annotation);

    /**
     * @brief Emitted when the list of annotations for a document changes.
     * @param doc The document whose annotation list changed.
     */
    void annotationsChanged(QuantilyxDoc::Document* doc);

    /**
     * @brief Emitted when the modification state of a document changes.
     * @param doc The document whose modification state changed.
     * @param modified The new modification state.
     */
    void documentModifiedChanged(QuantilyxDoc::Document* doc, bool modified);

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace QuantilyxDoc

#endif // QUANTILYX_ANNOTATIONMANAGER_H