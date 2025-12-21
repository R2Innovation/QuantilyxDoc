/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "EpubDocument.h"
#include "EpubPage.h" // Assuming this will be created
#include "../../core/Logger.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTemporaryDir>
#include <QDomDocument>
#include <QDomElement>
#include <QDomNode>
#include <QUrl>
#include <QRegularExpression>
#include <QUuid> // For generating temporary directory names if needed
#include <QDebug>
#include <zip.h> // Include libzip header

namespace QuantilyxDoc {

// Forward declaration of EpubPage creation helper (to be implemented later)
// std::unique_ptr<EpubPage> createEpubPage(EpubDocument* doc, int index, const QString& htmlPath);

class EpubDocument::Private {
public:
    Private() : zipArchive(nullptr), isLoaded(false) {}
    ~Private() {
        if (zipArchive) {
            zip_close(zipArchive);
        }
    }

    zip_t* zipArchive;
    QString containerPath; // Path to META-INF/container.xml inside the archive
    QString packagePath;   // Path to the .opf file inside the archive
    QString navigationPath; // Path to nav.xhtml or toc.ncx inside the archive
    QString uid;           // Unique identifier from OPF
    QString formatVersion; // e.g., EPUB 2.0, EPUB 3.0
    bool isLoaded;
    QMap<QString, QString> manifest; // ID -> HREF
    QStringList spine;               // List of manifest IDs in reading order
    QVariantList toc;              // Parsed TOC structure
    QList<std::unique_ptr<EpubPage>> pages; // Own the page objects
    QStringList embeddedFontsList;
    QStringList imagePathsList;
    QList<QUrl> hyperlinksList;

    // Helper to read a file from the ZIP archive
    QByteArray readFileFromZip(const QString& filePath) const {
        if (!zipArchive) return QByteArray();

        QString pathInZip = filePath;
        if (pathInZip.startsWith("/")) pathInZip.remove(0, 1); // Remove leading slash if present

        zip_stat_t stat;
        int result = zip_stat(zipArchive, pathInZip.toUtf8().constData(), 0, &stat);
        if (result < 0) {
            LOG_ERROR("EpubDocument: Failed to stat file in archive: " << filePath);
            return QByteArray();
        }

        zip_file_t* file = zip_fopen(zipArchive, pathInZip.toUtf8().constData(), 0);
        if (!file) {
            LOG_ERROR("EpubDocument: Failed to open file in archive: " << filePath);
            return QByteArray();
        }

        QByteArray data(stat.size, 0);
        zip_int64_t bytesRead = zip_fread(file, data.data(), stat.size);
        zip_fclose(file);

        if (bytesRead != static_cast<zip_int64_t>(stat.size)) {
            LOG_ERROR("EpubDocument: Failed to read full file content: " << filePath);
            return QByteArray();
        }

        return data;
    }

    // Helper to parse container.xml to find the package.opf path
    bool parseContainer() {
        QByteArray containerData = readFileFromZip("META-INF/container.xml");
        if (containerData.isEmpty()) {
            LOG_ERROR("EpubDocument: Could not read META-INF/container.xml");
            return false;
        }

        QDomDocument containerDoc;
        QString errorMsg;
        int errorLine, errorCol;
        if (!containerDoc.setContent(containerData, false, &errorMsg, &errorLine, &errorCol)) {
            LOG_ERROR("EpubDocument: Failed to parse container.xml: " << errorMsg << " at " << errorLine << ":" << errorCol);
            return false;
        }

        QDomElement rootElement = containerDoc.documentElement();
        if (rootElement.tagName() != "container") {
            LOG_ERROR("EpubDocument: Invalid container.xml root element.");
            return false;
        }

        QDomNodeList rootfiles = rootElement.elementsByTagName("rootfile");
        if (rootfiles.isEmpty()) {
            LOG_ERROR("EpubDocument: No <rootfile> found in container.xml");
            return false;
        }

        QDomElement rootfile = rootfiles.at(0).toElement();
        if (rootfile.attribute("media-type") != "application/oebps-package+xml") {
             LOG_WARN("EpubDocument: First <rootfile> does not have expected media-type, searching for correct one.");
             bool found = false;
             for (int i = 0; i < rootfiles.size(); ++i) {
                 QDomElement rf = rootfiles.at(i).toElement();
                 if (rf.attribute("media-type") == "application/oebps-package+xml") {
                     packagePath = rf.attribute("full-path");
                     found = true;
                     LOG_DEBUG("EpubDocument: Found package.opf path: " << packagePath);
                     break;
                 }
             }
             if (!found) {
                 LOG_ERROR("EpubDocument: No <rootfile> with media-type 'application/oebps-package+xml' found.");
                 return false;
             }
        } else {
            packagePath = rootfile.attribute("full-path");
            LOG_DEBUG("EpubDocument: Found package.opf path: " << packagePath);
        }

        return true;
    }

    // Helper to parse the OPF file to get manifest, spine, metadata
    bool parseOpf() {
        QByteArray opfData = readFileFromZip(packagePath);
        if (opfData.isEmpty()) {
            LOG_ERROR("EpubDocument: Could not read package file: " << packagePath);
            return false;
        }

        QDomDocument opfDoc;
        QString errorMsg;
        int errorLine, errorCol;
        if (!opfDoc.setContent(opfData, false, &errorMsg, &errorLine, &errorCol)) {
            LOG_ERROR("EpubDocument: Failed to parse " << packagePath << ": " << errorMsg << " at " << errorLine << ":" << errorCol);
            return false;
        }

        QDomElement packageElement = opfDoc.documentElement();
        if (packageElement.tagName() != "package") {
            LOG_ERROR("EpubDocument: Invalid OPF root element.");
            return false;
        }

        // Get version
        formatVersion = packageElement.attribute("version", "Unknown");
        LOG_DEBUG("EpubDocument: EPUB version: " << formatVersion);

        // Get unique identifier
        QString uidId = packageElement.attribute("unique-identifier", "");
        if (!uidId.isEmpty()) {
            QDomNodeList metadataList = packageElement.elementsByTagName("metadata");
            if (!metadataList.isEmpty()) {
                QDomElement metadataElement = metadataList.at(0).toElement();
                QDomNodeList identifierList = metadataElement.elementsByTagName("identifier");
                for (int i = 0; i < identifierList.size(); ++i) {
                    QDomElement identifier = identifierList.at(i).toElement();
                    if (identifier.attribute("id") == uidId) {
                        uid = identifier.text();
                        LOG_DEBUG("EpubDocument: EPUB UID: " << uid);
                        break;
                    }
                }
            }
        }

        // Parse manifest
        QDomNodeList manifestList = packageElement.elementsByTagName("manifest");
        if (!manifestList.isEmpty()) {
            QDomElement manifestElement = manifestList.at(0).toElement();
            QDomNodeList itemNodes = manifestElement.elementsByTagName("item");
            for (int i = 0; i < itemNodes.size(); ++i) {
                QDomElement item = itemNodes.at(i).toElement();
                QString id = item.attribute("id");
                QString href = item.attribute("href");
                QString mediaType = item.attribute("media-type");
                manifest.insert(id, href);
                LOG_DEBUG("EpubDocument: Manifest item - ID: " << id << ", HREF: " << href << ", Type: " << mediaType);
                // Identify embedded fonts and images
                if (mediaType.startsWith("font/")) {
                    embeddedFontsList.append(href);
                } else if (mediaType.startsWith("image/")) {
                    imagePathsList.append(href);
                }
            }
        }

        // Parse spine
        QDomNodeList spineList = packageElement.elementsByTagName("spine");
        if (!spineList.isEmpty()) {
            QDomElement spineElement = spineList.at(0).toElement();
            QDomNodeList itemRefNodes = spineElement.elementsByTagName("itemref");
            for (int i = 0; i < itemRefNodes.size(); ++i) {
                QDomElement itemRef = itemRefNodes.at(i).toElement();
                QString idRef = itemRef.attribute("idref");
                spine.append(idRef);
                LOG_DEBUG("EpubDocument: Spine item - IDREF: " << idRef);
            }
        }

        // Look for navigation file in spine or manifest (EPUB 3 uses nav.xhtml, EPUB 2 uses toc.ncx)
        // Often, the nav.xhtml is listed in the manifest with a specific property or in the spine with a linear="no" attribute.
        // For simplicity, let's first check if a manifest item has a property like "nav".
        for (auto it = manifest.constBegin(); it != manifest.constEnd(); ++it) {
            QDomNodeList itemNodes = opfDoc.elementsByTagName("item");
            for (int i = 0; i < itemNodes.size(); ++i) {
                QDomElement item = itemNodes.at(i).toElement();
                if (item.attribute("id") == it.key() && item.attribute("properties").contains("nav")) {
                    navigationPath = it.value();
                    LOG_DEBUG("EpubDocument: Found navigation file (nav.xhtml) in manifest: " << navigationPath);
                    break;
                }
            }
            if (!navigationPath.isEmpty()) break;
        }
        // If not found in manifest properties, check spine for linear="no"
        if (navigationPath.isEmpty()) {
            for (int i = 0; i < spine.size(); ++i) {
                QString idRef = spine[i];
                // Find the corresponding item in manifest to get href
                QString href = manifest.value(idRef);
                if (!href.isEmpty() && (href.endsWith(".xhtml") || href.endsWith(".html")) ) {
                    // Check if itemref in spine has linear="no" (often used for nav)
                    QDomNodeList itemRefNodes = opfDoc.elementsByTagName("itemref");
                    for (int j = 0; j < itemRefNodes.size(); ++j) {
                        QDomElement itemRef = itemRefNodes.at(j).toElement();
                        if (itemRef.attribute("idref") == idRef && itemRef.attribute("linear") == "no") {
                            navigationPath = href;
                            LOG_DEBUG("EpubDocument: Found navigation file (non-linear spine item) in spine: " << navigationPath);
                            break;
                        }
                    }
                }
                if (!navigationPath.isEmpty()) break;
            }
        }
        // If still not found, look for toc.ncx in manifest (EPUB 2)
        if (navigationPath.isEmpty()) {
            for (auto it = manifest.constBegin(); it != manifest.constEnd(); ++it) {
                QString mediaType = getMediaTypeForId(it.key(), opfDoc); // Helper to find media-type for manifest ID
                if (mediaType == "application/x-dtbncx+xml") {
                    navigationPath = it.value();
                    LOG_DEBUG("EpubDocument: Found navigation file (toc.ncx) in manifest: " << navigationPath);
                    break;
                }
            }
        }

        return true;
    }

    // Helper to get media-type for a manifest ID (used in parseOpf)
    QString getMediaTypeForId(const QString& id, const QDomDocument& opfDoc) const {
        QDomNodeList itemNodes = opfDoc.elementsByTagName("item");
        for (int i = 0; i < itemNodes.size(); ++i) {
            QDomElement item = itemNodes.at(i).toElement();
            if (item.attribute("id") == id) {
                return item.attribute("media-type");
            }
        }
        return QString();
    }

    // Helper to parse the navigation file (nav.xhtml or toc.ncx)
    bool parseNavigation() {
        if (navigationPath.isEmpty()) {
            LOG_WARN("EpubDocument: No navigation file path found, skipping TOC parsing.");
            return true; // Not an error, just no TOC
        }

        QByteArray navData = readFileFromZip(navigationPath);
        if (navData.isEmpty()) {
            LOG_WARN("EpubDocument: Could not read navigation file: " << navigationPath);
            return true; // Not an error, just no TOC
        }

        QDomDocument navDoc;
        QString errorMsg;
        int errorLine, errorCol;
        if (!navDoc.setContent(navData, false, &errorMsg, &errorLine, &errorCol)) {
            LOG_ERROR("EpubDocument: Failed to parse navigation file " << navigationPath << ": " << errorMsg << " at " << errorLine << ":" << errorCol);
            return false;
        }

        QDomElement rootElement = navDoc.documentElement();

        if (rootElement.tagName() == "nav" && rootElement.namespaceURI() == "http://www.idpf.org/2007/ops") {
            // EPUB 3 nav.xhtml
            LOG_DEBUG("EpubDocument: Parsing EPUB 3 nav.xhtml");
            QDomNodeList navPoints = navDoc.elementsByTagName("ol"); // TOC is often inside <nav epub:type="toc"><ol>...</ol></nav>
            if (!navPoints.isEmpty()) {
                QDomElement olElement = navPoints.at(0).toElement();
                toc = parseNavElement(olElement);
            }
        } else if (rootElement.tagName() == "ncx") {
            // EPUB 2 toc.ncx
            LOG_DEBUG("EpubDocument: Parsing EPUB 2 toc.ncx");
            QDomNodeList navPoints = navDoc.elementsByTagName("navPoint");
            if (!navPoints.isEmpty()) {
                toc = parseNcxNavPoints(navPoints);
            }
        } else {
            LOG_WARN("EpubDocument: Unknown navigation file format or root element: " << rootElement.tagName());
            return false;
        }

        LOG_DEBUG("EpubDocument: Parsed TOC with " << toc.size() << " top-level items.");
        return true;
    }

    // Helper to recursively parse <ol> elements in nav.xhtml
    QVariantList parseNavElement(const QDomElement& olElement) const {
        QVariantList list;
        QDomNode child = olElement.firstChild();
        while (!child.isNull()) {
            if (child.isElement()) {
                QDomElement childElement = child.toElement();
                if (childElement.tagName() == "li") {
                    QDomElement linkElement = childElement.firstChildElement("a"); // Should be <a>
                    if (!linkElement.isNull()) {
                        QVariantMap itemMap;
                        itemMap["title"] = linkElement.text().trimmed();
                        itemMap["destination"] = linkElement.attribute("href"); // Store the link destination (relative path + fragment)
                        // Check for nested <ol> for children
                        QDomElement nestedOl = childElement.firstChildElement("ol");
                        if (!nestedOl.isNull()) {
                            itemMap["children"] = parseNavElement(nestedOl);
                        } else {
                            itemMap["children"] = QVariantList(); // Ensure children key exists
                        }
                        list.append(itemMap);
                    } else {
                        // Handle <span> or other non-link elements if present
                        QDomElement spanElement = childElement.firstChildElement("span");
                        if (!spanElement.isNull()) {
                            QVariantMap itemMap;
                            itemMap["title"] = spanElement.text().trimmed();
                            // No destination for non-link elements
                            QDomElement nestedOl = childElement.firstChildElement("ol");
                            if (!nestedOl.isNull()) {
                                itemMap["children"] = parseNavElement(nestedOl);
                            } else {
                                itemMap["children"] = QVariantList();
                            }
                            list.append(itemMap);
                        }
                    }
                }
            }
            child = child.nextSibling();
        }
        return list;
    }

    // Helper to recursively parse <navPoint> elements in toc.ncx
    QVariantList parseNcxNavPoints(const QDomNodeList& navPoints) const {
        QVariantList list;
        for (int i = 0; i < navPoints.size(); ++i) {
            QDomElement navPoint = navPoints.at(i).toElement();
            if (navPoint.tagName() == "navPoint") {
                QDomElement navLabel = navPoint.firstChildElement("navLabel");
                QDomElement content = navPoint.firstChildElement("content");
                if (!navLabel.isNull() && !content.isNull()) {
                    QVariantMap itemMap;
                    // Get text from <text> inside <navLabel>
                    QDomElement navLabelText = navLabel.firstChildElement("text");
                    if (!navLabelText.isNull()) {
                        itemMap["title"] = navLabelText.text().trimmed();
                    } else {
                        itemMap["title"] = navLabel.text().trimmed(); // Fallback
                    }
                    itemMap["destination"] = content.attribute("src"); // Store the source (relative path + fragment)
                    // Recursively parse children
                    QDomNodeList childNavPoints = navPoint.elementsByTagName("navPoint");
                    if (!childNavPoints.isEmpty()) {
                        itemMap["children"] = parseNcxNavPoints(childNavPoints);
                    } else {
                        itemMap["children"] = QVariantList(); // Ensure children key exists
                    }
                    list.append(itemMap);
                }
            }
        }
        return list;
    }

    // Helper to create EpubPage objects based on the spine order
    void createPages(EpubDocument* doc) {
        pages.clear();
        pages.reserve(spine.size());
        for (int i = 0; i < spine.size(); ++i) {
            QString manifestId = spine[i];
            QString htmlPath = manifest.value(manifestId);
            if (!htmlPath.isEmpty()) {
                // Create the page object. This requires EpubPage to be implemented.
                // For now, let's assume a factory function exists or EpubPage can be constructed directly if it has access to the doc/zip.
                // A better design might be for EpubPage to hold a reference to EpubDocument and fetch its content on demand.
                // pages.append(std::make_unique<EpubPage>(doc, i, htmlPath)); // Placeholder
                LOG_DEBUG("EpubDocument: Planned page " << i << " from manifest ID " << manifestId << ", path: " << htmlPath);
            } else {
                LOG_WARN("EpubDocument: Spine item ID '" << manifestId << "' not found in manifest!");
            }
        }
        LOG_INFO("EpubDocument: Created " << pages.size() << " page objects.");
    }
};

EpubDocument::EpubDocument(QObject* parent)
    : Document(parent)
    , d(new Private())
{
    LOG_INFO("EpubDocument created.");
}

EpubDocument::~EpubDocument()
{
    LOG_INFO("EpubDocument destroyed.");
}

bool EpubDocument::load(const QString& filePath, const QString& password)
{
    Q_UNUSED(password); // EPUBs typically don't use archive-level passwords like ZIPs often do

    // Close any previously loaded document
    if (d->zipArchive) {
        zip_close(d->zipArchive);
        d->zipArchive = nullptr;
    }
    d->isLoaded = false;
    d->pages.clear();

    // Open the EPUB file as a ZIP archive
    int zipError;
    d->zipArchive = zip_open(filePath.toUtf8().constData(), ZIP_RDONLY, &zipError);
    if (!d->zipArchive) {
        char errorBuffer[256];
        zip_error_to_str(errorBuffer, sizeof(errorBuffer), zipError, errno);
        setLastError(tr("Failed to open EPUB file as ZIP archive: %1").arg(errorBuffer));
        LOG_ERROR(lastError());
        return false;
    }

    // Set file path and update file size
    setFilePath(filePath);

    // 1. Parse container.xml to find package.opf
    if (!d->parseContainer()) {
        setLastError(tr("Failed to parse EPUB container.xml."));
        LOG_ERROR(lastError());
        return false;
    }

    // 2. Parse package.opf to get manifest, spine, metadata
    if (!d->parseOpf()) {
        setLastError(tr("Failed to parse EPUB package.opf."));
        LOG_ERROR(lastError());
        return false;
    }

    // 3. Parse navigation file (nav.xhtml or toc.ncx) to get TOC
    if (!d->parseNavigation()) {
        LOG_WARN("EpubDocument: Failed to parse navigation file, TOC might be incomplete.");
        // Don't fail the load entirely for a TOC parse error, just warn.
    }

    // 4. Create EpubPage objects based on the spine order
    d->createPages(this); // Pass 'this' pointer to allow pages to access the document's ZIP archive

    // Populate base Document metadata from EPUB metadata (if parsed from OPF)
    // This would involve reading <dc:title>, <dc:creator>, etc. from the OPF's <metadata> section.
    // For brevity in this example, we'll skip detailed metadata parsing here but it should be done.
    // setTitle(...);
    // setAuthor(...);
    // setSubject(...);
    // setKeywords(...);

    d->isLoaded = true;
    setState(Loaded);
    emit epubLoaded(); // Emit specific signal for EPUB loading
    LOG_INFO("Successfully loaded EPUB document: " << filePath << " (Pages: " << pageCount() << ", TOC items: " << d->toc.size() << ")");
    return true;
}

bool EpubDocument::save(const QString& filePath)
{
    // Saving EPUB requires reconstructing the ZIP archive.
    // This is complex and involves updating the OPF manifest/spine, potentially re-rendering HTML,
    // and re-zipping everything. This is a significant undertaking.
    // For Phase 4, we might focus on *reading* and *displaying* EPUBs correctly.
    // Saving modified EPUBs (e.g., after text annotations or metadata changes) would be a later enhancement.
    LOG_WARN("EpubDocument::save: Saving modified EPUBs is not yet implemented.");
    Q_UNUSED(filePath);
    setLastError(tr("Saving modified EPUBs is not yet supported."));
    return false;
}

Document::DocumentType EpubDocument::type() const
{
    return DocumentType::EPUB;
}

int EpubDocument::pageCount() const
{
    return d->pages.size();
}

Page* EpubDocument::page(int index) const
{
    if (index >= 0 && index < d->pages.size()) {
        // Return raw pointer managed by unique_ptr. The page needs access to the ZIP archive to render/fetch content.
        // This requires EpubPage to be implemented and correctly constructed with access to EpubDocument's Private members.
        // return d->pages[index].get(); // Placeholder - requires EpubPage implementation
        LOG_DEBUG("EpubDocument::page: Requested page " << index << ", but EpubPage not yet implemented.");
        return nullptr; // For now, return null until EpubPage is ready
    }
    return nullptr;
}

bool EpubDocument::isLocked() const
{
    // EPUB files are typically not locked/password-protected at the container level like some PDFs.
    return false;
}

bool EpubDocument::isEncrypted() const
{
    // EPUB files are not typically encrypted like PDFs.
    return false;
}

QString EpubDocument::formatVersion() const
{
    return d->formatVersion;
}

bool EpubDocument::supportsFeature(const QString& feature) const
{
    static const QSet<QString> supportedFeatures = {
        "TextSelection", "TextExtraction", "Hyperlinks", "Images", "TableOfContents", "Stylesheets" // Basic EPUB features
        // "AnnotationEditing" // Would require saving logic to be truly supported
    };
    return supportedFeatures.contains(feature);
}

// --- EPUB-Specific Getters ---
QString EpubDocument::uid() const
{
    return d->uid;
}

QString EpubDocument::packagePath() const
{
    return d->packagePath;
}

QString EpubDocument::navigationPath() const
{
    return d->navigationPath;
}

QMap<QString, QString> EpubDocument::manifestItems() const
{
    return d->manifest;
}

QStringList EpubDocument::spineOrder() const
{
    return d->spine;
}

QByteArray EpubDocument::getFileContent(const QString& filePath) const
{
    return d->readFileFromZip(filePath);
}

QStringList EpubDocument::embeddedFonts() const
{
    return d->embeddedFontsList;
}

bool EpubDocument::hasTableOfContents() const
{
    return !d->toc.isEmpty();
}

QVariantList EpubDocument::tableOfContents() const
{
    return d->toc;
}

QStringList EpubDocument::imagePaths() const
{
    return d->imagePathsList;
}

QList<QUrl> EpubDocument::hyperlinks() const
{
    return d->hyperlinksList;
}

// --- Helpers (defined in Private class) ---
// parseOpf, parseNavigation, etc. are already defined within the Private class above.

} // namespace QuantilyxDoc