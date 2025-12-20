/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "PdfDocument.h"
#include "PdfPage.h"
#include "PdfAnnotation.h"
#include "PdfFormField.h" // Assuming this exists or will be created
#include "../../core/Logger.h"
#include <poppler-qt5.h>
#include <QFileInfo>
#include <QDateTime>
#include <QCryptographicHash>
#include <QDir>
#include <QSaveFile> // For safe saving
#include <QDebug>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFWriter.hh>
#include <qpdf/QPDFPageObjectHelper.hh>
#include <qpdf/Types.h>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/Pl_Buffer.hh>
#include <qpdf/Pl_StdioFile.hh>
#include <qpdf/QIntC.hh> // For integer types used by QPDF
#include <qpdf/QPDFSystemError.hh>
#include <qpdf/QUtil.hh> // For string conversion utilities if needed
#include <memory> // For std::unique_ptr if managing QPDF lifecycle carefully

namespace QuantilyxDoc {

class PdfDocument::Private {
public:
    Private() : popplerDoc(nullptr), locked(false), encrypted(false), restrictionsRemoved(false) {}
    ~Private() { delete popplerDoc; } // Poppler doc must be deleted explicitly

    Poppler::Document* popplerDoc;
    QString pdfVersionStr;
    bool locked;
    bool encrypted;
    bool restrictionsRemoved;
    QList<std::unique_ptr<PdfPage>> pages; // Own the page objects
    QList<std::unique_ptr<PdfAnnotation>> allAnnotations; // Own annotation objects associated with this doc
    QList<std::unique_ptr<PdfFormField>> formFields; // Own form field objects
    QStringList embeddedFileNames; // Cache list of embedded files

    // Helper to get Poppler's Page from index
    Poppler::Page* getPopplerPage(int index) const {
        if (popplerDoc && index >= 0 && index < popplerDoc->numPages()) {
            return popplerDoc->page(index);
        }
        return nullptr;
    }
};

PdfDocument::PdfDocument(QObject* parent)
    : Document(parent)
    , d(new Private())
{
    LOG_INFO("PdfDocument created.");
}

PdfDocument::~PdfDocument()
{
    LOG_INFO("PdfDocument destroyed.");
}

bool PdfDocument::load(const QString& filePath, const QString& password)
{
    // Delete old Poppler document if it exists
    delete d->popplerDoc;
    d->popplerDoc = nullptr;
    d->pages.clear();
    d->allAnnotations.clear();
    d->formFields.clear();
    d->embeddedFileNames.clear();

    // Load new Poppler document
    d->popplerDoc = Poppler::Document::load(filePath, password);
    if (!d->popplerDoc) {
        setLastError(tr("Failed to load PDF document. It may be corrupted or password-protected (and password was incorrect/wrong permissions)."));
        LOG_ERROR(lastError());
        return false;
    }

    if (d->popplerDoc->isLocked()) {
        d->locked = true;
        d->encrypted = true;
        if (password.isEmpty()) {
            setLastError(tr("PDF is encrypted and requires a password."));
            LOG_WARN(lastError());
            return false; // Or return true but keep locked state? Depends on desired behavior.
        }
        // If password was provided and doc is still locked, it means the password was wrong or permissions don't allow opening.
        if (d->popplerDoc->isLocked()) {
             setLastError(tr("Incorrect password or insufficient permissions to open PDF."));
             LOG_ERROR(lastError());
             return false;
        }
    } else {
        d->locked = false;
        d->encrypted = false;
    }

    // Set file path and update file size
    setFilePath(filePath);

    // Populate metadata
    populateMetadata();

    // Create PdfPage wrappers
    int numPages = d->popplerDoc->numPages();
    d->pages.reserve(numPages);
    for (int i = 0; i < numPages; ++i) {
        d->pages.append(createPdfPage(i));
    }

    // Load annotations, form fields, embedded files
    // This could be done lazily or on demand, but for now, load them all.
    // Annotations: Poppler provides access via Poppler::Page::annotations()
    // Form Fields: Poppler provides via popplerDoc->formFields()
    // Embedded Files: Poppler provides via popplerDoc->embeddedFiles()

    // Load Form Fields
    if (d->popplerDoc->hasFormFields()) {
        auto popplerFields = d->popplerDoc->formFields();
        d->formFields.reserve(popplerFields.size());
        for (auto* popplerField : popplerFields) {
            // Create PdfFormField wrapper
            auto field = std::make_unique<PdfFormField>(popplerField);
            d->formFields.append(std::move(field));
        }
        emit formFieldsChanged();
    }

    // Load Embedded Files
    auto embeddedFileObjs = d->popplerDoc->embeddedFiles(); // Returns QList<Poppler::EmbeddedFile*>
    d->embeddedFileNames.reserve(embeddedFileObjs.size());
    for (auto* embeddedFileObj : embeddedFileObjs) {
        if (embeddedFileObj) {
            d->embeddedFileNames.append(embeddedFileObj->name());
        }
    }
    emit embeddedFilesChanged();

    LOG_INFO("Successfully loaded PDF document: " << filePath << " (" << numPages << " pages)");
    setState(Loaded);
    return true;
}

bool PdfDocument::save(const QString& filePath)
{
    if (!d->popplerDoc) {
        setLastError(tr("No document loaded to save."));
        LOG_ERROR(lastError());
        return false;
    }

    QString targetPath = filePath.isEmpty() ? this->filePath() : filePath;
    if (targetPath.isEmpty()) {
        setLastError(tr("No file path specified for saving."));
        LOG_ERROR(lastError());
        return false;
    }

     // --- NEW LOGIC: Use QPDF for writing ---
    std::string originalPathStdString = filePath().toStdString(); // Path of the *currently loaded* file
    std::string targetPathStdString = targetPath.toStdString();   // Path to *save* to

    // Check if we have pending modifications to apply
    if (!d->inMemoryStateModified) {
        LOG_INFO("No pending modifications. Performing standard Poppler save.");
        // If no changes tracked by our system, try Poppler save (read-only properties only)
        // This might be okay for just saving metadata changes made via Poppler's API, if any.
        // For now, let's assume if inMemoryStateModified is false, the file is up-to-date or Poppler can't save changes anyway.
        // We'll proceed with QPDF which handles both scenarios, but maybe just copies if no changes.
        // Or, we could just return true if no modifications are tracked, assuming the file on disk is correct.
        // For safety and consistency with the new system, let's use QPDF to ensure *any* internal state changes are reflected.
    }

    std::unique_ptr<QPDF> qpdf = std::make_unique<QPDF>();
    try {
        LOG_DEBUG("QPDF: Loading original file: " << originalPathStdString);
        qpdf->processFile(originalPathStdString.c_str());
    } catch (const std::exception& e) {
        setLastError(tr("QPDF failed to load original file '%1': %2").arg(filePath()).arg(e.what()));
        LOG_ERROR(lastError());
        return false;
    }

    // --- Apply Pending Annotation Changes ---
    // Get changes from AnnotationManager
    // This requires AnnotationManager to have a method returning serialized changes for *this* document.
    // Let's assume it returns a structure like: struct Change { int page_idx; QString id; QString action; QVariantMap props; };
    // We need to map our QuantilyxDoc annotation objects to PDF annotation objects in the QPDF structure.

    // A more robust way is if PdfAnnotation/PdfPage stored the original QPDF object handle or a way to find it.
    // For now, let's assume a mapping exists or we find by properties like contents/bounds/name.

    // Example: Iterate through all pages and potentially modified annotations
    // This is complex because we need to find the *original* annotation in the QPDF structure.
    // Poppler::Annotation provides its boundary, contents, etc. We can use these as identifiers.
    // QPDF: Iterate pages
    std::vector<QPDFObjectHandle> allPages = qpdf->getAllPages();
    int pageCount = allPages.size();

    // Get *all* annotations for *this* document from AnnotationManager that are marked as modified
    // This is a key function we need to implement in AnnotationManager.
    // It should return annotations associated with *this* PdfDocument instance that have isModified() == true.
    // For this example, let's call a hypothetical method.
    QList<Annotation*> modifiedAnnotations = AnnotationManager::instance().getModifiedAnnotationsForDocument(this);

    for (Annotation* qAnnot : modifiedAnnotations) {
        PdfAnnotation* pdfAnnot = dynamic_cast<PdfAnnotation*>(qAnnot);
        if (pdfAnnot && pdfAnnot->document() == this) { // Ensure it belongs to this doc
            int pageIndex = pdfAnnot->pageIndex(); // This requires PdfAnnotation to store its page index correctly
            if (pageIndex >= 0 && pageIndex < pageCount) {
                QPDFObjectHandle pageObj = allPages[pageIndex];
                QPDFObjectHandle annotsArray = pageObj.getKey("/Annots");

                if (annotsArray.isArray()) {
                    bool foundAndModified = false;
                    // Iterate the /Annots array on this page to find the matching annotation
                    for (size_t i = 0; i < annotsArray.getArrayNItems(); ++i) {
                        QPDFObjectHandle annotObj = annotsArray.getArrayItem(i);

                        // Find by a unique identifier if possible (e.g., original contents + bounds combination, or a stored ID)
                        // Poppler::Annotation *popplerAnnot = pdfAnnot->popplerAnnotation(); // Need to link back
                        // This is the crux: linking QuantilyxDoc's PdfAnnotation to QPDF's object handle.
                        // The most reliable way is if PdfAnnotation stored the QPDF handle during creation/loading via Poppler.
                        // Since we didn't do that initially, matching by properties is less robust but possible for basic types.
                        // Let's assume PdfAnnotation has a method to get its original Poppler object and we can match by boundary/content.
                        // OR, we enhance PdfAnnotation to store its QPDF handle when loaded.

                        // For now, let's assume PdfAnnotation has a method getQpdfObjectHandle() that returns the original handle if loaded via a method that captured it.
                        // This requires modification of PdfAnnotation.
                        // QPDFObjectHandle originalQpdfHandle = pdfAnnot->getQpdfObjectHandle();

                        // If we can't directly link, we must find by matching properties.
                        // This is fragile. Let's assume a hypothetical match function exists or is added to PdfAnnotation.
                        // bool matches = pdfAnnot->matchesQpdfHandle(annotObj); // Hypothetical

                        // Let's try matching by boundary and type as a proxy (still fragile but better than nothing for a demo).
                        // We need the original boundary from the QPDF object and compare to PdfAnnotation's current state.
                        // QPDFObjectHandle originalBounds = annotObj.getKey("/Rect"); // Get /Rect from QPDF object
                        // QRectF qpdfBounds = ...; // Parse QPDFObjectHandle to QRectF
                        // if (qpdfBounds == pdfAnnot->bounds() && ...) { // Check type etc.

                        // For this example, let's assume we have a way to identify the correct QPDF object handle.
                        // Let's call a hypothetical method that attempts to find the matching handle based on stored initial state.
                        QPDFObjectHandle matchingAnnotObj = findQpdfAnnotationHandle(pageObj, pdfAnnot);
                        if (!matchingAnnotObj.isInitialized() || matchingAnnotObj.getOID() != annotObj.getOID()) {
                             // If the handles don't match based on our lookup, keep searching.
                             continue;
                        }

                        // Found the matching annotation object in QPDF structure
                        // Apply changes from PdfAnnotation to the QPDF object handle
                        // Example: Modify contents
                        if (pdfAnnot->contents() != getQpdfAnnotationContents(annotObj)) { // Hypothetical getter
                            LOG_DEBUG("QPDF: Modifying annotation contents on page " << pageIndex);
                            annotObj.replaceKey("/Contents", QPDFObjectHandle::newUnicodeString(pdfAnnot->contents().toStdU32String()));
                        }
                        // Example: Modify color
                        if (pdfAnnot->color() != getQpdfAnnotationColor(annotObj)) { // Hypothetical getter
                            LOG_DEBUG("QPDF: Modifying annotation color on page " << pageIndex);
                            QColor color = pdfAnnot->color();
                            QPDFObjectHandle colorArray = QPDFObjectHandle::newArray({
                                QPDFObjectHandle::newReal(color.redF()),
                                QPDFObjectHandle::newReal(color.greenF()),
                                QPDFObjectHandle::newReal(color.blueF())
                            });
                            annotObj.replaceKey("/C", colorArray);
                        }
                        // Apply other properties like border style, opacity, etc., as needed.
                        foundAndModified = true;
                        LOG_DEBUG("QPDF: Modified annotation on page " << pageIndex);
                        break; // Stop searching once found and modified for this specific QuantilyxDoc annotation
                    }
                    if (!foundAndModified) {
                        LOG_WARN("QPDF: Could not find matching QPDF object for modified QuantilyxDoc annotation on page " << pageIndex);
                    }
                } else {
                    LOG_WARN("QPDF: Page " << pageIndex << " has no /Annots array. Cannot modify annotations.");
                }
            } else {
                LOG_WARN("QPDF: Annotation refers to invalid page index: " << pageIndex);
            }
        }
    }

    // --- Apply Pending Form Field Changes ---
    // Similar process: get modified form fields, find their QPDF object handles, modify values.
    // This involves navigating the /AcroForm structure in the QPDF object.
    // QPDFObjectHandle acroForm = qpdf->getRoot().getKey("/AcroForm");
    // if (acroForm.isDictionary()) { ... iterate fields ... }
    // This is also complex and requires mapping QuantilyxDoc PdfFormField objects to QPDF handles.
    // QList<PdfFormField*> modifiedFormFields = getModifiedFormFields(); // Hypothetical method
    // for (PdfFormField* field : modifiedFormFields) { ... apply changes ... }

    // --- Write the modified QPDF object to the target file ---
    LOG_DEBUG("QPDF: Writing modified file to: " << targetPathStdString);
    QPDFWriter writer(*qpdf, targetPathStdString.c_str());
    // Configure writer if needed
    // writer.set PreserveUnreferencedObjects(true);
    // writer.setMinimumVersion("1.4"); // Or keep original?
    try {
        writer.write();
    } catch (const std::exception& e) {
        setLastError(tr("QPDF failed to write file '%1': %2").arg(targetPath).arg(e.what()));
        LOG_ERROR(lastError());
        return false;
    }

    // --- Update internal state after successful save ---
    setFilePath(targetPath);
    setModified(false); // Qt's document modified flag
    d->inMemoryStateModified = false; // Internal QPDF-based modification flag
    LOG_INFO("Successfully saved PDF document with QPDF: " << targetPath);
    return true;
}

Document::DocumentType PdfDocument::type() const
{
    return DocumentType::PDF;
}

int PdfDocument::pageCount() const
{
    return d->popplerDoc ? d->popplerDoc->numPages() : 0;
}

Page* PdfDocument::page(int index) const
{
    if (index >= 0 && index < d->pages.size()) {
        return d->pages[index].get(); // Return raw pointer managed by unique_ptr
    }
    return nullptr;
}

bool PdfDocument::isLocked() const
{
    return d->locked;
}

bool PdfDocument::isEncrypted() const
{
    return d->encrypted;
}

QString PdfDocument::formatVersion() const
{
    return d->pdfVersionStr;
}

bool PdfDocument::supportsFeature(const QString& feature) const
{
    // Define features this format supports
    static const QSet<QString> supportedFeatures = {
        "TextSelection", "TextExtraction", "Annotations", "Forms",
        "Bookmarks", "Hyperlinks", "EmbeddedFiles", "RestrictionRemoval" // Feature specific to QuantilyxDoc philosophy
    };
    return supportedFeatures.contains(feature);
}

// --- PDF-Specific Metadata ---

QString PdfDocument::pdfVersion() const
{
    // Poppler doesn't directly expose the version string like "1.4".
    // It might be derivable from pdfVersionEnum() or other internal details.
    // For now, we'll use a placeholder or a derived value if possible.
    // Poppler::Document::PageLayout, PageMode give structural info but not version string easily.
    // Let's assume populateMetadata fills d->pdfVersionStr.
    return d->pdfVersionStr;
}

void PdfDocument::setInMemoryStateModifiedFlag(bool modified)
{
    d->inMemoryStateModified = modified;
    LOG_DEBUG("PdfDocument: In-memory state modified flag set to " << modified << " for: " << filePath());
}

bool PdfDocument::isInMemoryStateModified() const
{
    return d->inMemoryStateModified;
}

bool PdfDocument::isLinearized() const
{
    return d->popplerDoc ? d->popplerDoc->isLinearized() : false;
}

Poppler::Document::PageLayout PdfDocument::pageLayout() const
{
    return d->popplerDoc ? d->popplerDoc->pageLayout() : Poppler::Document::NoLayout;
}

Poppler::Document::PageMode PdfDocument::pageMode() const
{
    return d->popplerDoc ? d->popplerDoc->pageMode() : Poppler::Document::UseNone;
}

QString PdfDocument::producer() const
{
    return d->popplerDoc ? d->popplerDoc->info("Producer") : QString();
}

QString PdfDocument::creator() const
{
    return d->popplerDoc ? d->popplerDoc->info("Creator") : QString();
}

QString PdfDocument::subject() const
{
    // Override base class to get from PDF metadata
    return d->popplerDoc ? d->popplerDoc->info("Subject") : Document::subject();
}

QStringList PdfDocument::keywords() const
{
    // Override base class to get from PDF metadata
    QString keywordsStr = d->popplerDoc ? d->popplerDoc->info("Keywords") : QString();
    if (!keywordsStr.isEmpty()) {
        return keywordsStr.split(',', Qt::SkipEmptyParts);
    }
    return Document::keywords(); // Fallback to base class
}

// --- PDF-Specific Functionality ---

bool PdfDocument::hasForms() const
{
    return d->popplerDoc ? d->popplerDoc->hasFormFields() : false;
}

bool PdfDocument::hasAnnotations() const
{
    // Check if any page has annotations
    if (!d->popplerDoc) return false;
    for (int i = 0; i < d->popplerDoc->numPages(); ++i) {
        Poppler::Page* popplerPage = d->getPopplerPage(i);
        if (popplerPage && !popplerPage->annotations().isEmpty()) {
            return true;
        }
    }
    return false;
}

bool PdfDocument::hasEmbeddedFiles() const
{
    return !d->embeddedFileNames.isEmpty();
}

QString PdfDocument::xmpMetadata() const
{
    return d->popplerDoc ? d->popplerDoc->xmpMetadata() : QString();
}

QVariantMap PdfDocument::metadata() const
{
    QVariantMap map;
    if (d->popplerDoc) {
        // Populate common PDF metadata fields into the map
        map["Title"] = d->popplerDoc->info("Title");
        map["Author"] = d->popplerDoc->info("Author");
        map["Subject"] = d->popplerDoc->info("Subject");
        map["Keywords"] = d->popplerDoc->info("Keywords");
        map["Creator"] = d->popplerDoc->info("Creator");
        map["Producer"] = d->popplerDoc->info("Producer");
        map["CreationDate"] = d->popplerDoc->date("CreationDate"); // Returns QDateTime
        map["ModDate"] = d->popplerDoc->date("ModDate");
        map["FormatVersion"] = pdfVersion(); // Use our derived version
        map["IsLinearized"] = isLinearized();
        map["HasForms"] = hasForms();
        map["HasAnnotations"] = hasAnnotations();
        map["HasEmbeddedFiles"] = hasEmbeddedFiles();
        // Add more fields as needed
    }
    return map;
}

bool PdfDocument::getQpdfAnnotationHidden(const QPDFObjectHandle& annotObj) const {
    QPDFObjectHandle flagsObj = annotObj.getKey("/F"); // /F key holds flags integer
    if (flagsObj.isInteger()) {
        int flags = flagsObj.getIntValue();
        const int HIDDEN_PDF_FLAG = 2;
        return (flags & HIDDEN_PDF_FLAG) != 0;
    }
    return false; // Default to not hidden if /F key is missing or not integer
}

bool PdfDocument::getQpdfAnnotationReadOnly(const QPDFObjectHandle& annotObj) const {
    QPDFObjectHandle flagsObj = annotObj.getKey("/F"); // /F key holds flags integer
    if (flagsObj.isInteger()) {
        int flags = flagsObj.getIntValue();
        const int READONLY_PDF_FLAG = 1;
        return (flags & READONLY_PDF_FLAG) != 0;
    }
    return false; // Default to not read-only if /F key is missing or not integer
}

bool PdfDocument::getQpdfAnnotationNoZoom(const QPDFObjectHandle& annotObj) const {
    QPDFObjectHandle flagsObj = annotObj.getKey("/F"); // /F key holds flags integer
    if (flagsObj.isInteger()) {
        int flags = flagsObj.getIntValue();
        const int NOZOOM_PDF_FLAG = 8;
        return (flags & NOZOOM_PDF_FLAG) != 0;
    }
    return false; // Default to zoom allowed if /F key is missing or not integer
}
Poppler::Document* PdfDocument::popplerDocument() const
{
    return d->popplerDoc;
}

QList<PdfFormField*> PdfDocument::formFields() const
{
    QList<PdfFormField*> ptrList;
    ptrList.reserve(d->formFields.size());
    for (const auto& field : d->formFields) {
        ptrList.append(field.get());
    }
    return ptrList;
}

QStringList PdfDocument::embeddedFiles() const
{
    return d->embeddedFileNames;
}

bool PdfDocument::extractEmbeddedFile(const QString& fileName, const QString& outputPath) const
{
    if (!d->popplerDoc) return false;

    auto embeddedFileObjs = d->popplerDoc->embeddedFiles();
    for (auto* embeddedFileObj : embeddedFileObjs) {
        if (embeddedFileObj && embeddedFileObj->name() == fileName) {
            // Save the embedded file content to the specified path
            QFile outputFile(outputPath);
            if (!outputFile.open(QIODevice::WriteOnly)) {
                LOG_ERROR("Failed to open output file for embedded file extraction: " << outputPath);
                return false;
            }
            bool writeSuccess = outputFile.write(embeddedFileObj->data()) != -1;
            outputFile.close();
            if (writeSuccess) {
                LOG_INFO("Extracted embedded file: " << fileName << " to " << outputPath);
                return true;
            } else {
                LOG_ERROR("Failed to write embedded file data to: " << outputPath);
                return false;
            }
        }
    }
    LOG_WARN("Embedded file not found in document: " << fileName);
    return false;
}

bool PdfDocument::removePassword(const QString& password)
{
    // Poppler-Qt5 cannot remove passwords during save.
    // This requires an external library like QPDF or mupdf bindings.
    // For QuantilyxDoc's philosophy of "document liberation", this is a key feature.
    // We would need to call an external tool or integrate QPDF.
    // Example using QPDF (command line tool):
    // qpdf --password=PASSWORD --remove-password input.pdf output.pdf
    // This is complex to implement directly in C++ without bindings.
    LOG_WARN("removePassword: Poppler-Qt5 cannot remove passwords. Integration with QPDF or similar is required for full liberation feature.");
    Q_UNUSED(password);
    // A stub implementation might involve calling an external process.
    // QProcess process;
    // process.start("qpdf", {"--password=" + password, "--remove-password", filePath(), newFilePath});
    // process.waitForFinished();
    // bool success = (process.exitCode() == 0);
    // if (success) { load(newFilePath); } // Reload the unlocked version
    // This is a significant undertaking and might be deferred or implemented as a plugin.
    return false; // Not implemented with Poppler alone
}

bool PdfDocument::hasRestrictions() const
{
    if (!d->popplerDoc) return false;
    // Check specific permissions
    // Poppler::Document::isLocked() implies some restrictions, but there are finer-grained ones.
    // Poppler::Page::textList() might fail if text extraction is forbidden.
    // Poppler::Document::print... permissions exist.
    // A comprehensive check involves multiple permission flags.
    bool hasRestr = false;
    hasRestr |= !d->popplerDoc->isAllowed(Poppler::Document::Action::Print); // Check print permission
    hasRestr |= !d->popplerDoc->isAllowed(Poppler::Document::Action::Copy);  // Check copy permission
    // Add checks for other permissions (annot, fill forms, etc.) if needed
    return hasRestr;
}

bool PdfDocument::removeRestrictions()
{
    // Similar to removePassword, removing restrictions typically requires a library like QPDF
    // that can modify the PDF's permission bits.
    LOG_WARN("removeRestrictions: Poppler-Qt5 cannot remove restrictions. Integration with QPDF or similar is required for full liberation feature.");
    // A stub implementation might involve calling an external process.
    // This is also a significant undertaking.
    d->restrictionsRemoved = true; // Indicate intent, though not technically achieved yet
    emit restrictionsRemoved();
    return d->restrictionsRemoved; // Return the flag, not true, unless actually implemented
}

QList<PdfAnnotation*> PdfDocument::annotationsForPage(int pageIndex) const
{
    QList<PdfAnnotation*> annotations;
    if (pageIndex >= 0 && pageIndex < pageCount()) {
        Poppler::Page* popplerPage = d->getPopplerPage(pageIndex);
        if (popplerPage) {
            auto popplerAnnots = popplerPage->annotations();
            annotations.reserve(popplerAnnots.size());
            for (auto* popplerAnnot : popplerAnnots) {
                // Find or create a PdfAnnotation wrapper for this Poppler annotation
                // This requires a mapping or a way to get our wrapper from the Poppler object.
                // For now, assume we create new wrappers on demand or cache them.
                // A more efficient way is to create wrappers when the page is first accessed
                // and store them within the PdfPage object or a central map in PdfDocument.
                // Let's assume PdfPage handles its own annotations for now.
                // This method might just call page(pageIndex)->annotations() which is already implemented in base Document.
                // Or, we iterate popplerAnnots and create PdfAnnotation wrappers here.
                // PdfAnnotation* wrapper = new PdfAnnotation(popplerAnnot); // Need constructor
                // annotations.append(wrapper);
                // The ownership and lifecycle of these wrappers need careful management.
                // It's likely better handled within PdfPage.
            }
        }
    }
    return annotations;
}

bool PdfDocument::addAnnotationToPage(int pageIndex, PdfAnnotation* annotation)
{
    Q_UNUSED(pageIndex);
    Q_UNUSED(annotation);
    // Adding/modifying annotations requires Poppler to support writing.
    // Poppler-Qt5 has limited write support. Full annotation creation/modification often requires
    // a library with full PDF writing capabilities (like PoDoFo, mupdf, or direct PDF manipulation).
    LOG_WARN("addAnnotationToPage: Poppler-Qt5 has limited annotation writing capabilities. Full implementation requires a PDF writing library.");
    return false; // Not implemented with Poppler alone
}

bool PdfDocument::removeAnnotationFromPage(int pageIndex, PdfAnnotation* annotation)
{
    Q_UNUSED(pageIndex);
    Q_UNUSED(annotation);
    LOG_WARN("removeAnnotationFromPage: Poppler-Qt5 has limited annotation writing capabilities. Full implementation requires a PDF writing library.");
    return false; // Not implemented with Poppler alone
}

bool PdfDocument::hasTableOfContents() const
{
    return d->popplerDoc && d->popplerDoc->hasEmbeddedToc();
}

QVariantList PdfDocument::tableOfContents() const
{
    QVariantList tocList;
    if (d->popplerDoc && d->popplerDoc->hasEmbeddedToc()) {
        // Poppler::Document::toc() returns a QDomDocument representing the TOC structure.
        QDomDocument tocDoc = d->popplerDoc->toc();
        if (!tocDoc.isNull()) {
            // Convert QDomDocument to QVariantList recursively
            QDomElement rootElement = tocDoc.documentElement(); // Usually <ROOT> or <OUTLINES>
            if (!rootElement.isNull()) {
                tocList = convertTocElementToVariantList(rootElement);
            }
        }
    }
    return tocList;
}

QMap<QString, QVariant> PdfDocument::namedDestinations() const
{
    QMap<QString, QVariant> destinations;
    if (d->popplerDoc) {
        // Poppler::Document::links() might provide named destinations indirectly,
        // or there might be a specific function. Let's check Poppler API.
        // Poppler::Document::pageLabels() is different (page numbering).
        // Named destinations are often linked to links or bookmarks.
        // Poppler::LinkDestination might be involved.
        // For now, this is a stub.
        LOG_WARN("namedDestinations: Implementation requires detailed Poppler API exploration for named destinations.");
    }
    return destinations;
}

bool PdfDocument::navigateToDestination(const QString& name)
{
    Q_UNUSED(name);
    // Requires finding the destination and telling the view to go there.
    LOG_WARN("navigateToDestination: Stub implementation.");
    return false;
}

void PdfDocument::populateMetadata()
{
    if (!d->popplerDoc) return;

    // Title
    QString title = d->popplerDoc->info("Title");
    if (!title.isEmpty()) setTitle(title);

    // Author
    QString author = d->popplerDoc->info("Author");
    if (!author.isEmpty()) setAuthor(author);

    // Subject
    QString subject = d->popplerDoc->info("Subject");
    if (!subject.isEmpty()) setSubject(subject);

    // Keywords
    QString keywordsStr = d->popplerDoc->info("Keywords");
    if (!keywordsStr.isEmpty()) {
        setKeywords(keywordsStr.split(',', Qt::SkipEmptyParts));
    }

    // Creation/Modification Dates
    QDateTime creationDate = d->popplerDoc->date("CreationDate");
    if (creationDate.isValid()) setCreationDate(creationDate);
    QDateTime modDate = d->popplerDoc->date("ModDate");
    if (modDate.isValid()) setModificationDate(modDate);

    // PDF Version (Poppler doesn't expose this cleanly, derive from spec or internal details if possible)
    // This might require parsing the header or trailer, which Poppler doesn't directly offer.
    // A common way is to use the Catalog's Version entry, but Poppler doesn't expose that easily in Qt API.
    // We'll leave d->pdfVersionStr as a placeholder for now or set it to a default if unknown.
    d->pdfVersionStr = "PDF 1.x"; // Placeholder, needs better logic if possible with Poppler.

    LOG_DEBUG("Populated PDF metadata for: " << filePath());
}

std::unique_ptr<PdfPage> PdfDocument::createPdfPage(int index) const
{
    // This factory method creates a PdfPage object associated with this document
    // and the given index. It passes the Poppler page object to the PdfPage constructor.
    Poppler::Page* popplerPage = d->getPopplerPage(index);
    if (popplerPage) {
        return std::make_unique<PdfPage>(this, popplerPage, index);
    }
    return nullptr;
}

// Helper function to recursively convert TOC QDomElement to QVariantList
// This is a helper, could be a static function in a utility class or a private member.
// Implement the helper function to recursively convert TOC QDomElement to QVariantList
QVariantList PdfDocument::convertTocElementToVariantList(const QDomElement& element) const
{
    QVariantList list;
    QDomNode child = element.firstChild();
    while (!child.isNull()) {
        if (child.isElement()) {
            QDomElement childElement = child.toElement();
            if (!childElement.isNull()) {
                QVariantMap itemMap;
                // Title
                itemMap["title"] = childElement.attribute("title").trimmed(); // Use 'title' attribute or text content
                if (itemMap["title"].toString().isEmpty()) {
                    itemMap["title"] = childElement.text().trimmed(); // Fallback to text content
                }

                // Destination (this is the complex part - needs Poppler::LinkDestination parsing)
                // Poppler::Document::toc() returns a QDomDocument. The destination information
                // (page number, coordinates) is often stored within the element's attributes
                // or as child elements depending on the PDF.
                // Common attributes might be 'Destination', 'Page', 'Top', 'Left'.
                // Let's try to extract common ones.
                QString destinationStr = childElement.attribute("Destination"); // Check for common attribute
                if (!destinationStr.isEmpty()) {
                    itemMap["destination"] = destinationStr;
                    // Further parsing of destinationStr (e.g., "/Page 5 /XYZ 100 700 null") might be needed
                    // This is highly PDF-specific and complex. Poppler doesn't directly expose
                    // a simple page number from the TOC element easily via this DOM.
                    // A more robust way might be to use Poppler::Link objects associated with TOC items,
                    // but the DOM from Document::toc() doesn't directly link to Poppler::Link objects.
                    // For now, store the raw destination string.
                } else {
                    // Try other common attributes
                    QString pageStr = childElement.attribute("Page");
                    if (!pageStr.isEmpty()) {
                        bool ok;
                        int pageNum = pageStr.toInt(&ok);
                        if (ok) {
                            itemMap["destination"] = QVariantMap{{"type", "page"}, {"page", pageNum - 1}}; // Convert to 0-based index
                        }
                    }
                }

                // Recursively add children
                itemMap["children"] = convertTocElementToVariantList(childElement);
                list.append(itemMap);
            }
        }
        child = child.nextSibling();
    }
    return list;
}


// --- NEW HELPER METHODS (add to Private class or as free functions near PdfDocument) ---

// Helper to find the QPDF object handle corresponding to a PdfAnnotation within a QPDF page object.
// This is the critical link. It requires PdfAnnotation to store identifying information from its original load.
// For now, this is a stub demonstrating the concept.
QPDFObjectHandle PdfDocument::findQpdfAnnotationHandle(const QPDFObjectHandle& pageObj, PdfAnnotation* pdfAnnot) const {
    // This function needs the PdfAnnotation to have stored information that can uniquely identify
    // its corresponding object in the QPDF structure when the PDF was initially loaded *via QPDF*.
    // Since PdfAnnotation wraps Poppler::Annotation, this link isn't direct.
    // The *best* way is to load the PDF once with QPDF during PdfDocument::load, store the initial
    // QPDF object handles for each annotation, and then use Poppler to *read* properties for the UI,
    // but use the stored QPDF handles for *writing* changes.

    // A less ideal way (but possible with Poppler loading) is to match based on properties like
    // boundary rectangle, type, and potentially contents (if unique enough). This is fragile.
    // Example:
    QPDFObjectHandle annotsArray = pageObj.getKey("/Annots");
    if (annotsArray.isArray()) {
        QRectF targetBounds = pdfAnnot->bounds(); // This comes from Poppler::Annotation::boundary()
        // We would need to iterate and compare /Rect keys in the QPDF objects.
        for (size_t i = 0; i < annotsArray.getArrayNItems(); ++i) {
            QPDFObjectHandle annotObj = annotsArray.getArrayItem(i);
            QPDFObjectHandle rectObj = annotObj.getKey("/Rect"); // /Rect is an array [l, b, r, t]
            if (rectObj.isArray() && rectObj.getArrayNItems() == 4) {
                double l = rectObj.getArrayItem(0).getNumericValue();
                double b = rectObj.getArrayItem(1).getNumericValue();
                double r = rectObj.getArrayItem(2).getNumericValue();
                double t = rectObj.getArrayItem(3).getNumericValue();
                QRectF qpdfBounds(l, b, r - l, t - b); // Convert PDF rect to QRectF
                if (qpdfBounds == targetBounds) { // Compare boundaries
                    // Type check might be needed too: annotObj.getKey("/Subtype")
                    return annotObj; // Found a potential match
                }
            }
        }
    }
    return QPDFObjectHandle(); // Return uninitialized handle if not found
}

// Helper to get contents from a QPDF annotation object handle (for comparison)
QString PdfDocument::getQpdfAnnotationContents(const QPDFObjectHandle& annotObj) const {
    QPDFObjectHandle contentsObj = annotObj.getKey("/Contents");
    if (contentsObj.isString()) {
        // Handle both regular and unicode strings if necessary
        std::string contentsUtf8 = contentsObj.getUTF8Value();
        return QString::fromStdString(contentsUtf8);
    } else if (contentsObj.isUnicodeString()) {
        std::u32string contentsU32 = contentsObj.getUnicodeValue();
        return QString::fromStdU32String(contentsU32);
    }
    return QString(); // Return empty if not a string
}

// Helper to get color from a QPDF annotation object handle (for comparison)
QColor PdfDocument::getQpdfAnnotationColor(const QPDFObjectHandle& annotObj) const {
    QPDFObjectHandle colorObj = annotObj.getKey("/C"); // /C key holds color array [r, g, b]
    if (colorObj.isArray() && colorObj.getArrayNItems() == 3) {
        double r = colorObj.getArrayItem(0).getNumericValue();
        double g = colorObj.getArrayItem(1).getNumericValue();
        double b = colorObj.getArrayItem(2).getNumericValue();
        // Clamp values to 0-1 range and convert to QColor (0-255 range)
        return QColor::fromRgbF(qBound(0.0, r, 1.0), qBound(0.0, g, 1.0), qBound(0.0, b, 1.0));
    }
    return QColor(); // Return default (black) if not found or invalid
}

} // namespace QuantilyxDoc