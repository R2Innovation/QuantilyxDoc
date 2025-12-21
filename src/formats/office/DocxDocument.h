/**
 * QuantilyxDoc - Professional Document Editor
 * Copyright (C) 2025 R² Innovative Software
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#ifndef QUANTILYX_DOCXDOCUMENT_H
#define QUANTILYX_DOCXDOCUMENT_H

#include "../../core/Document.h"
#include <memory>
#include <QList>
#include <QMap>

namespace QuantilyxDoc {

class DocxPage; // Forward declaration

/**
 * @brief DOCX (Office Open XML) document implementation.
 * 
 * Handles loading and parsing of DOCX files (Microsoft Word documents).
 * DOCX is a ZIP archive containing XML files.
 */
class DocxDocument : public Document
{
    Q_OBJECT

public:
    explicit DocxDocument(QObject* parent = nullptr);
    ~DocxDocument() override;

    // --- Document Interface Implementation ---
    bool load(const QString& filePath, const QString& password = QString()) override;
    bool save(const QString& filePath = QString()) override;
    DocumentType type() const override;
    int pageCount() const override; // Page count might be estimated from content
    Page* page(int index) const override;
    bool isLocked() const override;
    bool isEncrypted() const override;
    QString formatVersion() const override; // e.g., "WordprocessingML 2006"
    bool supportsFeature(const QString& feature) const override;

    // --- DOCX-Specific Metadata ---
    QString documentTitle() const;
    QString documentAuthor() const;
    QList<QString> documentKeywords() const;
    QStringList styleNames() const;
    QList<QString> embeddedObjectNames() const;
    bool hasTrackChanges() const;

signals:
    void docxLoaded();

private:
    class Private;
    std::unique_ptr<Private> d;

    void createPages();
};

} // namespace QuantilyxDoc

#endif // QUANTILYX_DOCXDOCUMENT_H