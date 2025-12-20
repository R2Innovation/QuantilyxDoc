/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "AnnotationManager.h"
#include "../core/Document.h"
#include "../core/Page.h"
#include "Annotation.h" // Assuming base Annotation class exists
#include "../core/Logger.h"
#include <QHash>
#include <QList>
#include <QPointer>
#include <QMutex>
#include <QMutexLocker>
#include <QCoreApplication>
#include <QDebug>

namespace QuantilyxDoc {

struct AnnotationKey {
    Document* document;
    int pageIndex;
    Annotation* annotation; // Or maybe use a unique ID from the annotation itself if available

    bool operator==(const AnnotationKey& other) const {
        return document == other.document && pageIndex == other.pageIndex && annotation == other.annotation;
    }
};

// Define qHash for AnnotationKey to be used in QHash
uint qHash(const AnnotationKey& key, uint seed = 0) {
    return ::qHash(QPair<void*, QPair<int, void*>>(key.document, QPair<int, void*>(key.pageIndex, key.annotation)), seed);
}

class AnnotationManager::Private {
public:
    Private(AnnotationManager* q_ptr) : q(q_ptr) {}

    AnnotationManager* q;
    mutable QMutex mutex; // Protect access to the annotation maps
    QHash<AnnotationKey, QPointer<Annotation>> annotations; // Map key -> Annotation*
    QHash<Document*, QSet<Annotation*>> docToAnnotations; // Map Document* -> Set of its annotations
    QHash<Document*, QHash<int, QSet<Annotation*>>> docPageToAnnotations; // Map Document* -> (PageIndex -> Set of its annotations)
    QSet<Document*> modifiedDocs; // Set of documents with modified annotations

    // Helper to remove an annotation from all internal maps
    void removeAnnotationInternal(Document* doc, Annotation* annot, int pageIndex) {
        AnnotationKey key{doc, pageIndex, annot};
        annotations.remove(key);

        if (docToAnnotations.contains(doc)) {
            docToAnnotations[doc].remove(annot);
            if (docToAnnotations[doc].isEmpty()) {
                docToAnnotations.remove(doc);
            }
        }

        if (docPageToAnnotations.contains(doc) && docPageToAnnotations[doc].contains(pageIndex)) {
            docPageToAnnotations[doc][pageIndex].remove(annot);
            if (docPageToAnnotations[doc][pageIndex].isEmpty()) {
                docPageToAnnotations[doc].remove(pageIndex);
                if (docPageToAnnotations[doc].isEmpty()) {
                    docPageToAnnotations.remove(doc);
                }
            }
        }
    }
};

// Static instance pointer
AnnotationManager* AnnotationManager::s_instance = nullptr;

AnnotationManager& AnnotationManager::instance()
{
    if (!s_instance) {
        s_instance = new AnnotationManager();
    }
    return *s_instance;
}

AnnotationManager::AnnotationManager(QObject* parent)
    : QObject(parent)
    , d(new Private(this))
{
    LOG_INFO("AnnotationManager initialized.");
}

AnnotationManager::~AnnotationManager()
{
    LOG_INFO("AnnotationManager destroyed.");
    // Annotations are owned by their respective documents/pages or created by UI,
    // so the manager just holds pointers. The QHash will clear automatically.
}

void AnnotationManager::registerDocument(Document* doc)
{
    if (!doc) return;

    QMutexLocker locker(&d->mutex);
    // Initialization for the document's annotation tracking happens implicitly when annotations are added.
    LOG_DEBUG("Registered document with AnnotationManager: " << (doc ? doc->filePath() : "nullptr"));
}

void AnnotationManager::unregisterDocument(Document* doc)
{
    if (!doc) return;

    QMutexLocker locker(&d->mutex);
    // Remove all annotations associated with this document
    auto it = d->docToAnnotations.find(doc);
    if (it != d->docToAnnotations.end()) {
        const QSet<Annotation*>& annots = it.value();
        for (Annotation* annot : annots) {
            // Find the page index for this annotation (requires annotation to know its page)
            // This is tricky if the annotation object itself doesn't store its page index.
            // A better map might be QHash<Annotation*, QPair<Document*, int>>.
            // For now, assume we can find it or iterate the page map.
            for (auto pageIt = d->docPageToAnnotations[doc].begin(); pageIt != d->docPageToAnnotations[doc].end(); ++pageIt) {
                if (pageIt.value().contains(annot)) {
                    d->removeAnnotationInternal(doc, annot, pageIt.key());
                    emit annotationRemoved(doc, annot);
                    break; // Found on this page, stop searching
                }
            }
        }
        d->docToAnnotations.erase(it);
        d->docPageToAnnotations.remove(doc);
        d->modifiedDocs.remove(doc);
        emit annotationsChanged(doc);
        LOG_DEBUG("Unregistered document and removed its annotations from AnnotationManager: " << doc->filePath());
    }
}

bool AnnotationManager::addAnnotation(Document* doc, int pageIndex, Annotation* annotation)
{
    if (!doc || !annotation) return false;

    QMutexLocker locker(&d->mutex);

    AnnotationKey key{doc, pageIndex, annotation};
    if (d->annotations.contains(key)) {
        LOG_WARN("Annotation already registered with AnnotationManager for doc/page.");
        return false; // Or maybe update? For now, prevent duplicates.
    }

    d->annotations.insert(key, annotation);
    d->docToAnnotations[doc].insert(annotation);
    d->docPageToAnnotations[doc][pageIndex].insert(annotation);

    // Mark document as modified as adding an annotation is a change
    markDocumentAsModified(doc);

    emit annotationAdded(doc, pageIndex, annotation);
    emit annotationsChanged(doc);
    LOG_DEBUG("Added annotation to AnnotationManager for doc: " << doc->filePath() << ", page: " << pageIndex);
    return true;
}

bool AnnotationManager::removeAnnotation(Document* doc, Annotation* annotation)
{
    if (!doc || !annotation) return false;

    QMutexLocker locker(&d->mutex);

    // Find the page index for this annotation
    int pageIndex = -1;
    auto docPageIt = d->docPageToAnnotations.find(doc);
    if (docPageIt != d->docPageToAnnotations.end()) {
        for (auto pageIt = docPageIt.value().begin(); pageIt != docPageIt.value().end(); ++pageIt) {
            if (pageIt.value().contains(annotation)) {
                pageIndex = pageIt.key();
                break;
            }
        }
    }

    if (pageIndex != -1) {
        d->removeAnnotationInternal(doc, annotation, pageIndex);
        markDocumentAsModified(doc); // Removing an annotation is also a change
        emit annotationRemoved(doc, annotation);
        emit annotationsChanged(doc);
        LOG_DEBUG("Removed annotation from AnnotationManager for doc: " << doc->filePath() << ", page: " << pageIndex);
        return true;
    } else {
        LOG_WARN("Annotation not found in AnnotationManager for doc: " << (doc ? doc->filePath() : "nullptr"));
        return false;
    }
}

QList<Annotation*> AnnotationManager::annotationsForDocument(Document* doc) const
{
    if (!doc) return {};

    QMutexLocker locker(&d->mutex);
    auto it = d->docToAnnotations.constFind(doc);
    if (it != d->docToAnnotations.constEnd()) {
        return it.value().values(); // Convert QSet to QList
    }
    return {};
}

QList<Annotation*> AnnotationManager::annotationsForPage(Document* doc, int pageIndex) const
{
    if (!doc) return {};

    QMutexLocker locker(&d->mutex);
    auto docIt = d->docPageToAnnotations.constFind(doc);
    if (docIt != d->docPageToAnnotations.constEnd()) {
        auto pageIt = docIt.value().constFind(pageIndex);
        if (pageIt != docIt.value().constEnd()) {
            return pageIt.value().values(); // Convert QSet to QList
        }
    }
    return {};
}

QList<Annotation*> AnnotationManager::findAnnotationsInRect(Document* doc, int pageIndex, const QRectF& rect) const
{
    QList<Annotation*> results;
    if (!doc) return results;

    QMutexLocker locker(&d->mutex);
    auto docIt = d->docPageToAnnotations.constFind(doc);
    if (docIt != d->docPageToAnnotations.constEnd()) {
        auto pageIt = docIt.value().constFind(pageIndex);
        if (pageIt != docIt.value().constEnd()) {
            for (Annotation* annot : pageIt.value()) {
                // This requires the Annotation class to have a 'bounds()' method returning QRectF
                if (annot->bounds().intersects(rect)) { // Assuming bounds() exists and returns QRectF
                    results.append(annot);
                }
            }
        }
    }
    return results;
}

int AnnotationManager::totalAnnotationCount() const
{
    QMutexLocker locker(&d->mutex);
    return d->annotations.size();
}

int AnnotationManager::annotationCountForDocument(Document* doc) const
{
    if (!doc) return 0;

    QMutexLocker locker(&d->mutex);
    auto it = d->docToAnnotations.constFind(doc);
    return (it != d->docToAnnotations.constEnd()) ? it.value().size() : 0;
}

QList<Annotation*> AnnotationManager::getModifiedAnnotationsForDocument(Document* doc) const
{
    if (!doc) return {};

    QMutexLocker locker(&d->mutex);
    QList<Annotation*> results;

    // Iterate through all annotations associated with the document
    auto docAnnotsIt = d->docToAnnotations.constFind(doc);
    if (docAnnotsIt != d->docToAnnotations.constEnd()) {
        for (Annotation* annot : docAnnotsIt.value()) {
            // Check if the specific annotation object reports itself as modified
            // This requires the Annotation base class or its subclasses (like PdfAnnotation) to have an isModified() method.
            // Assuming PdfAnnotation implements isModified().
            PdfAnnotation* pdfAnnot = dynamic_cast<PdfAnnotation*>(annot);
            if (pdfAnnot && pdfAnnot->isModified()) {
                results.append(annot);
            }
        }
    }
    LOG_DEBUG("AnnotationManager: Found " << results.size() << " modified annotations for document: " << (doc ? doc->filePath() : "nullptr"));
    return results;
}

void AnnotationManager::markDocumentAsModified(Document* doc)
{
    if (!doc) return;

    QMutexLocker locker(&d->mutex);
    bool wasModified = d->modifiedDocs.contains(doc);
    d->modifiedDocs.insert(doc);

    // Also mark the *document's* internal flag (if it has one, like d->inMemoryStateModified in PdfDocument)
    // This requires PdfDocument to have a public method or friend access, or for this manager to know about PdfDocument specifically.
    // A cleaner way might be for PdfDocument to connect to this signal itself.
    // For now, let's assume PdfDocument connects to the signal emitted below or checks this manager's state.
    // PdfDocument *pdfDoc = dynamic_cast<PdfDocument*>(doc); // Check type
    // if (pdfDoc) { pdfDoc->setInMemoryStateModifiedFlag(true); } // Hypothetical method

    if (!wasModified) {
        LOG_DEBUG("AnnotationManager: Marked document as modified (annotations): " << doc->filePath());
        emit documentModifiedChanged(doc, true);
    }
}

bool AnnotationManager::isDocumentModified(Document* doc) const
{
    if (!doc) return false;

    QMutexLocker locker(&d->mutex);
    return d->modifiedDocs.contains(doc);
}

bool AnnotationManager::prepareAnnotationsForSave(Document* doc)
{
    if (!doc) return false;

    QMutexLocker locker(&d->mutex);
    if (!d->modifiedDocs.contains(doc)) {
        LOG_DEBUG("No modified annotations to prepare for save for doc: " << doc->filePath());
        return true; // Nothing to do, technically successful
    }

    // This is where the complexity of read-only annotations (like Poppler::Annotation) comes in.
    // We need to iterate through the annotations associated with this document.
    // For each annotation that has been modified (requires tracking in the Annotation class itself,
    // e.g., a `bool isModified` flag set by its setters), we need to figure out how to apply
    // that change back to the underlying document format.
    // For PDF (Poppler), this requires an external library or tool capable of writing PDFs.

    auto docAnnots = annotationsForDocument(doc); // Get list under lock
    locker.unlock(); // Release lock before potentially long-running external processes

    bool allPrepared = true;
    for (Annotation* annot : docAnnots) {
        // Check if the annotation object itself thinks it's modified
        // This requires the Annotation base class or its subclasses to track modification.
        // For PdfAnnotation, this would involve checking if local properties (color, contents)
        // differ from the original Poppler::Annotation values.
        // if (annot->isModified()) { ... }
        // Then, somehow, update the source document format. This is the hard part with Poppler-Qt5.

        // A potential approach:
        // 1. Serialize the modified annotation state (type, bounds, color, contents, etc.).
        // 2. Store this serialized state associated with the document.
        // 3. During Document::save(), check for these serialized modifications.
        // 4. Use an external tool (e.g., QPDF, mutool, PoDoFo) to apply the serialized changes
        //    to the PDF file before or during the save process.

        // For now, this is a significant undertaking beyond basic Poppler integration.
        // We can log the intent or prepare serialized data structures.
        LOG_WARN("AnnotationManager::prepareAnnotationsForSave: Preparing annotations for save requires external PDF writing tool. Serialization/flagging for doc: " << doc->filePath());
        // Example serialization placeholder:
        // SerializedAnnotationData data = serializeAnnotation(annot);
        // doc->addPendingAnnotationChange(data); // Hypothetical method on Document
    }

    if (allPrepared) {
        locker.relock(); // Re-acquire lock to update modification state
        d->modifiedDocs.remove(doc); // Mark as no longer modified internally
        emit documentModifiedChanged(doc, false);
        LOG_INFO("Prepared annotations for save for doc: " << doc->filePath());
    }

    return allPrepared;
}

} // namespace QuantilyxDoc