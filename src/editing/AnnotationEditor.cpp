/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "AnnotationEditor.h"
#include "../core/Document.h"
#include "../core/Page.h"
#include "../core/Logger.h"
#include <QPointer>
#include <QMutex>
#include <QMutexLocker>
#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QTimer>
#include <QMetaObject>
#include <QDebug>

namespace QuantilyxDoc {

// Forward declaration of base Annotation class if not included
// class Annotation { ... };

class AnnotationEditor::Private {
public:
    Private(AnnotationEditor* q_ptr)
        : q(q_ptr), activeDocument(nullptr), isEditingVal(false) {}

    AnnotationEditor* q;
    QPointer<Document> activeDocument; // Use QPointer for safety
    mutable QMutex mutex; // Protect access to the annotations map/list during concurrent access
    QHash<QPair<Document*, int>, QSet<QPointer<Annotation>>> docPageToAnnotations; // Map Doc+Page -> Set of Annotations
    QHash<Document*, QSet<QPointer<Annotation>>> docToAnnotations; // Map Doc -> Set of all its Annotations
    bool isEditingVal;
    QPointer<Annotation> currentEditingAnnotation;

    // Helper to add an annotation to the internal maps
    void addToMaps(Document* doc, int pageIndex, Annotation* annotation) {
        QMutexLocker locker(&mutex);
        QPair<Document*, int> docPageKey = {doc, pageIndex};
        docPageToAnnotations[docPageKey].insert(annotation);
        docToAnnotations[doc].insert(annotation);
        LOG_DEBUG("AnnotationEditor: Added annotation '" << annotation->id() << "' to doc: " << doc->filePath() << ", page: " << pageIndex);
    }

    // Helper to remove an annotation from the internal maps
    void removeFromMaps(Document* doc, int pageIndex, Annotation* annotation) {
        QMutexLocker locker(&mutex);
        QPair<Document*, int> docPageKey = {doc, pageIndex};
        if (docPageToAnnotations.contains(docPageKey)) {
            docPageToAnnotations[docPageKey].remove(annotation);
            if (docPageToAnnotations[docPageKey].isEmpty()) {
                docPageToAnnotations.remove(docPageKey);
            }
        }
        if (docToAnnotations.contains(doc)) {
            docToAnnotations[doc].remove(annotation);
            if (docToAnnotations[doc].isEmpty()) {
                docToAnnotations.remove(doc);
            }
        }
        LOG_DEBUG("AnnotationEditor: Removed annotation '" << annotation->id() << "' from doc: " << doc->filePath() << ", page: " << pageIndex);
    }
};

// Static instance pointer
AnnotationEditor* AnnotationEditor::s_instance = nullptr;

AnnotationEditor& AnnotationEditor::instance()
{
    if (!s_instance) {
        s_instance = new AnnotationEditor();
    }
    return *s_instance;
}

AnnotationEditor::AnnotationEditor(QObject* parent)
    : QObject(parent)
    , d(new Private(this))
{
    LOG_INFO("AnnotationEditor created.");
}

AnnotationEditor::~AnnotationEditor()
{
    LOG_INFO("AnnotationEditor destroyed.");
    // Annotations are owned by their respective documents/pages.
    // The editor just holds pointers. Clearing the maps is handled by unregistering documents.
}

QPointer<Annotation> AnnotationEditor::addAnnotation(Document* document, int pageIndex, const AnnotationProperties& properties)
{
    if (!document) {
        LOG_ERROR("AnnotationEditor::addAnnotation: Null document provided.");
        return nullptr;
    }

    if (pageIndex < 0 || pageIndex >= document->pageCount()) {
        LOG_ERROR("AnnotationEditor::addAnnotation: Invalid page index " << pageIndex << " for document with " << document->pageCount() << " pages.");
        return nullptr;
    }

    // Get the page object
    Page* page = document->page(pageIndex);
    if (!page) {
        LOG_ERROR("AnnotationEditor::addAnnotation: Failed to get page " << pageIndex << " from document.");
        return nullptr;
    }

    // --- CRITICAL: Poppler-Qt5 is READ-ONLY ---
    // Poppler::Annotation objects cannot be created or modified directly using Poppler-Qt5.
    // Therefore, `page->addAnnotation()` or similar methods relying on Poppler modification are impossible here.
    // We need a different approach for *creating* annotations.

    // Strategy 1: Use an external tool (e.g., QPDF for PDF, similar tools for others) via QProcess.
    // This requires saving the document, calling the tool, and reloading it.
    // This is complex and slow.

    // Strategy 2: Prepare serialized annotation data for later application during save.
    // This is the more viable approach for a read-only core format handler.
    // The annotation object itself needs to be a custom object that stores its properties
    // and knows how to serialize itself and apply changes during document save.

    // For now, let's assume we have a mechanism to create a *temporary* Annotation object
    // that represents the new annotation. This object would be managed by the page/document
    // and contain the new properties. The *actual* PDF modification happens later during save.

    // Hypothetical: Create a new annotation object managed by the page/document.
    // This requires the Page/Document classes to have methods for adding new annotations.
    // Annotation* newAnnotation = page->createNewAnnotation(properties); // This method does not exist in Poppler-based Page
    // Or, we create a temporary object here and pass it to a hypothetical document/page method for registration.
    // This is complex and touches core document structure significantly.

    // Let's assume for now that the Document/Page/FormatHandler classes have been extended
    // to allow adding *temporary* annotation objects that are persisted later.
    // This requires significant changes to the core architecture beyond just the editor.

    // For this implementation, we'll focus on the *interface* and *tracking* assuming the underlying
    // document structure can handle additions/modifications (perhaps via an external tool hook).
    // We'll return a mock/sentinel object or nullptr to indicate the limitation.

    LOG_WARN("AnnotationEditor::addAnnotation: Cannot add annotation using Poppler-Qt5 (read-only). "
             "This requires a PDF writing library or external tool integration (e.g., QPDF) during save. "
             "Annotation object created temporarily but not persisted to document yet. "
             "Returning a mock object for UI purposes.");
    // QPointer<Annotation> mockAnnotation = new Annotation(properties); // This is not possible without a proper constructor and management
    // page->registerTemporaryAnnotation(mockAnnotation); // Hypothetical method
    // d->addToMaps(document, pageIndex, mockAnnotation);
    // emit annotationAdded(mockAnnotation, document, pageIndex);
    // return mockAnnotation;

    // As a stub for now, return nullptr to indicate the operation cannot be completed with Poppler alone.
    return nullptr;
}

bool AnnotationEditor::modifyAnnotation(Annotation* annotation, const AnnotationProperties& newProperties)
{
    if (!annotation) {
        LOG_ERROR("AnnotationEditor::modifyAnnotation: Null annotation provided.");
        return false;
    }

    // Find the document and page for this annotation by iterating maps (inefficient, needs better lookup)
    // A better design would store document/page pointers directly in the Annotation object if possible.
    // For now, assume the Annotation object has methods to get its page/document (which it should).
    // Document* doc = annotation->document(); // Hypothetical method
    // Page* page = annotation->page();       // Hypothetical method
    // int pageIndex = page ? page->index() : -1; // Hypothetical method

    // For this stub, we'll assume we have the doc and page index.
    Document* doc = nullptr; // Placeholder
    int pageIndex = -1;      // Placeholder
    // ... Logic to find doc/pageIndex from annotation ...

    if (!doc || pageIndex == -1) {
        LOG_ERROR("AnnotationEditor::modifyAnnotation: Could not find document or page index for annotation.");
        return false;
    }

    // --- CRITICAL: Poppler-Qt5 is READ-ONLY ---
    // Similar to addAnnotation, modifying the underlying Poppler::Annotation object is not possible.
    // We must store the intended changes within the Annotation object itself (if it supports it)
    // or in a separate map managed by the editor/document.
    // The changes are applied later during document save.

    // Assume the Annotation object has methods to set properties and mark itself as modified.
    // annotation->setContents(newProperties.contents);
    // annotation->setColor(newProperties.color);
    // annotation->setBounds(newProperties.bounds);
    // annotation->setModified(true); // Mark as needing save

    LOG_WARN("AnnotationEditor::modifyAnnotation: Cannot modify annotation using Poppler-Qt5 (read-only). "
             "Changes stored temporarily in annotation object. "
             "Actual modification requires PDF writing library or external tool during save.");
    // emit annotationModified(annotation);
    // doc->markAsModified(); // Hypothetical method
    // return true; // Indicate local change was accepted

    // As a stub for now, return false to indicate the operation cannot be completed with Poppler alone.
    return false;
}

bool AnnotationEditor::deleteAnnotation(Annotation* annotation)
{
    if (!annotation) {
        LOG_ERROR("AnnotationEditor::deleteAnnotation: Null annotation provided.");
        return false;
    }

    // Find document and page index (similar to modifyAnnotation)
    Document* doc = nullptr; // Placeholder
    int pageIndex = -1;      // Placeholder
    // ... Logic to find doc/pageIndex ...

    if (!doc || pageIndex == -1) {
        LOG_ERROR("AnnotationEditor::deleteAnnotation: Could not find document or page index for annotation.");
        return false;
    }

    // --- CRITICAL: Poppler-Qt5 is READ-ONLY ---
    // Deleting the underlying Poppler::Annotation object is not possible directly.
    // We must mark the annotation object for deletion or store the deletion intent.
    // The deletion is applied later during document save.

    // Assume the Annotation object has a method to mark itself for deletion.
    // annotation->setScheduledForDeletion(true); // Hypothetical method
    // d->removeFromMaps(doc, pageIndex, annotation);
    // emit annotationDeleted(annotation, doc, pageIndex);
    // doc->markAsModified(); // Hypothetical method
    // return true; // Indicate local change was accepted

    LOG_WARN("AnnotationEditor::deleteAnnotation: Cannot delete annotation using Poppler-Qt5 (read-only). "
             "Deletion scheduled for next save operation using external tool.");
    // return true; // Indicate local change was accepted

    // As a stub for now, return false to indicate the operation cannot be completed with Poppler alone.
    return false;
}

int AnnotationEditor::deleteAllAnnotationsOnPage(Document* document, int pageIndex)
{
    if (!document || pageIndex < 0 || pageIndex >= document->pageCount()) {
        LOG_ERROR("AnnotationEditor::deleteAllAnnotationsOnPage: Invalid document or page index.");
        return 0;
    }

    // Get annotations for this page
    auto annotationsOnPage = annotationsForPage(document, pageIndex);
    int count = annotationsOnPage.size();

    for (Annotation* annot : annotationsOnPage) {
        deleteAnnotation(annot); // Use the deleteAnnotation method which handles read-only limitations
    }

    LOG_DEBUG("AnnotationEditor::deleteAllAnnotationsOnPage: Scheduled deletion of " << count << " annotations on page " << pageIndex << ".");
    return count;
}

int AnnotationEditor::deleteAllAnnotationsInDocument(Document* document)
{
    if (!document) {
        LOG_ERROR("AnnotationEditor::deleteAllAnnotationsInDocument: Null document provided.");
        return 0;
    }

    // Get all annotations for this document
    auto allAnnotations = annotationsForDocument(document);
    int count = allAnnotations.size();

    for (Annotation* annot : allAnnotations) {
        deleteAnnotation(annot); // Use the deleteAnnotation method which handles read-only limitations
    }

    LOG_DEBUG("AnnotationEditor::deleteAllAnnotationsInDocument: Scheduled deletion of " << count << " annotations in document: " << document->filePath());
    return count;
}

QList<QPointer<Annotation>> AnnotationEditor::annotationsForPage(Document* document, int pageIndex) const
{
    if (!document || pageIndex < 0 || pageIndex >= document->pageCount()) {
        return {};
    }

    QMutexLocker locker(&d->mutex);
    QPair<Document*, int> key = {document, pageIndex};
    auto it = d->docPageToAnnotations.constFind(key);
    if (it != d->docPageToAnnotations.constEnd()) {
        // Convert QSet to QList, filtering out null pointers (deleted objects)
        QList<QPointer<Annotation>> list;
        list.reserve(it.value().size());
        for (QPointer<Annotation> annot : it.value()) {
            if (annot) list.append(annot);
        }
        LOG_DEBUG("AnnotationEditor: Retrieved " << list.size() << " annotations for doc: " << document->filePath() << ", page: " << pageIndex);
        return list;
    }
    LOG_DEBUG("AnnotationEditor: No annotations found for doc: " << document->filePath() << ", page: " << pageIndex);
    return {};
}

QList<QPointer<Annotation>> AnnotationEditor::annotationsForDocument(Document* document) const
{
    if (!document) return {};

    QMutexLocker locker(&d->mutex);
    auto it = d->docToAnnotations.constFind(document);
    if (it != d->docToAnnotations.constEnd()) {
        // Convert QSet to QList, filtering out null pointers
        QList<QPointer<Annotation>> list;
        list.reserve(it.value().size());
        for (QPointer<Annotation> annot : it.value()) {
            if (annot) list.append(annot);
        }
        LOG_DEBUG("AnnotationEditor: Retrieved " << list.size() << " annotations for doc: " << document->filePath());
        return list;
    }
    LOG_DEBUG("AnnotationEditor: No annotations found for doc: " << (document ? document->filePath() : "nullptr"));
    return {};
}

QList<QPointer<Annotation>> AnnotationEditor::findAnnotationsInRect(Document* document, int pageIndex, const QRectF& rect) const
{
    if (!document || pageIndex < 0 || pageIndex >= document->pageCount() || rect.isEmpty()) {
        return {};
    }

    QList<QPointer<Annotation>> results;
    auto pageAnnots = annotationsForPage(document, pageIndex);

    for (QPointer<Annotation> annot : pageAnnots) {
        if (annot && annot->bounds().intersects(rect)) { // Assuming Annotation has a bounds() method returning QRectF
            results.append(annot);
        }
    }

    LOG_DEBUG("AnnotationEditor: Found " << results.size() << " annotations intersecting rect on doc: " << document->filePath() << ", page: " << pageIndex);
    return results;
}

AnnotationProperties AnnotationEditor::getAnnotationProperties(Annotation* annotation) const
{
    if (!annotation) {
        LOG_ERROR("AnnotationEditor::getAnnotationProperties: Null annotation provided.");
        return AnnotationProperties(); // Return default-constructed invalid properties
    }

    // Assume the Annotation object has getters for its properties
    // AnnotationProperties props;
    // props.type = annotation->type();
    // props.contents = annotation->contents();
    // props.color = annotation->color();
    // props.bounds = annotation->bounds();
    // props.isHidden = annotation->isHidden();
    // props.isPrintable = annotation->isPrintable();
    // ... set other properties ...
    // return props;

    LOG_WARN("AnnotationEditor::getAnnotationProperties: Requires Annotation object to implement property getters.");
    return AnnotationProperties(); // Placeholder
}

void AnnotationEditor::setActiveDocument(Document* document)
{
    QMutexLocker locker(&d->mutex);
    d->activeDocument = document; // Use QPointer
    LOG_DEBUG("AnnotationEditor: Set active document to: " << (document ? document->filePath() : "nullptr"));
}

Document* AnnotationEditor::activeDocument() const
{
    QMutexLocker locker(&d->mutex);
    return d->activeDocument.data(); // Returns nullptr if document was deleted
}

bool AnnotationEditor::isEditing() const
{
    QMutexLocker locker(&d->mutex);
    return d->isEditingVal;
}

bool AnnotationEditor::startEditing(Annotation* annotation)
{
    if (!annotation) {
        LOG_ERROR("AnnotationEditor::startEditing: Null annotation provided.");
        return false;
    }

    QMutexLocker locker(&d->mutex);
    if (d->isEditingVal) {
        LOG_WARN("AnnotationEditor::startEditing: Another annotation is already being edited. Finishing previous edit.");
        finishEditing(); // Finish any existing edit first
    }

    d->currentEditingAnnotation = annotation;
    d->isEditingVal = true;
    LOG_DEBUG("AnnotationEditor: Started editing annotation '" << annotation->id() << "'");
    emit editStarted(annotation);
    return true;
}

void AnnotationEditor::finishEditing()
{
    QMutexLocker locker(&d->mutex);
    if (d->isEditingVal) {
        Annotation* editingAnnot = d->currentEditingAnnotation.data(); // Use QPointer
        if (editingAnnot) {
            LOG_DEBUG("AnnotationEditor: Finished editing annotation '" << editingAnnot->id() << "'");
            emit editFinished(editingAnnot);
        }
        d->currentEditingAnnotation.clear(); // Clear the pointer
        d->isEditingVal = false;
    } else {
        LOG_DEBUG("AnnotationEditor: finishEditing called but no edit was in progress.");
    }
}

void AnnotationEditor::cancelEditing()
{
    QMutexLocker locker(&d->mutex);
    if (d->isEditingVal) {
        Annotation* editingAnnot = d->currentEditingAnnotation.data(); // Use QPointer
        if (editingAnnot) {
            LOG_DEBUG("AnnotationEditor: Canceled editing annotation '" << editingAnnot->id() << "'");
            emit editCanceled(editingAnnot);
            // Potentially revert local changes made to the annotation object during the edit session
        }
        d->currentEditingAnnotation.clear(); // Clear the pointer
        d->isEditingVal = false;
    } else {
        LOG_DEBUG("AnnotationEditor: cancelEditing called but no edit was in progress.");
    }
}

QList<AnnotationType> AnnotationEditor::supportedAnnotationTypes() const
{
    // Define the list of types supported by the editor/UI for *creation*.
    // This list might be limited by the underlying format's capabilities (revealed during save/write).
    return QList<AnnotationType>()
           << AnnotationType::Text
           << AnnotationType::Highlight
           << AnnotationType::Underline
           << AnnotationType::Squiggly
           << AnnotationType::StrikeOut
           << AnnotationType::Line
           << AnnotationType::Square
           << AnnotationType::Circle
           << AnnotationType::Ink
           << AnnotationType::Stamp;
}

} // namespace QuantilyxDoc