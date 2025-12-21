/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "PsDocument.h"
#include "PsPage.h" // Assuming this will be created
#include "../../core/Logger.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>
#include <QTextStream>
#include <QDebug>

namespace QuantilyxDoc {

class PsDocument::Private {
public:
    Private() : isLoaded(false), psLevelVal(0), isEpsFile(false), pageCountVal(0) {}
    ~Private() = default; // No specific resources to release here, Ghostscript integration would be elsewhere

    bool isLoaded;
    int psLevelVal;
    QRectF boundingBox;
    QSize resolution;
    bool isEpsFile;
    int pageCountVal;
    QString psCodeContent;
    QList<std::unique_ptr<PsPage>> pages; // Own the page objects

    // Helper to parse the beginning of the PS file for header info and DSC comments
    bool parseHeader(const QString& filePath) {
        QFile psFile(filePath);
        if (!psFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            LOG_ERROR("PsDocument: Failed to open PS file for reading: " << filePath);
            return false;
        }

        QTextStream stream(&psFile);
        QString line;
        int linesRead = 0;
        // DSC (Document Structuring Conventions) comments are usually in the first 1000 bytes or so
        while (!stream.atEnd() && linesRead < 100) { // Read a reasonable header section
            line = stream.readLine().trimmed();
            linesRead++;

            // Check for PS/EPS header
            if (line.startsWith("%!PS-Adobe-")) {
                // Standard PS header
            } else if (line.startsWith("%!")) {
                // Could be EPS header like %!PS-Adobe-3.0 EPSF-3.0
                if (line.contains("EPSF", Qt::CaseInsensitive)) {
                    isEpsFile = true;
                }
            }

            // Check DSC comments
            if (line.startsWith("%%")) {
                if (line.startsWith("%%Title:")) {
                    // Extract title if needed, store in Document base class
                    // setTitle(line.mid(8).trimmed());
                } else if (line.startsWith("%%Creator:")) {
                    // Extract creator if needed
                    // setAuthor(...);
                } else if (line.startsWith("%%BoundingBox:")) {
                    QRegularExpression bboxRegex(R"(%%BoundingBox:\s+(-?\d+)\s+(-?\d+)\s+(-?\d+)\s+(-?\d+))");
                    QRegularExpressionMatch match = bboxRegex.match(line);
                    if (match.hasMatch()) {
                        int llx = match.captured(1).toInt();
                        int lly = match.captured(2).toInt();
                        int urx = match.captured(3).toInt();
                        int ury = match.captured(4).toInt();
                        boundingBox = QRect(llx, lly, urx - llx, ury - lly);
                        LOG_DEBUG("PsDocument: Found BoundingBox: " << boundingBox);
                    }
                } else if (line.startsWith("%%HiResBoundingBox:")) {
                    // Similar parsing for HiResBoundingBox if needed
                    QRegularExpression bboxRegex(R"(%%HiResBoundingBox:\s+(-?\d+\.?\d*)\s+(-?\d+\.?\d*)\s+(-?\d+\.?\d*)\s+(-?\d+\.?\d*))");
                    QRegularExpressionMatch match = bboxRegex.match(line);
                    if (match.hasMatch()) {
                        // Parse as doubles if needed for higher precision
                        double llx = match.captured(1).toDouble();
                        double lly = match.captured(2).toDouble();
                        double urx = match.captured(3).toDouble();
                        double ury = match.captured(4).toDouble();
                        // Store or use HiRes bounding box
                        LOG_DEBUG("PsDocument: Found HiResBoundingBox: " << llx << "," << lly << " to " << urx << "," << ury);
                    }
                } else if (line.startsWith("%%DocumentData:")) {
                    // Check for Clean8Bit, Binary, etc.
                } else if (line.startsWith("%%Pages:")) {
                    // Get page count directly if available
                    QString pageCountStr = line.mid(9).trimmed(); // "%%Pages: <num>" or "%%Pages: (atend)"
                    if (pageCountStr != "(atend)") {
                         bool ok;
                         int pageCountFromDsc = pageCountStr.toInt(&ok);
                         if (ok) {
                             pageCountVal = pageCountFromDsc;
                             LOG_DEBUG("PsDocument: Found page count in DSC: " << pageCountVal);
                         }
                    }
                } else if (line.startsWith("%%EndComments")) {
                    // Stop parsing header after this comment
                    break;
                }
            }

            // Check PostScript Level (often in the header line itself or a specific comment)
            // The level is often part of the initial header line like "%!PS-Adobe-3.0"
            // Or look for %%Version: comment if available.
            // Let's assume Level is part of the main header for now.
            if (line.startsWith("%!PS-Adobe-")) {
                 QRegularExpression levelRegex(R"(PS-Adobe-\d+\.\d+\s+Level\s+(\d+))", QRegularExpression::CaseInsensitiveOption);
                 QRegularExpressionMatch levelMatch = levelRegex.match(line);
                 if (levelMatch.hasMatch()) {
                     psLevelVal = levelMatch.captured(1).toInt();
                 } else {
                     // If level isn't explicitly stated in the main header, it might be Level 2 by default for PS, or inferred.
                     // Common headers: %!PS-Adobe-3.0 (Level 2/3), %!PS-Adobe-2.0 (Level 2)
                     if (line.contains("3.0")) psLevelVal = 3;
                     else if (line.contains("2.0")) psLevelVal = 2;
                     else psLevelVal = 1; // Default assumption
                 }
                 LOG_DEBUG("PsDocument: Found PS Level: " << psLevelVal);
            }
        }

        // Read the rest of the file content if needed for full text extraction or other analysis
        // psFile.seek(0); // Reset to beginning if full read is needed later
        // psCodeContent = psFile.readAll(); // Be careful with large files

        return true;
    }

    // Helper to count pages by parsing the PS code (e.g., counting 'showpage' or using DSC)
    bool countPages(const QString& filePath) {
        if (pageCountVal > 0) return true; // Already determined from DSC

        QFile psFile(filePath);
        if (!psFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            LOG_ERROR("PsDocument: Failed to open PS file for page counting: " << filePath);
            return false;
        }

        // A simple heuristic: count occurrences of 'showpage' command.
        // This might not work perfectly for all PS files, especially complex ones or EPS.
        // A more robust method involves Ghostscript to actually process the file and count pages.
        // For now, let's use the simple text-based approach as a fallback.
        // If DSC %%Pages was (atend) or not present, this becomes the primary method.
        QTextStream stream(&psFile);
        QString content = stream.readAll();
        // Count 'showpage' occurrences. Need to be careful about comments and strings.
        // A regex might be too simple. Ghostscript is the reliable way.
        int showpageCount = 0;
        int pos = 0;
        QRegularExpression showpageRegex(R"\bshowpage\b"); // Word boundary to avoid 'nshowpage'
        QRegularExpressionMatchIterator iter = showpageRegex.globalMatch(content);
        while (iter.hasNext()) {
            QRegularExpressionMatch match = iter.next();
            // Check if match is inside a comment or string (simplified check)
            int matchPos = match.capturedStart();
            // Find the nearest % before the match to see if it's a comment line
            int commentStart = content.lastIndexOf('%', matchPos);
            int lineStart = content.lastIndexOf('\n', matchPos);
            if (commentStart > lineStart) {
                // Match is on a comment line, skip
                continue;
            }
            // Check for strings is harder without a full PS parser.
            // For now, assume most 'showpage' instances are valid.
            showpageCount++;
        }

        pageCountVal = showpageCount;
        LOG_DEBUG("PsDocument: Estimated page count by counting 'showpage': " << pageCountVal);
        // This count might be inaccurate. Relying on Ghostscript for accurate count is recommended.
        return pageCountVal > 0;
    }
};

PsDocument::PsDocument(QObject* parent)
    : Document(parent)
    , d(new Private())
{
    LOG_INFO("PsDocument created. Note: PS rendering typically requires Ghostscript.");
}

PsDocument::~PsDocument()
{
    LOG_INFO("PsDocument destroyed.");
}

bool PsDocument::load(const QString& filePath, const QString& password)
{
    Q_UNUSED(password); // PS files don't typically use file-level passwords like PDFs

    // Reset state
    d->isLoaded = false;
    d->pages.clear();

    // Parse header and DSC comments
    if (!d->parseHeader(filePath)) {
        setLastError(tr("Failed to parse PostScript header/DSC comments."));
        LOG_ERROR(lastError());
        return false;
    }

    // Count pages
    if (!d->countPages(filePath)) {
        setLastError(tr("Failed to determine page count for PostScript document."));
        LOG_ERROR(lastError());
        return false;
    }

    // Set file path and update file size
    setFilePath(filePath);

    // Create PsPage objects based on estimated page count
    // Actual rendering will likely require Ghostscript calls per page
    createPages();

    d->isLoaded = true;
    setState(Loaded);
    emit psLoaded(); // Emit specific signal for PS loading
    LOG_INFO("Successfully loaded PostScript document: " << filePath << " (Pages: " << pageCount() << ", Level: " << d->psLevelVal << ", EPS: " << d->isEpsFile << ")");
    return true;
}

bool PsDocument::save(const QString& filePath)
{
    // Saving PS requires generating valid PostScript code, which is complex.
    LOG_WARN("PsDocument::save: Saving modified PostScript documents is not yet implemented.");
    Q_UNUSED(filePath);
    setLastError(tr("Saving modified PostScript documents is not yet supported."));
    return false;
}

Document::DocumentType PsDocument::type() const
{
    return d->isEpsFile ? DocumentType::EPS : DocumentType::PS;
}

int PsDocument::pageCount() const
{
    return d->pageCountVal;
}

Page* PsDocument::page(int index) const
{
    if (index >= 0 && index < d->pages.size()) {
         return d->pages[index].get(); // Placeholder - requires PsPage implementation
        LOG_DEBUG("PsDocument::page: Requested page " << index << ", but PsPage not yet implemented.");
        return nullptr; // For now, return null until PsPage is ready
    }
    return nullptr;
}

Page* PsDocument::page(int index) const
{
    if (index >= 0 && index < d->pages.size()) {
        return d->pages[index].get(); // Now returns a valid PsPage*
    }
    return nullptr;
}

bool PsDocument::isLocked() const
{
    // PS files can have security settings embedded in the PostScript code itself,
    // checked during interpretation. This is not typically a file-system level lock.
    // Determining this without interpreting the code is difficult.
    // For now, assume unlocked if loaded.
    return false;
}

bool PsDocument::isEncrypted() const
{
    // Similar to isLocked. PS doesn't have standard encryption like PDF.
    return false;
}

QString PsDocument::formatVersion() const
{
    return QString("PostScript Level %1%2").arg(d->psLevelVal).arg(d->isEpsFile ? " (EPS)" : "");
}

bool PsDocument::supportsFeature(const QString& feature) const
{
    static const QSet<QString> supportedFeatures = {
        "VectorGraphics", "HighQualityPrinting", "Text", "ComplexLayout" // PS specific features
        // "Rendering" // Requires Ghostscript
    };
    return supportedFeatures.contains(feature);
}

// --- PS-Specific Getters ---
int PsDocument::psLevel() const
{
    return d->psLevelVal;
}

QRectF PsDocument::documentBoundingBox() const
{
    return d->boundingBox;
}

QSize PsDocument::intendedResolution() const
{
    return d->resolution; // Placeholder, might be derived from HiResBoundingBox or other DSC hints
}

bool PsDocument::isEps() const
{
    return d->isEpsFile;
}

QString PsDocument::postScriptCode() const
{
    // This would return the full PS code, potentially read during parseHeader if needed.
    // Reading the full code for large files might be memory intensive.
    LOG_WARN("PsDocument::postScriptCode: Returning placeholder. Full code access requires careful implementation.");
    return d->psCodeContent; // Placeholder
}

bool PsDocument::pageHasEpsStructures(int pageIndex) const
{
    Q_UNUSED(pageIndex);
    // EPS structures like preview bitmaps are often embedded in the PS code itself.
    // Detecting them requires parsing the PS code for specific EPS conventions (e.g., %%BeginPreview).
    // This is complex without a full PS interpreter.
    LOG_WARN("PsDocument::pageHasEpsStructures: Requires parsing PS code for EPS conventions.");
    return false; // Placeholder
}

bool PsDocument::exportAsImageSequence(const QString& outputDirectory, const QString& format, int resolution) const
{
    // This would typically involve calling Ghostscript (gs) command line tool for each page.
    // Example gs command: gs -dNOPAUSE -dBATCH -sDEVICE=tiff24nc -r300 -sOutputFile=output_page_%d.tiff input.ps
    LOG_WARN("PsDocument::exportAsImageSequence: Requires Ghostscript integration.");
    Q_UNUSED(outputDirectory);
    Q_UNUSED(format);
    Q_UNUSED(resolution);
    return false; // Placeholder
}

// --- Helpers ---
void PsDocument::createPages()
{
    d->pages.clear();
    d->pages.reserve(d->pageCountVal);
    for (int i = 0; i < d->pageCountVal; ++i) {
        // Create the page object. This requires PsPage to be implemented.
        // pages.append(std::make_unique<PsPage>(this, i)); // Placeholder
        LOG_DEBUG("PsDocument: Planned page " << i);
    }
    LOG_INFO("PsDocument: Created " << d->pages.size() << " page objects.");
}

} // namespace QuantilyxDoc