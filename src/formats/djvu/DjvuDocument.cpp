/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "DjvuDocument.h"
#include "DjvuPage.h" // Assuming this will be created
#include "../../core/Logger.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTemporaryDir>
#include <QTextStream>
#include <QDebug>
#include <ddjvuapi.h> // Include DjVuLibre header

namespace QuantilyxDoc {

class DjvuDocument::Private {
public:
    Private() : context(nullptr), document(nullptr), pageCountVal(0), isLoaded(false) {}
    ~Private() {
        if (document) {
            ddjvu_document_release(document);
        }
        if (context) {
            ddjvu_context_release(context);
        }
    }

    ddjvu_context_t* context;
    ddjvu_document_t* document;
    int pageCountVal;
    bool isLoaded;
    QString djvuVersionStr;
    QRectF boundingBox; // Calculated from page info
    QList<std::unique_ptr<DjvuPage>> pages; // Own the page objects
    QStringList embeddedFileNames;
    bool hasSharedAnnots = false;

    // Helper to handle DjVuLibre messages (errors, warnings, progress)
    void handleMessages() {
        const ddjvu_message_t *msg;
        while ((msg = ddjvu_message_wait(context))) {
            ddjvu_message_tag_t tag = msg->m_any.tag;
            switch (tag) {
                case DDJVU_ERROR:
                    LOG_ERROR("DjVuLibre Error: " << msg->m_error.message);
                    // Potentially set a lastError string here for the Document class
                    break;
                case DDJVU_INFO:
                    LOG_INFO("DjVuLibre Info: " << msg->m_info.message);
                    break;
                case DDJVU_PROGRESS:
                    LOG_DEBUG("DjVuLibre Progress: " << msg->m_progress.percent << "%");
                    // Could emit a progress signal here if needed
                    break;
                case DDJVU_NEWSTREAM:
                    LOG_DEBUG("DjVuLibre New Stream");
                    break;
                case DDJVU_JB2DECODE:
                case DDJVU_PAGELAYOUT:
                case DDJVU_PAGEDECODE:
                case DDJVU_RELAYOUT:
                case DDJVU_RELOAD:
                case DDJVU_CHUNK:
                    // These are internal progress messages, usually not logged unless debugging
                    break;
            }
            ddjvu_message_pop(context);
        }
    }

    // Helper to get page info (size, rotation) without fully decoding the page
    bool getPageInfo(int pageIndex, ddjvu_pageinfo_t* info) {
        if (!document || pageIndex < 0 || pageIndex >= pageCountVal) return false;

        ddjvu_status_t status = ddjvu_document_get_pageinfo(document, pageIndex, info);
        // Wait for the info request to complete
        while (status < DDJVU_JOB_OK) {
            handleMessages(); // Process messages to allow job to progress
            status = ddjvu_document_get_pageinfo_status(document, pageIndex, 0);
        }
        return status == DDJVU_JOB_OK;
    }
};

DjvuDocument::DjvuDocument(QObject* parent)
    : Document(parent)
    , d(new Private())
{
    LOG_INFO("DjvuDocument created.");
}

DjvuDocument::~DjvuDocument()
{
    LOG_INFO("DjvuDocument destroyed.");
}

bool DjvuDocument::load(const QString& filePath, const QString& password)
{
    // Close any previously loaded document
    if (d->document) {
        ddjvu_document_release(d->document);
        d->document = nullptr;
    }
    if (d->context) {
        ddjvu_context_release(d->context);
        d->context = nullptr;
    }
    d->isLoaded = false;
    d->pages.clear();

    // Initialize DjVuLibre context
    d->context = ddjvu_context_create("QuantilyxDoc");
    if (!d->context) {
        setLastError(tr("Failed to initialize DjVuLibre context."));
        LOG_ERROR(lastError());
        return false;
    }

    // Set password if provided (DjVu can be encrypted)
    if (!password.isEmpty()) {
        ddjvu_context_set_password(d->context, password.toUtf8().constData());
    }

    // Load the DjVu document
    d->document = ddjvu_document_create_by_filename_utf8(d->context, filePath.toUtf8().constData(), 0 /* no cache */);
    if (!d->document) {
        // Check for errors/messages immediately after creation attempt
        d->handleMessages();
        setLastError(tr("Failed to load DjVu document. It may be corrupted or password-protected (and password was incorrect)."));
        LOG_ERROR(lastError());
        return false;
    }

    // Wait for document header to be loaded
    ddjvu_status_t status = ddjvu_document_decoding_status(d->document);
    while (status < DDJVU_JOB_OK) {
        d->handleMessages();
        status = ddjvu_document_decoding_status(d->document);
    }

    if (status != DDJVU_JOB_OK) {
        setLastError(tr("Error decoding DjVu document header."));
        LOG_ERROR(lastError());
        return false;
    }

    // Set file path and update file size
    setFilePath(filePath);

    // Query document info (page count, global metadata)
    if (!queryDocumentInfo()) {
        setLastError(tr("Failed to query DjVu document information."));
        LOG_ERROR(lastError());
        return false;
    }

    // Create DjvuPage objects
    createPages();

    d->isLoaded = true;
    setState(Loaded);
    emit djvuLoaded(); // Emit specific signal for DjVu loading
    LOG_INFO("Successfully loaded DjVu document: " << filePath << " (Pages: " << pageCount() << ")");
    return true;
}

bool DjvuDocument::save(const QString& filePath)
{
    // Saving DjVu documents requires writing DjVu files, which is complex and
    // typically done with tools like djvumake, c44, cjb2, etc., or using
    // the full IW44EncodeContext and BZZ functions from DjVuLibre, which is non-trivial.
    LOG_WARN("DjvuDocument::save: Saving modified DjVu documents is not yet implemented.");
    Q_UNUSED(filePath);
    setLastError(tr("Saving modified DjVu documents is not yet supported."));
    return false;
}

Document::DocumentType DjvuDocument::type() const
{
    return DocumentType::DJVU;
}

int DjvuDocument::pageCount() const
{
    return d->pageCountVal;
}

Page* DjvuDocument::page(int index) const
{
    if (index >= 0 && index < d->pages.size()) {
        // Return raw pointer managed by unique_ptr.
        // return d->pages[index].get(); // Placeholder - requires DjvuPage implementation
        LOG_DEBUG("DjvuDocument::page: Requested page " << index << ", but DjvuPage not yet implemented.");
        return nullptr; // For now, return null until DjvuPage is ready
    }
    return nullptr;
}

bool DjvuDocument::isLocked() const
{
    // DjVuLibre might have a function to check if the document is locked due to password.
    // ddjvu_document_get_type() returns DDJVU_DOCTYPE_* enums, but doesn't directly indicate lock status from password failure.
    // The load function should fail or require password re-entry if locked and wrong password was given initially.
    // For simplicity here, if the document loaded successfully without needing a password now, assume it's not locked.
    // A more accurate check might involve attempting an operation that requires full access or checking internal state.
    // This is often determined during the `load` call itself.
    // Let's assume if `d->document` is valid and loaded, it's not locked *anymore*.
    // The initial lock is handled during `load`.
    return false; // If we got here, load() succeeded, so it's not currently locked.
}

bool DjvuDocument::isEncrypted() const
{
    // Similar to isLocked. If load() succeeded with a password or without needing one, it's decrypted in memory for reading.
    // The file on disk might have been encrypted.
    return false; // In-memory state is decrypted if load() worked.
}

QString DjvuDocument::formatVersion() const
{
    return d->djvuVersionStr;
}

bool DjvuDocument::supportsFeature(const QString& feature) const
{
    static const QSet<QString> supportedFeatures = {
        "TextSelection", "TextExtraction", "Images", "MultiLayer", "HighCompression", "BackgroundSeparation" // DjVu specific features
        // "AnnotationEditing" // Would require saving logic
    };
    return supportedFeatures.contains(feature);
}

// --- DjVu-Specific Getters ---
QString DjvuDocument::djvuVersion() const
{
    return d->djvuVersionStr;
}

QRectF DjvuDocument::documentBoundingBox() const
{
    return d->boundingBox;
}

bool DjvuDocument::hasSharedAnnotations() const
{
    return d->hasSharedAnnots;
}

QStringList DjvuDocument::embeddedFiles() const
{
    return d->embeddedFileNames;
}

bool DjvuDocument::pageHasText(int pageIndex) const
{
    // DjVu pages can have 'text' layers (hidden text for OCR/selection).
    // This requires checking the page's chunks or decoding status for text components.
    // ddjvu_page_get_{text,anno} might be used, but they require the page object to be created/decoded first.
    // A simpler check might be possible by inspecting the page info or document structure during load.
    // For now, assume true if the page index is valid, as most DjVu files intended for document viewing have text.
    // A more accurate implementation would query the page object once created.
    return (pageIndex >= 0 && pageIndex < pageCount());
}

bool DjvuDocument::pageHasMask(int pageIndex) const
{
    // Similar to pageHasText, checking for the 'FG44' (foreground mask) chunk.
    // Requires page decoding or structure inspection.
    // Assume true for valid pages for now.
    return (pageIndex >= 0 && pageIndex < pageCount());
}

double DjvuDocument::averageCompressionRatio() const
{
    // Calculating compression ratio requires knowing the original size vs. DjVu size.
    // This is not directly provided by DjVuLibre API. It would need to be estimated
    // based on page dimensions, number of layers, and DjVu file size.
    LOG_WARN("DjvuDocument::averageCompressionRatio: Not directly available from DjVuLibre API.");
    return 0.0; // Placeholder
}

bool DjvuDocument::exportPageAsImage(int pageIndex, const QString& outputPath, const QString& format) const
{
    // This would involve creating a ddjvu_page_t, rendering it using ddjvu_page_render,
    // and saving the resulting ddjvu_rect_t to an image file using external libraries
    // (like Qt's image I/O or specific TIFF/PNG libraries).
    // This is a complex operation involving raw buffer manipulation.
    LOG_WARN("DjvuDocument::exportPageAsImage: Not yet implemented.");
    Q_UNUSED(pageIndex);
    Q_UNUSED(outputPath);
    Q_UNUSED(format);
    return false; // Placeholder
}

// --- Helpers ---
bool DjvuDocument::queryDocumentInfo()
{
    if (!d->document) return false;

    // Get page count
    d->pageCountVal = ddjvu_document_get_pagenum(d->document);
    LOG_DEBUG("DjVu Document Page Count: " << d->pageCountVal);

    // Get document version/type? Not directly available as a simple string like PDF.
    // ddjvu_document_get_type() gives enums. We can synthesize a version string.
    ddjvu_document_type_t docType = ddjvu_document_get_type(d->document);
    switch (docType) {
        case DDJVU_DOCTYPE_UNKNOWN: d->djvuVersionStr = "DjVu (Unknown Type)"; break;
        case DDJVU_DOCTYPE_SINGLEPAGE: d->djvuVersionStr = "DjVu Single Page"; break;
        case DDJVU_DOCTYPE_BUNDLED: d->djvuVersionStr = "DjVu Bundled (DjV)"; break;
        case DDJVU_DOCTYPE_INDIRECT: d->djvuVersionStr = "DjVu Indirect (DjVu)"; break;
        case DDJVU_DOCTYPE_OLD_BUNDLED: d->djvuVersionStr = "DjVu Old Bundled"; break;
        case DDJVU_DOCTYPE_OLD_INDIRECT: d->djvuVersionStr = "DjVu Old Indirect"; break;
        default: d->djvuVersionStr = "DjVu (Type " + QString::number(docType) + ")"; break;
    }

    // Calculate overall bounding box by iterating pages
    // This requires getting size info for each page.
    QRectF overallBounds;
    for (int i = 0; i < d->pageCountVal; ++i) {
        ddjvu_pageinfo_t info;
        if (d->getPageInfo(i, &info)) {
            QRectF pageBounds(0, 0, info.width, info.height);
            overallBounds = overallBounds.united(pageBounds);
        }
    }
    d->boundingBox = overallBounds;

    // Check for shared annotations
    // This involves checking for a 'ANTa' chunk at the document level.
    // ddjvu_document_get_filenum and iterating chunks might be needed.
    // For now, assume false or implement chunk inspection later.
    // d->hasSharedAnnots = checkForSharedAnnotChunk(); // Hypothetical helper

    LOG_DEBUG("DjVu Document Info - Type: " << d->djvuVersionStr << ", BBox: " << d->boundingBox);
    return true;
}

void DjvuDocument::createPages()
{
    d->pages.clear();
    d->pages.reserve(d->pageCountVal);
    for (int i = 0; i < d->pageCountVal; ++i) {
        // Create the page object. This requires DjvuPage to be implemented.
        // pages.append(std::make_unique<DjvuPage>(this, i)); // Placeholder
        LOG_DEBUG("DjvuDocument: Planned page " << i);
    }
    LOG_INFO("DjvuDocument: Created " << d->pages.size() << " page objects.");
}

} // namespace QuantilyxDoc